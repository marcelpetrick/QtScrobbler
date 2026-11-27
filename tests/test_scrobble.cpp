// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2026 Marcel Petrick

#include <QCoreApplication>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtTest>
#include <memory>

#include "libscrobble.h"
#include "test_environment.h"

namespace {
QStringList warnings;
QMutex warningMutex;
QtMessageHandler previousMessageHandler = nullptr;

class LocalService : public QObject {
   public:
    QTcpServer server;
    QByteArray submissionReply = "OK\n";
    QByteArray handshakeReply;
    bool holdHandshake = false;
    int submissionDelay = 0;
    QList<QByteArray> requests;
    QList<QPointer<QTcpSocket>> connections;

    LocalService() {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (auto* socket = server.nextPendingConnection()) {
                connections.append(socket);
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    QByteArray data = socket->property("request").toByteArray() + socket->readAll();
                    socket->setProperty("request", data);
                    if (socket->property("handled").toBool()) return;
                    const int headerEnd = data.indexOf("\r\n\r\n");
                    if (headerEnd < 0) return;
                    int length = 0;
                    for (const QByteArray& header : data.left(headerEnd).split('\n')) {
                        if (header.trimmed().toLower().startsWith("content-length:"))
                            length = header.mid(header.indexOf(':') + 1).trimmed().toInt();
                    }
                    if (data.size() < headerEnd + 4 + length) return;
                    socket->setProperty("handled", true);
                    requests.append(data);
                    const bool handshake = data.startsWith("GET ");
                    if (handshake && holdHandshake) return;
                    QByteArray body = submissionReply;
                    if (handshake) {
                        body = handshakeReply.isEmpty()
                                   ? "OK\nsession\n" + url().toUtf8() + "/now\n" + url().toUtf8() + "/submit\n"
                                   : handshakeReply;
                    }
                    QTimer::singleShot(handshake ? 0 : submissionDelay, socket, [socket, body] {
                        socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) +
                                      "\r\nConnection: close\r\n\r\n" + body);
                        socket->disconnectFromHost();
                    });
                });
            }
        });
    }

    bool listen() { return server.listen(QHostAddress::LocalHost); }
    QString url() const { return "http://127.0.0.1:" + QString::number(server.serverPort()); }

    void configure(Scrobble& scrobble, int site = CONFIG_LASTFM) const {
        scrobble.conf->sites[site].enabled = true;
        scrobble.conf->sites[site].username = "fictional-user";
        scrobble.conf->sites[site].password_hash = "fictional-hash";
        scrobble.conf->sites[site].handshake_host = "127.0.0.1:" + QString::number(server.serverPort());
        scrobble.conf->sites[site].conf_name = "Loopback service";
    }
};

void captureMessages(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    if (type == QtWarningMsg || type == QtCriticalMsg) {
        QMutexLocker locker(&warningMutex);
        warnings.append(message);
    }
    if (previousMessageHandler) previousMessageHandler(type, context, message);
}

struct Track {
    QString artist;
    QString title;
    int length;
    int timestamp;
    QChar played = 'L';
};

bool writeTracks(const QString& directory, const QList<Track>& tracks) {
    QFile file(directory + "/.scrobbler.log");
    if (!file.open(QIODevice::WriteOnly)) return false;
    QByteArray contents("#AUDIOSCROBBLER/1.0\n#TZ/UTC\n#CLIENT/contracts\n");
    for (const Track& track : tracks) {
        contents += QString("%1\tAlbum\t%2\t1\t%3\t%4\t%5\n")
                        .arg(track.artist, track.title)
                        .arg(track.length)
                        .arg(track.played)
                        .arg(track.timestamp)
                        .toUtf8();
    }
    if (file.write(contents) != contents.size()) return false;
    file.close();
    return true;
}

bool loadTracks(Scrobble& scrobble, const QString& directory, const QList<Track>& tracks) {
    if (!writeTracks(directory, tracks)) return false;
    if (!scrobble.parse(SCROBBLE_LOG, directory)) return false;
    return true;
}
}  // namespace

class ScrobbleTest : public QObject {
    Q_OBJECT

   private slots:
    void init() {
        const QString config = qEnvironmentVariable("XDG_CONFIG_HOME");
        QVERIFY(!config.isEmpty());
        QVERIFY(QDir(config).removeRecursively());
        QVERIFY(QDir().mkpath(config));
        warnings.clear();
        previousMessageHandler = qInstallMessageHandler(captureMessages);
    }

    void cleanup() {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        qInstallMessageHandler(previousMessageHandler);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        QVERIFY(QSqlDatabase::connectionNames().isEmpty());
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    }

    void initialStateAndTimezone() {
        const auto before = QDateTime::currentSecsSinceEpoch();
        Scrobble scrobble;
        const auto after = QDateTime::currentSecsSinceEpoch();
        QVERIFY(scrobble.get_gmt() >= before);
        QVERIFY(scrobble.get_gmt() <= after + 1);
        QCOMPARE(scrobble.get_num_tracks(), 0);
        QVERIFY(!scrobble.get_parser_busy());
        scrobble.clear_method();
        QVERIFY(!scrobble.get_parser_busy());
        QVERIFY(scrobble.get_error_str().isEmpty());
        QVERIFY(!scrobble.check_age());
        QCOMPARE(scrobble.get_gmt_offset(), 0);
        QCOMPARE(scrobble.get_custom_offset(), 0);
        QCOMPARE(scrobble.offset_str(), "UTC +00:00");
        QCOMPARE(scrobble.get_dst(), 0);
        QCOMPARE(scrobble.get_zonename(), "UTC");
        scrobble.conf->tz_override = true;
        scrobble.conf->utc_offset = -19800;
        QCOMPARE(scrobble.get_custom_offset(), -19800);
        scrobble.conf->tz_override = false;
        QCOMPARE(scrobble.get_custom_offset(), 0);
        QVERIFY(scrobble.get_num_logs() >= 4);
        scrobble.clear_log();
        QCOMPARE(scrobble.get_num_logs(), 0);
        scrobble.set_log_level(LOG_TRACE);
        scrobble.parser_clear();
        QCOMPARE(scrobble.get_num_logs(), 1);
        QCOMPARE(scrobble.get_log(0).level, LOG_ERROR);
        QCOMPARE(scrobble.get_log(0).msg, "Parser: asked to clear without a live object");
        QCOMPARE(scrobble.get_error_str(), scrobble.get_log(0).msg);
    }

    void activeTimezone_data() {
        QTest::addColumn<QByteArray>("zone");
        QTest::newRow("utc") << QByteArray("UTC");
        QTest::newRow("europe") << QByteArray("Europe/Berlin");
        QTest::newRow("america") << QByteArray("America/New_York");
        QTest::newRow("quarter-hour-offset") << QByteArray("Asia/Kathmandu");
        QTest::newRow("southern-hemisphere-dst") << QByteArray("Australia/Sydney");
    }

    void activeTimezone() {
        QFETCH(QByteArray, zone);
        QProcess child;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("QTSCROBBLER_TEST_TIMEZONE", QString::fromLatin1(zone));
        child.setProcessEnvironment(environment);
        child.start(QCoreApplication::applicationFilePath(), {"--timezone-probe"});
        QVERIFY(child.waitForStarted(1000));
        QVERIFY(child.waitForFinished(3000));
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        const QByteArray output = child.readAllStandardOutput();
        QVERIFY2(child.exitCode() == 0, output.constData());
        QCOMPARE(child.readAllStandardError(), QByteArray());
        // The default print level is INFO, so the constructor's DEBUG logs stay off stdout.
        QVERIFY2(!output.contains("DEBUG:"), output.constData());
    }

    void emptySubmission() {
        Scrobble scrobble;
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(!scrobble.submit());
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(0).toBool(), false);
        QCOMPARE(scrobble.get_error_str(), "Nothing to submit!");
        QVERIFY(scrobble.submissions.isEmpty());
    }

    void parseSortEditRemoveAndRecalculate() {
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy parsed(&scrobble, &Scrobble::parsing_opened);
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Later", "C", 40, 300}, {"Earlier", "A", 60, 100}, {"Middle", "B", 80, 200}}));
        QCOMPARE(parsed.count(), 1);
        QVERIFY(parsed.first().at(0).toBool());
        const auto openedLog = scrobble.get_log(scrobble.get_num_logs() - 1);
        QCOMPARE(openedLog.level, LOG_INFO);
        QCOMPARE(openedLog.msg, "Successfully parsed 3 log entries");
        QCOMPARE(scrobble.get_num_tracks(), 3);
        QCOMPARE(scrobble.get_track(0).title, "A");
        QCOMPARE(scrobble.get_track(1).title, "B");
        QCOMPARE(scrobble.get_track(2).title, "C");
        QVERIFY(scrobble.get_parser_busy());
        auto edited = scrobble.get_track(1);
        edited.artist = "Edited";
        edited.length = 100;
        scrobble.update_track(edited, 1);
        QCOMPARE(scrobble.get_track(1).artist, "Edited");
        QCOMPARE(scrobble.get_track(1).length, 100);
        scrobble.recalc_dt(1000);
        QCOMPARE(scrobble.get_track(2).when, 960);
        QCOMPARE(scrobble.get_track(1).when, 860);
        QCOMPARE(scrobble.get_track(0).when, 800);
        scrobble.recalc_dt(500, 1);
        QCOMPARE(scrobble.get_track(2).when, 960);
        QCOMPARE(scrobble.get_track(1).when, 400);
        QCOMPARE(scrobble.get_track(0).when, 340);
        const uint before = scrobble.get_gmt();
        scrobble.recalc_now();
        const uint after = scrobble.get_gmt();
        QVERIFY(scrobble.get_track(2).when >= static_cast<int>(before) - 40);
        QVERIFY(scrobble.get_track(2).when <= static_cast<int>(after) - 40);
        QCOMPARE(scrobble.get_track(0).when, scrobble.get_track(1).when - 60);
        scrobble.remove_track(1);
        QCOMPARE(scrobble.get_num_tracks(), 2);
        QCOMPARE(scrobble.get_track(1).title, "C");
        QSignalSpy cleared(&scrobble, &Scrobble::parsing_cleared);
        scrobble.parser_clear();
        QCOMPARE(cleared.count(), 1);
        QVERIFY(cleared.first().at(0).toBool());
        const auto clearedLog = scrobble.get_log(scrobble.get_num_logs() - 1);
        QCOMPARE(clearedLog.level, LOG_INFO);
        QCOMPARE(clearedLog.msg, "Deleted log file " + directory.filePath(".scrobbler.log"));
        QVERIFY(!scrobble.get_parser_busy());
        QVERIFY(!QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void parseErrorRecovery() {
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy parsed(&scrobble, &Scrobble::parsing_opened);
        QVERIFY(!scrobble.parse(SCROBBLE_NONE, directory.path()));
        QCOMPARE(parsed.count(), 0);
        QCOMPARE(scrobble.get_error_str(), "Error - asked to use an unknown Parser!");
        QVERIFY(scrobble.parse(SCROBBLE_LOG, directory.path()));
        QCOMPARE(parsed.count(), 1);
        QCOMPARE(parsed.first().at(0).toBool(), false);
        const auto failedLog = scrobble.get_log(scrobble.get_num_logs() - 1);
        QCOMPARE(failedLog.level, LOG_ERROR);
        QCOMPARE(failedLog.msg, "Parsing failed - could not find a logfile at " + directory.path() + " to open");
        QVERIFY(!scrobble.get_parser_busy());
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Recovered", 180, 1700000000}}));
        QCOMPARE(parsed.count(), 2);
        QVERIFY(parsed.last().at(0).toBool());
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QVERIFY(!scrobble.parse(SCROBBLE_LOG, directory.path()));
        QCOMPARE(scrobble.get_error_str(), "Asked to create a duplicate parser");
        scrobble.parser_clear();
        QVERIFY(scrobble.parse(SCROBBLE_IPOD, directory.path()));
        QCOMPARE(parsed.count(), 3);
        QCOMPARE(parsed.last().at(0).toBool(), false);
        QVERIFY(!scrobble.get_parser_busy());
        scrobble.clear_method();
        scrobble.parse_open(directory.path(), 0);  // No live parser: no event or crash.
        QCOMPARE(parsed.count(), 3);
    }

    void clearFailureRetainsTracks() {
        Scrobble scrobble;
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Retain", 180, 1700000000}}));
        QVERIFY(QFile::remove(directory.filePath(".scrobbler.log")));
        QSignalSpy cleared(&scrobble, &Scrobble::parsing_cleared);
        scrobble.parser_clear();
        QCOMPARE(cleared.count(), 1);
        QCOMPARE(cleared.first().at(0).toBool(), false);
        QVERIFY(!scrobble.get_parser_busy());
        QCOMPARE(scrobble.get_num_tracks(), 1);
        const auto failedLog = scrobble.get_log(scrobble.get_num_logs() - 1);
        QCOMPARE(failedLog.level, LOG_ERROR);
        QCOMPARE(failedLog.msg, "Failed to remove log file " + directory.filePath(".scrobbler.log"));
        QCOMPARE(scrobble.get_error_str(), failedLog.msg);
    }

    void deletingAllTracksAllowsNewImport() {
        Scrobble scrobble;
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "First", 180, 1700000000}}));
        scrobble.remove_track(0);
        scrobble.clear_method();
        QVERIFY(!scrobble.get_parser_busy());
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Second", 240, 1700000100}}));
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QCOMPARE(scrobble.get_track(0).title, "Second");
    }

    void reopenLiveParserReplacesTracks() {
        Scrobble scrobble;
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "First", 180, 1700000000}}));
        QVERIFY(writeTracks(directory.path(), {{"Artist", "Second", 240, 1700000100}}));
        QSignalSpy parsed(&scrobble, &Scrobble::parsing_opened);
        scrobble.parse_open(directory.path(), 0);
        QCOMPARE(parsed.count(), 1);
        QVERIFY(parsed.first().at(0).toBool());
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QCOMPARE(scrobble.get_track(0).title, "Second");
    }

    void disabledSitesRetainUnsentTracks() {
        Scrobble scrobble;
        QTemporaryDir directory;
        const int recent = static_cast<int>(scrobble.get_gmt()) - 60;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Keep", 180, recent},
                            {"Artist", "Short", 29, recent},
                            {"Artist", "Skipped", 240, recent, 'S'}}));
        QCOMPARE(scrobble.get_track(1).played, QChar('S'));
        auto keep = scrobble.get_track(0);
        keep.sent[CONFIG_LASTFM] = SENT_SUCCESS;
        keep.sent[CONFIG_LIBREFM] = SENT_FAILED;
        scrobble.update_track(keep, 0);
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(!scrobble.submit());
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(0).toBool(), false);
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QCOMPARE(scrobble.get_track(0).title, "Keep");
        QCOMPARE(scrobble.get_track(0).sent[CONFIG_LASTFM], SENT_SUCCESS);
        QCOMPARE(scrobble.get_track(0).sent[CONFIG_LIBREFM], SENT_UNSENT);
        QCOMPARE(scrobble.get_error_str(), "No sites were configured and/or enabled. Unable to submit");
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void tooOldSubmission() {
        Scrobble scrobble;
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Ancient", 180, 1}}));
        scrobble.clear_log();
        QVERIFY(scrobble.check_age());
        QCOMPARE(scrobble.get_num_logs(), 1);
        QCOMPARE(scrobble.get_log(0).level, LOG_INFO);
        QCOMPARE(scrobble.get_log(0).msg, "Track too old: Artist Ancient");
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(!scrobble.submit());
        QCOMPARE(finished.count(), 1);
        QCOMPARE(scrobble.get_error_str(), "One or more tracks are too old.  Correct this and try again.");
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QVERIFY(scrobble.get_parser_busy());
    }

    void allFilteredTracksFailImmediately() {
        LocalService service;
        QVERIFY(service.listen());
        Scrobble scrobble;
        service.configure(scrobble);
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Skipped", 180, static_cast<int>(scrobble.get_gmt()) - 60, 'S'}}));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(!scrobble.submit());
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(0).toBool(), false);
        QCOMPARE(scrobble.get_error_str(), "Nothing to submit!");
        QCOMPARE(scrobble.get_num_tracks(), 0);
        QVERIFY(scrobble.submissions.isEmpty());
        QVERIFY(service.requests.isEmpty());
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void incompleteSiteDoesNotSubmit_data() {
        QTest::addColumn<QString>("field");
        for (const QString& field :
             {QString("enabled"), QString("username"), QString("password"), QString("host"), QString("name")})
            QTest::newRow(qPrintable(field)) << field;
    }

    void incompleteSiteDoesNotSubmit() {
        QFETCH(QString, field);
        LocalService service;
        QVERIFY(service.listen());
        Scrobble scrobble;
        service.configure(scrobble);
        auto& site = scrobble.conf->sites[CONFIG_LASTFM];
        if (field == "enabled")
            site.enabled = false;
        else if (field == "username")
            site.username.clear();
        else if (field == "password")
            site.password_hash.clear();
        else if (field == "host")
            site.handshake_host.clear();
        else
            site.conf_name.clear();
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Keep", 180, static_cast<int>(scrobble.get_gmt()) - 60}}));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(!scrobble.submit());
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(0).toBool(), false);
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QVERIFY(scrobble.submissions.isEmpty());
        QVERIFY(service.requests.isEmpty());
        QCOMPARE(scrobble.get_error_str(), "No sites were configured and/or enabled. Unable to submit");
    }

    void metadataFollowsTrackWhenParsingSortsRows() {
        LocalService service;
        QVERIFY(service.listen());
        service.submissionReply = "<lfm status=\"ok\"><track><duration>235000</duration></track></lfm>";
        QNetworkProxy::setApplicationProxy(
            QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", service.server.serverPort()));
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy finished(&scrobble, &Scrobble::missing_times_finished);
        QVERIFY(loadTracks(
            scrobble, directory.path(),
            {{"Later artist", "Missing later", 0, 1700000100}, {"Earlier artist", "Known earlier", 180, 1700000000}}));
        QCOMPARE(scrobble.get_track(0).title, "Known earlier");
        QCOMPARE(scrobble.get_track(1).title, "Missing later");
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(scrobble.get_track(0).length, 180);
        QCOMPARE(scrobble.get_track(1).length, 235);
        DBCache cache;
        QVERIFY(cache.init());
        QCOMPARE(cache.get_track_length("Later artist", "Missing later"), 235);
        QCOMPARE(cache.get_track_length("Earlier artist", "Known earlier"), -1);
    }

    void removingTrackRestartsPendingMetadata() {
        LocalService service;
        QVERIFY(service.listen());
        service.submissionDelay = 50;
        service.submissionReply = "<lfm status=\"ok\"><track><duration>235000</duration></track></lfm>";
        QNetworkProxy::setApplicationProxy(
            QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", service.server.serverPort()));
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy finished(&scrobble, &Scrobble::missing_times_finished);
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Known artist", "Remove", 180, 1700000000}, {"Missing artist", "Keep", 0, 1700000100}}));
        QTRY_COMPARE_WITH_TIMEOUT(service.requests.size(), 1, 1000);
        scrobble.remove_track(0);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QCOMPARE(scrobble.get_track(0).title, "Keep");
        QCOMPARE(scrobble.get_track(0).length, 235);
        QCOMPARE(service.requests.size(), 2);
        DBCache cache;
        QVERIFY(cache.init());
        QCOMPARE(cache.get_track_length("Missing artist", "Keep"), 235);
        QCOMPARE(cache.get_track_length("Known artist", "Remove"), -1);
    }

    void editingTrackIdentityRestartsPendingMetadata() {
        LocalService service;
        QVERIFY(service.listen());
        service.submissionDelay = 50;
        service.submissionReply = "<lfm status=\"ok\"><track><duration>100000</duration></track></lfm>";
        QNetworkProxy::setApplicationProxy(
            QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", service.server.serverPort()));
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy finished(&scrobble, &Scrobble::missing_times_finished);
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Old title", 0, 1700000000}}));
        QTRY_COMPARE_WITH_TIMEOUT(service.requests.size(), 1, 1000);
        service.submissionReply = "<lfm status=\"ok\"><track><duration>235000</duration></track></lfm>";
        auto edited = scrobble.get_track(0);
        edited.title = "New title";
        scrobble.update_track(edited, 0);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(scrobble.get_track(0).title, "New title");
        QCOMPARE(scrobble.get_track(0).length, 235);
        QCOMPARE(service.requests.size(), 2);
        QVERIFY(service.requests.last().contains("track=New%20title"));
        DBCache cache;
        QVERIFY(cache.init());
        QCOMPARE(cache.get_track_length("Artist", "New title"), 235);
        QCOMPARE(cache.get_track_length("Artist", "Old title"), -1);
    }

    void cancellingFromMetadataSignalsIsSafe_data() {
        QTest::addColumn<bool>("onStart");
        QTest::newRow("cancel-start") << true;
        QTest::newRow("cancel-progress") << false;
    }

    void cancellingFromMetadataSignalsIsSafe() {
        QFETCH(bool, onStart);
        LocalService service;
        QVERIFY(service.listen());
        service.submissionReply = "<lfm status=\"ok\"><track><duration>235000</duration></track></lfm>";
        QNetworkProxy::setApplicationProxy(
            QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", service.server.serverPort()));
        Scrobble scrobble;
        if (onStart)
            connect(&scrobble, &Scrobble::missing_times_start, &scrobble,
                    [&scrobble](int) { scrobble.cancel_submission(); });
        else
            connect(&scrobble, &Scrobble::missing_times_progress, &scrobble,
                    [&scrobble](int) { scrobble.cancel_submission(); });
        QTemporaryDir directory;
        QSignalSpy cancelled(&scrobble, &Scrobble::submission_finished);
        QSignalSpy finished(&scrobble, &Scrobble::missing_times_finished);
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "First", 0, 1700000000}, {"Artist", "Second", 0, 1700000100}}));
        QTRY_COMPARE_WITH_TIMEOUT(cancelled.count(), 1, 2000);
        QCOMPARE(finished.count(), 0);
        QCOMPARE(service.requests.size(), onStart ? 0 : 1);
        QCOMPARE(scrobble.get_num_tracks(), 2);
        QVERIFY(!scrobble.get_parser_busy());
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void submissionWaitsForMetadata() {
        LocalService service;
        QVERIFY(service.listen());
        service.submissionDelay = 50;
        service.submissionReply = "<lfm status=\"ok\"><track><duration>235000</duration></track></lfm>";
        QNetworkProxy::setApplicationProxy(
            QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", service.server.serverPort()));
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy finished(&scrobble, &Scrobble::missing_times_finished);
        QSignalSpy submitted(&scrobble, &Scrobble::submission_finished);
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Keep", 0, 1700000000}}));
        QVERIFY(!scrobble.submit());
        QCOMPARE(submitted.count(), 1);
        QCOMPARE(scrobble.get_error_str(), "Track lengths are still being fetched. Please wait before submitting.");
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QVERIFY(scrobble.submissions.isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(service.requests.size(), 1);
        QCOMPARE(scrobble.get_track(0).length, 235);
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void missingDurationPrecedesTimelessRecalculation() {
        LocalService service;
        QVERIFY(service.listen());
        service.submissionReply = "<lfm status=\"ok\"><track><duration>235000</duration></track></lfm>";
        QNetworkProxy::setApplicationProxy(
            QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", service.server.serverPort()));
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy finished(&scrobble, &Scrobble::missing_times_finished);
        const int before = scrobble.get_gmt();
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "First", 0, 0}, {"Artist", "Second", 180, 0}}));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        const int after = scrobble.get_gmt();
        QCOMPARE(scrobble.get_track(0).length, 235);
        QCOMPARE(scrobble.get_track(0).when + 235, scrobble.get_track(1).when);
        QVERIFY(scrobble.get_track(1).when + 180 >= before);
        QVERIFY(scrobble.get_track(1).when + 180 <= after);
    }

    void cachedMissingDuration() {
        {
            DBCache cache;
            QVERIFY(cache.init());
            cache.set_track_length("Artist", "Cached", 235);
        }
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy missing(&scrobble, &Scrobble::missing_times_start);
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Cached", 0, 1700000000}}));
        QCOMPARE(missing.count(), 0);
        QCOMPARE(scrobble.get_track(0).length, 235);
    }

    void missingDurationsUseLoopbackProxy_data() {
        QTest::addColumn<int>("delay");
        QTest::newRow("fast-response-needs-rate-delay") << 0;
        QTest::newRow("slow-response-needs-no-extra-delay") << 220;
    }

    void missingDurationsUseLoopbackProxy() {
        QFETCH(int, delay);
        LocalService service;
        QVERIFY(service.listen());
        service.submissionDelay = delay;
        service.submissionReply = "<lfm status=\"ok\"><track><duration>235000</duration></track></lfm>";
        // Route the real metadata manager exclusively to this local HTTP proxy.
        // The public service hostname is never resolved or contacted.
        QNetworkProxy::setApplicationProxy(
            QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", service.server.serverPort()));
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy started(&scrobble, &Scrobble::missing_times_start);
        QSignalSpy requested(&scrobble, &Scrobble::missing_times_get);
        QSignalSpy progress(&scrobble, &Scrobble::missing_times_progress);
        QSignalSpy finished(&scrobble, &Scrobble::missing_times_finished);
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Missing one", 0, 1700000000}, {"Artist", "Missing two", 0, 1700000010}}));
        QCOMPARE(started.count(), 1);
        QCOMPARE(started.first().at(0).toInt(), 2);
        const auto fetchingLog = scrobble.get_log(scrobble.get_num_logs() - 2);
        QCOMPARE(fetchingLog.level, LOG_INFO);
        QCOMPARE(fetchingLog.msg, "Fetching missing track lengths from last.fm...");
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        QCOMPARE(requested.count(), 2);
        QCOMPARE(requested[0][0].toInt(), 0);
        QCOMPARE(requested[0][2].toInt(), 0);
        QCOMPARE(requested[1][0].toInt(), 1);
        const QRegularExpression timing(R"(^API: 0 is 235, took (\d+(?:\.\d+)?)$)");
        QRegularExpressionMatch firstResponse;
        for (int i = 0; i < scrobble.get_num_logs(); ++i) {
            firstResponse = timing.match(scrobble.get_log(i).msg);
            if (firstResponse.hasMatch()) break;
        }
        QVERIFY(firstResponse.hasMatch());
        const int elapsed = qRound(firstResponse.captured(1).toDouble() * 1000);
        QCOMPARE(requested[1][2].toInt(), qMax(0, 200 - elapsed));
        QCOMPARE(progress.count(), 1);
        QCOMPARE(progress.first().at(0).toInt(), 1);
        QCOMPARE(service.requests.size(), 2);
        QCOMPARE(scrobble.get_track(0).length, 235);
        QCOMPARE(scrobble.get_track(1).length, 235);
        QVERIFY(service.requests[0].contains("method=track.getinfo"));
        QVERIFY(service.requests[0].contains("artist=Artist"));
        DBCache cache;
        QVERIFY(cache.init());
        QCOMPARE(cache.get_track_length("Artist", "Missing one"), 235);
        QCOMPARE(cache.get_track_length("Artist", "Missing two"), 235);
    }

    void failedDurationLookupLeavesTrackUntouched() {
        LocalService service;
        QVERIFY(service.listen());
        service.submissionReply = "<lfm status=\"failed\"><error code=\"6\">Not found</error></lfm>";
        QNetworkProxy::setApplicationProxy(
            QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", service.server.serverPort()));
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy finished(&scrobble, &Scrobble::missing_times_finished);
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Unknown", 0, 1700000000}}));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(scrobble.get_track(0).length, 0);
        QCOMPARE(scrobble.get_track(0).title, "Unknown");
        DBCache cache;
        QVERIFY(cache.init());
        QCOMPARE(cache.get_track_length("Artist", "Unknown"), -1);
    }

    void cancelMetadataBeforeNewImport() {
        LocalService service;
        QVERIFY(service.listen());
        service.submissionDelay = 1000;
        service.submissionReply = "<lfm status=\"ok\"><track><duration>235000</duration></track></lfm>";
        QNetworkProxy::setApplicationProxy(
            QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", service.server.serverPort()));
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy metadataFinished(&scrobble, &Scrobble::missing_times_finished);
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Old generation", 0, 1700000000}}));
        QTRY_COMPARE_WITH_TIMEOUT(service.requests.size(), 1, 2000);
        QCOMPARE(service.connections.size(), 1);
        scrobble.cancel_submission();
        QTRY_VERIFY_WITH_TIMEOUT(service.connections.first().isNull(), 2000);
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "New generation", 180, 1700000100}}));
        QCOMPARE(scrobble.get_track(0).title, "New generation");
        QCOMPARE(scrobble.get_track(0).length, 180);
        QCOMPARE(metadataFinished.count(), 0);
        QCOMPARE(service.requests.size(), 1);
    }

    void allTimelessTracks() {
        Scrobble scrobble;
        QTemporaryDir directory;
        const int before = static_cast<int>(scrobble.get_gmt());
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "First", 60, 0}, {"Artist", "Last", 180, 0}}));
        const int after = static_cast<int>(scrobble.get_gmt());
        const auto first = scrobble.get_track(0);
        const auto last = scrobble.get_track(1);
        QCOMPARE(first.when + first.length, last.when);
        QVERIFY(last.when + last.length >= before);
        QVERIFY(last.when + last.length <= after);
    }

    void mixedTimestampsPreserveKnownTime() {
        Scrobble scrobble;
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Missing one", 60, 0},
                            {"Artist", "Missing two", 120, 0},
                            {"Artist", "Known", 180, 1700000000}}));
        QCOMPARE(scrobble.get_track(2).when, 1700000000);
        QCOMPARE(scrobble.get_track(1).when, 1699999880);
        QCOMPARE(scrobble.get_track(0).when, 1699999820);
    }

    void cancelWithoutParser() {
        Scrobble scrobble;
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        scrobble.cancel_submission();
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(0).toBool(), false);
        QVERIFY(!scrobble.get_parser_busy());
    }

    void cancelWithoutSitesRetainsTracks() {
        Scrobble scrobble;
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Keep", 180, 1700000000}}));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        scrobble.cancel_submission();
        QCOMPARE(finished.count(), 1);
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QCOMPARE(scrobble.get_track(0).title, "Keep");
        QVERIFY(!scrobble.get_parser_busy());
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
        scrobble.cancel_submission();  // Repeated cancellation is harmless.
        QCOMPARE(finished.count(), 2);
        QCOMPARE(scrobble.get_num_tracks(), 1);
    }

    void ageLimitIsThirtyDays() {
        Scrobble scrobble;
        QTemporaryDir directory;
        const int now = static_cast<int>(scrobble.get_gmt());
        const int day = 24 * 60 * 60;
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Recent", 180, now - 29 * day}}));
        scrobble.clear_log();
        QVERIFY(!scrobble.check_age());
        QCOMPARE(scrobble.get_num_logs(), 0);
        Scrobble older;
        QTemporaryDir olderDirectory;
        QVERIFY(loadTracks(older, olderDirectory.path(), {{"Artist", "Older", 180, now - 31 * day}}));
        older.clear_log();
        QVERIFY(older.check_age());
        QCOMPARE(older.get_num_logs(), 1);
        QCOMPARE(older.get_log(0).msg, "Track too old: Artist Older");
    }

    void submissionReportsEarlierErrors() {
        LocalService service;
        QVERIFY(service.listen());
        Scrobble scrobble;
        service.configure(scrobble);
        scrobble.parser_clear();  // Records an error before the import.
        QCOMPARE(scrobble.get_error_str(), "Parser: asked to clear without a live object");
        QTemporaryDir directory;
        const int recent = static_cast<int>(scrobble.get_gmt()) - 60;
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Sent", 180, recent}}));
        scrobble.clear_log();
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(scrobble.submit());
        QStringList messages;
        for (int i = 0; i < scrobble.get_num_logs(); ++i) messages.append(scrobble.get_log(i).msg);
        QVERIFY2(messages.contains("Scrobble::submit() but with the following errors:"
                                   "Parser: asked to clear without a live object"),
                 qPrintable(messages.join('\n')));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
    }

    void thirtySecondTracksSubmitWhileShortAndSkippedTracksAreFiltered() {
        LocalService service;
        QVERIFY(service.listen());
        Scrobble scrobble;
        service.configure(scrobble);
        QTemporaryDir directory;
        const int recent = static_cast<int>(scrobble.get_gmt()) - 60;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Too short", 29, recent},
                            {"Artist", "Exactly thirty", 30, recent + 1},
                            {"Artist", "Over thirty", 31, recent + 2},
                            {"Artist", "Skipped thirty", 30, recent + 3, 'S'}}));
        QCOMPARE(scrobble.get_track(0).played, QChar('S'));
        QCOMPARE(scrobble.get_track(1).played, QChar('L'));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(scrobble.submit());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(finished.first()[0].toBool(), true);
        QCOMPARE(service.requests.size(), 2);
        const QByteArray body = service.requests[1].mid(service.requests[1].indexOf("\r\n\r\n") + 4);
        const QUrlQuery fields(QString::fromUtf8(body));
        QCOMPARE(fields.queryItemValue("t[0]", QUrl::FullyDecoded), "Exactly thirty");
        QCOMPARE(fields.queryItemValue("l[0]"), "30");
        QCOMPARE(fields.queryItemValue("t[1]", QUrl::FullyDecoded), "Over thirty");
        QCOMPARE(fields.queryItemValue("l[1]"), "31");
        QVERIFY(!fields.hasQueryItem("t[2]"));
        QCOMPARE(scrobble.get_num_tracks(), 0);
        QVERIFY(!scrobble.get_parser_busy());
        QVERIFY(!QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void retryAfterFailureClearsSource_data() {
        QTest::addColumn<QString>("failure");
        QTest::newRow("submission-error") << "submission";
        QTest::newRow("authentication-error") << "authentication";
        QTest::newRow("cancel-handshake") << "cancel";
    }

    void retryAfterFailureClearsSource() {
        QFETCH(QString, failure);
        LocalService service;
        QVERIFY(service.listen());
        if (failure == "submission") service.submissionReply = "FAILED try again\n";
        if (failure == "authentication") service.handshakeReply = "BADAUTH\n";
        service.holdHandshake = failure == "cancel";
        Scrobble scrobble;
        service.configure(scrobble);
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Retry", 180, static_cast<int>(scrobble.get_gmt()) - 60}}));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(scrobble.submit());
        if (failure == "cancel") {
            QTRY_COMPARE_WITH_TIMEOUT(service.requests.size(), 1, 2000);
            scrobble.cancel_submission();
        }
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(finished.first()[0].toBool(), false);
        QVERIFY(!scrobble.get_parser_busy());
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
        const int previousRequests = service.requests.size();
        service.submissionReply = "OK\n";
        service.handshakeReply.clear();
        service.holdHandshake = false;
        QVERIFY(scrobble.submit());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 2000);
        QCOMPARE(finished.last()[0].toBool(), true);
        QCOMPARE(service.requests.size(), previousRequests + 2);
        QVERIFY(service.requests.last().contains("t[0]=Retry"));
        QCOMPARE(scrobble.get_num_tracks(), 0);
        QVERIFY(!scrobble.get_parser_busy());
        QVERIFY(!QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void mixedSiteRetryDoesNotResubmitSuccessfulSite_data() {
        QTest::addColumn<bool>("disableFailedSite");
        QTest::newRow("retry-failed-site") << false;
        QTest::newRow("disable-failed-site") << true;
    }

    void mixedSiteRetryDoesNotResubmitSuccessfulSite() {
        QFETCH(bool, disableFailedSite);
        LocalService failing;
        LocalService successful;
        QVERIFY(failing.listen());
        QVERIFY(successful.listen());
        failing.submissionReply = "FAILED try again\n";
        successful.submissionDelay = 50;
        Scrobble scrobble;
        failing.configure(scrobble, CONFIG_LASTFM);
        successful.configure(scrobble, CONFIG_LIBREFM);
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Retry only failed site", 180, static_cast<int>(scrobble.get_gmt()) - 60}}));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(scrobble.submit());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(finished.first()[0].toBool(), false);
        QCOMPARE(scrobble.get_track(0).sent[CONFIG_LASTFM], SENT_FAILED);
        QCOMPARE(scrobble.get_track(0).sent[CONFIG_LIBREFM], SENT_SUCCESS);
        QCOMPARE(failing.requests.size(), 2);
        QCOMPARE(successful.requests.size(), 2);
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
        failing.submissionReply = "OK\n";
        if (disableFailedSite) scrobble.conf->sites[CONFIG_LASTFM].enabled = false;
        QVERIFY(scrobble.submit());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 2000);
        QCOMPARE(finished.last()[0].toBool(), true);
        QCOMPARE(failing.requests.size(), disableFailedSite ? 2 : 4);
        QCOMPARE(successful.requests.size(), 2);
        if (!disableFailedSite) QVERIFY(failing.requests.last().contains("t[0]=Retry%20only%20failed%20site"));
        QCOMPARE(scrobble.get_num_tracks(), 0);
        QVERIFY(!QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void newImportAfterFailurePreservesPreviousSource_data() {
        QTest::addColumn<bool>("deleteAll");
        QTest::newRow("replace-import") << false;
        QTest::newRow("delete-all-then-import") << true;
    }

    void newImportAfterFailurePreservesPreviousSource() {
        QFETCH(bool, deleteAll);
        LocalService service;
        QVERIFY(service.listen());
        service.submissionReply = "FAILED try again\n";
        Scrobble scrobble;
        service.configure(scrobble);
        QTemporaryDir previousDirectory;
        QTemporaryDir nextDirectory;
        const int recent = static_cast<int>(scrobble.get_gmt()) - 60;
        QVERIFY(loadTracks(scrobble, previousDirectory.path(), {{"Artist", "Previous", 180, recent}}));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(scrobble.submit());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(finished.first()[0].toBool(), false);
        if (deleteAll) {
            scrobble.remove_track(0);
            scrobble.clear_method();
            QCOMPARE(scrobble.get_num_tracks(), 0);
            QVERIFY(!scrobble.get_parser_busy());
        }
        QVERIFY(loadTracks(scrobble, nextDirectory.path(), {{"Artist", "Next", 180, recent + 1}}));
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QCOMPARE(scrobble.get_track(0).title, "Next");
        service.submissionReply = "OK\n";
        QVERIFY(scrobble.submit());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 2000);
        QCOMPARE(finished.last()[0].toBool(), true);
        QVERIFY(service.requests.last().contains("t[0]=Next"));
        QVERIFY(QFile::exists(previousDirectory.filePath(".scrobbler.log")));
        QVERIFY(!QFile::exists(nextDirectory.filePath(".scrobbler.log")));
    }

    void successfulSubmissionClearsSource() {
        LocalService service;
        QVERIFY(service.listen());
        Scrobble scrobble;
        service.configure(scrobble);
        QTemporaryDir directory;
        const int recent = static_cast<int>(scrobble.get_gmt()) - 60;
        QVERIFY(
            loadTracks(scrobble, directory.path(),
                       {{"Artist", "Initially First", 180, recent - 60}, {"Artist", "Initially Second", 240, recent}}));
        auto moved = scrobble.get_track(0);
        moved.when = recent + 1;
        scrobble.update_track(moved, 0);
        scrobble.clear_log();
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(scrobble.submit());
        const QPointer<Submit> pending = scrobble.submissions.value(CONFIG_LASTFM);
        QVERIFY(pending);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(finished.first().at(0).toBool(), true);
        QCOMPARE(service.requests.size(), 2);
        QVERIFY(service.requests[0].startsWith("GET /?hs=true"));
        QVERIFY(service.requests[1].startsWith("POST /submit "));
        QVERIFY(service.requests[1].contains("a[0]=Artist"));
        QVERIFY(service.requests[1].contains("t[0]=Initially%20Second"));
        QVERIFY(service.requests[1].contains("t[1]=Initially%20First"));
        QCOMPARE(scrobble.get_num_tracks(), 0);
        QVERIFY(!scrobble.get_parser_busy());
        QVERIFY(!QFile::exists(directory.filePath(".scrobbler.log")));
        QVERIFY(scrobble.submissions.isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(pending.isNull());
        QStringList controllerLog;
        for (int i = 0; i < scrobble.get_num_logs(); ++i) {
            const auto event = scrobble.get_log(i);
            if (event.level == LOG_INFO &&
                (event.msg.startsWith("Scrobble::submit()") || event.msg.startsWith("Submit()")))
                controllerLog.append(event.msg);
        }
        QCOMPARE(controllerLog, (QStringList{"Scrobble::submit() finished", "Submit() complete"}));
    }

    void failedSubmissionRetainsSource() {
        LocalService service;
        QVERIFY(service.listen());
        service.submissionReply = "FAILED test failure\n";
        Scrobble scrobble;
        service.configure(scrobble);
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Keep", 180, static_cast<int>(scrobble.get_gmt()) - 60}}));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(scrobble.submit());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(finished.first().at(0).toBool(), false);
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QVERIFY(!scrobble.get_parser_busy());
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
        QVERIFY(scrobble.get_error_str().contains("Server returned an error"));
        QVERIFY(scrobble.submissions.isEmpty());
    }

    void mixedSiteResultsReportFailure() {
        LocalService failing;
        LocalService successful;
        QVERIFY(failing.listen());
        QVERIFY(successful.listen());
        failing.submissionReply = "FAILED test failure\n";
        successful.submissionDelay = 50;  // Controlled completion order, not a test-side sleep.
        Scrobble scrobble;
        failing.configure(scrobble, CONFIG_LASTFM);
        successful.configure(scrobble, CONFIG_LIBREFM);
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Keep", 180, static_cast<int>(scrobble.get_gmt()) - 60}}));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(scrobble.submit());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(finished.first().at(0).toBool(), false);
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QCOMPARE(scrobble.get_track(0).sent[CONFIG_LASTFM], SENT_FAILED);
        QCOMPARE(scrobble.get_track(0).sent[CONFIG_LIBREFM], SENT_SUCCESS);
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
        QVERIFY(scrobble.submissions.isEmpty());
    }

    void handshakeFailureTerminatesWithoutGui() {
        LocalService service;
        QVERIFY(service.listen());
        service.handshakeReply = "BADAUTH\n";
        Scrobble scrobble;
        service.configure(scrobble);
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Keep", 180, static_cast<int>(scrobble.get_gmt()) - 60}}));
        QSignalSpy cancelRequested(&scrobble, &Scrobble::signalCancelProgress);
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(scrobble.submit());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(cancelRequested.count(), 1);
        QCOMPARE(finished.first().at(0).toBool(), false);
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
        QVERIFY(scrobble.submissions.isEmpty());
    }

    void cancellationDuringHandshakeIsRepeatable() {
        LocalService service;
        QVERIFY(service.listen());
        service.holdHandshake = true;
        Scrobble scrobble;
        service.configure(scrobble);
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Keep", 180, static_cast<int>(scrobble.get_gmt()) - 60}}));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(scrobble.submit());
        QTRY_COMPARE_WITH_TIMEOUT(service.requests.size(), 1, 2000);
        scrobble.cancel_submission();
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(0).toBool(), false);
        QVERIFY(scrobble.submissions.isEmpty());
        QCOMPARE(scrobble.get_num_tracks(), 1);
        scrobble.cancel_submission();
        QCOMPARE(finished.count(), 2);
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void destructionDuringSubmissionClosesOwnedRequests() {
        LocalService service;
        QVERIFY(service.listen());
        service.holdHandshake = true;
        auto scrobble = std::make_unique<Scrobble>();
        service.configure(*scrobble);
        QTemporaryDir directory;
        QVERIFY(loadTracks(*scrobble, directory.path(),
                           {{"Artist", "Retain on close", 180, static_cast<int>(scrobble->get_gmt()) - 60}}));
        int completed = 0;
        connect(scrobble.get(), &Scrobble::submission_finished, this, [&](bool) { ++completed; });
        QVERIFY(scrobble->submit());
        QTRY_COMPARE_WITH_TIMEOUT(service.requests.size(), 1, 1000);
        QPointer<Submit> owned = scrobble->submissions.value(CONFIG_LASTFM);
        QVERIFY(owned);
        QList<QPointer<QNetworkAccessManager>> managers;
        for (auto* manager : owned->findChildren<QNetworkAccessManager*>()) managers.append(manager);
        QCOMPARE(managers.size(), 2);
        scrobble.reset();
        QVERIFY(owned.isNull());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        for (const auto& manager : managers) QVERIFY(manager.isNull());
        QTRY_VERIFY_WITH_TIMEOUT(service.connections.first().isNull(), 1000);
        QCOMPARE(completed, 0);
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void metadataEditsRestartOnlyWhenRequired_data() {
        QTest::addColumn<QString>("field");
        for (const QString& field : {QString("artist"), QString("title"), QString("length"), QString("album")})
            QTest::newRow(qPrintable(field)) << field;
    }

    void metadataEditsRestartOnlyWhenRequired() {
        QFETCH(QString, field);
        LocalService service;
        QVERIFY(service.listen());
        service.submissionDelay = 50;
        service.submissionReply = "<lfm status=\"ok\"><track><duration>111000</duration></track></lfm>";
        QNetworkProxy::setApplicationProxy(
            QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", service.server.serverPort()));
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy finished(&scrobble, &Scrobble::missing_times_finished);
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Original", 0, 1700000000}}));
        QTRY_COMPARE_WITH_TIMEOUT(service.requests.size(), 1, 1000);
        service.submissionReply = "<lfm status=\"ok\"><track><duration>235000</duration></track></lfm>";
        auto edited = scrobble.get_track(0);
        if (field == "artist")
            edited.artist = "Edited artist";
        else if (field == "title")
            edited.title = "Edited title";
        else if (field == "length")
            edited.length = 180;
        else
            edited.album = "Edited album";
        scrobble.update_track(edited, 0);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        const auto result = scrobble.get_track(0);
        QCOMPARE(result.artist, edited.artist);
        QCOMPARE(result.title, edited.title);
        QCOMPARE(result.album, edited.album);
        QCOMPARE(result.length, field == "length" ? 180 : field == "album" ? 111 : 235);
        QCOMPARE(service.requests.size(), field == "artist" || field == "title" ? 2 : 1);
        DBCache cache;
        QVERIFY(cache.init());
        QCOMPARE(cache.get_track_length(result.artist, result.title), field == "length" ? -1 : result.length);
    }

    void removingLastPendingTrackCompletesWithoutWritingCache() {
        LocalService service;
        QVERIFY(service.listen());
        service.submissionDelay = 50;
        service.submissionReply = "<lfm status=\"ok\"><track><duration>235000</duration></track></lfm>";
        QNetworkProxy::setApplicationProxy(
            QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", service.server.serverPort()));
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy finished(&scrobble, &Scrobble::missing_times_finished);
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Removed", 0, 1700000000}}));
        QTRY_COMPARE_WITH_TIMEOUT(service.requests.size(), 1, 1000);
        scrobble.remove_track(0);
        QCOMPARE(finished.size(), 1);
        QCOMPARE(scrobble.get_num_tracks(), 0);
        scrobble.clear_method();
        QVERIFY(!scrobble.get_parser_busy());
        QTRY_VERIFY_WITH_TIMEOUT(service.connections.first().isNull(), 1000);
        DBCache cache;
        QVERIFY(cache.init());
        QCOMPARE(cache.get_track_length("Artist", "Removed"), -1);
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void negativeAndEditedShortDurationsAreFilteredSafely() {
        Scrobble scrobble;
        QTemporaryDir directory;
        const int recent = scrobble.get_gmt() - 60;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Negative", -1, recent}, {"Artist", "Edited short", 180, recent}}));
        auto edited = scrobble.get_track(1);
        edited.length = 29;
        scrobble.update_track(edited, 1);
        QCOMPARE(scrobble.get_track(1).played, QChar('L'));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(!scrobble.submit());
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first()[0].toBool(), false);
        QCOMPARE(scrobble.get_error_str(), "Nothing to submit!");
        QCOMPARE(scrobble.get_num_tracks(), 0);
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void unavailableCacheStillAllowsMetadataLookup() {
        const QString database = qEnvironmentVariable("XDG_CONFIG_HOME") + "/qtscrob/qtscrob.db";
        QVERIFY(QDir().mkpath(database));  // A directory cannot be opened as a SQLite database file.
        LocalService service;
        QVERIFY(service.listen());
        service.submissionReply = "<lfm status=\"ok\"><track><duration>235000</duration></track></lfm>";
        QNetworkProxy::setApplicationProxy(
            QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", service.server.serverPort()));
        Scrobble scrobble;
        QTemporaryDir directory;
        QSignalSpy finished(&scrobble, &Scrobble::missing_times_finished);
        QVERIFY(loadTracks(scrobble, directory.path(), {{"Artist", "Uncached", 0, 1700000000}}));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 2000);
        QCOMPARE(scrobble.get_track(0).length, 235);
        QCOMPARE(scrobble.get_track(0).title, "Uncached");
        QCOMPARE(service.requests.size(), 1);
        QVERIFY(QFileInfo(database).isDir());
    }

    void synchronousCancellationListenerCompletesOnlyOnce() {
        LocalService service;
        QVERIFY(service.listen());
        service.handshakeReply = "BADAUTH\n";
        Scrobble scrobble;
        service.configure(scrobble);
        connect(&scrobble, &Scrobble::signalCancelProgress, &scrobble, &Scrobble::cancel_submission);
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Retain", 180, static_cast<int>(scrobble.get_gmt()) - 60}}));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(scrobble.submit());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 1000);
        QCOMPARE(finished.first()[0].toBool(), false);
        QVERIFY(scrobble.submissions.isEmpty());
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void synchronousCancellationListenerCanDestroyController() {
        LocalService service;
        QVERIFY(service.listen());
        service.handshakeReply = "BADAUTH\n";
        QPointer<Scrobble> scrobble = new Scrobble;
        service.configure(*scrobble);
        connect(scrobble, &Scrobble::signalCancelProgress, this, [&] { delete scrobble.data(); });
        QTemporaryDir directory;
        QVERIFY(loadTracks(*scrobble, directory.path(),
                           {{"Artist", "Retain", 180, static_cast<int>(scrobble->get_gmt()) - 60}}));
        QVERIFY(scrobble->submit());
        QList<QPointer<QNetworkAccessManager>> managers;
        for (auto* manager : scrobble->submissions.value(CONFIG_LASTFM)->findChildren<QNetworkAccessManager*>())
            managers.append(manager);
        QCOMPARE(managers.size(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(scrobble.isNull(), 1000);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        for (const auto& manager : managers) QVERIFY(manager.isNull());
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
        QCOMPARE(service.requests.size(), 1);
    }

    void successfulSubmissionAfterImportFailure() {
        LocalService service;
        QVERIFY(service.listen());
        Scrobble scrobble;
        service.configure(scrobble);
        QTemporaryDir directory;
        QVERIFY(scrobble.parse(SCROBBLE_LOG, directory.path()));
        QVERIFY(!scrobble.get_parser_busy());
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Recovered", 180, static_cast<int>(scrobble.get_gmt()) - 60}}));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(scrobble.submit());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 1000);
        QCOMPARE(finished.first()[0].toBool(), true);
        QCOMPARE(scrobble.get_num_tracks(), 0);
        QVERIFY(!QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void allSitesFailWithoutDeletingTrackOrCrashing() {
        LocalService first;
        LocalService second;
        QVERIFY(first.listen());
        QVERIFY(second.listen());
        first.submissionReply = "FAILED first\n";
        second.submissionReply = "FAILED second\n";
        second.submissionDelay = 50;
        Scrobble scrobble;
        first.configure(scrobble, CONFIG_LASTFM);
        second.configure(scrobble, CONFIG_LIBREFM);
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Retain", 180, static_cast<int>(scrobble.get_gmt()) - 60}}));
        QSignalSpy finished(&scrobble, &Scrobble::submission_finished);
        QVERIFY(scrobble.submit());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 1000);
        QCOMPARE(finished.first()[0].toBool(), false);
        QCOMPARE(scrobble.get_num_tracks(), 1);
        QCOMPARE(scrobble.get_track(0).sent[CONFIG_LASTFM], SENT_FAILED);
        QCOMPARE(scrobble.get_track(0).sent[CONFIG_LIBREFM], SENT_FAILED);
        QVERIFY(!scrobble.get_parser_busy());
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void invalidMethodSentinelDoesNotStartParser() {
        Scrobble scrobble;
        QSignalSpy parsed(&scrobble, &Scrobble::parsing_opened);
        QVERIFY(!scrobble.parse(SCROBBLE_NUM_METHODS, QString()));
        QCOMPARE(parsed.size(), 0);
        QVERIFY(!scrobble.get_parser_busy());
        QCOMPARE(scrobble.get_error_str(), "Error - asked to use an unknown Parser!");
    }

    void proxyConfigurationAndReset() {
        Scrobble scrobble;
        QTemporaryDir directory;
        QVERIFY(loadTracks(scrobble, directory.path(),
                           {{"Artist", "Keep", 180, static_cast<int>(scrobble.get_gmt()) - 60}}));
        scrobble.conf->proxy_host = "proxy.invalid";
        scrobble.conf->proxy_port = 3128;
        scrobble.conf->proxy_user = "user";
        scrobble.conf->proxy_pass = "password";
        QVERIFY(!scrobble.submit());  // No sites enabled, so no request is sent to this proxy.
        QNetworkProxy proxy = QNetworkProxy::applicationProxy();
        QCOMPARE(proxy.type(), QNetworkProxy::HttpProxy);
        QCOMPARE(proxy.hostName(), "proxy.invalid");
        QCOMPARE(proxy.port(), quint16(3128));
        QCOMPARE(proxy.user(), "user");
        QCOMPARE(proxy.password(), "password");
        scrobble.conf->proxy_host.clear();
        QVERIFY(!scrobble.submit());
        QCOMPARE(QNetworkProxy::applicationProxy().type(), QNetworkProxy::NoProxy);
    }
};

int main(int argc, char** argv) {
    QTemporaryDir home;
    if (!home.isValid()) return 1;
    test_environment::isolate(home.path());
    const QByteArray zone = qgetenv("QTSCROBBLER_TEST_TIMEZONE");
    qputenv("TZ", zone.isEmpty() ? QByteArray("UTC") : zone);
    QCoreApplication application(argc, argv);
    if (argc == 2 && QByteArray(argv[1]) == "--timezone-probe") {
        const auto before = QDateTime::currentSecsSinceEpoch();
        Scrobble scrobble;
        const auto now = QDateTime::currentDateTime();
        QTextStream output(stdout);
        output << "Observed offset " << scrobble.get_gmt_offset() << ", DST " << scrobble.get_dst() << ", zone "
               << scrobble.get_zonename() << "; expected " << now.offsetFromUtc() << ", " << now.isDaylightTime()
               << ", " << now.timeZoneAbbreviation() << Qt::endl;
        const QString expectedOffset = "UTC " + now.toString("ttt");
        const bool correct = scrobble.get_gmt_offset() == now.offsetFromUtc() &&
                             scrobble.get_dst() == static_cast<int>(now.isDaylightTime()) &&
                             scrobble.get_zonename() == now.timeZoneAbbreviation() &&
                             scrobble.offset_str() == expectedOffset;
        const QStringList expectedLogs{"Detected Timezone: " + now.timeZoneAbbreviation(),
                                       QString("Detected DST: %1").arg(now.isDaylightTime()),
                                       "Detected Offset: " + expectedOffset};
        const int firstTimezoneLog = scrobble.get_num_logs() - 4;
        if (firstTimezoneLog < 0) return 1;
        for (int i = 0; i < expectedLogs.size(); ++i) {
            const auto event = scrobble.get_log(firstTimezoneLog + i);
            output << event.msg << Qt::endl;
            if (event.level != LOG_DEBUG || event.msg != expectedLogs[i]) return 1;
        }
        const auto gmtLog = scrobble.get_log(firstTimezoneLog + 3);
        const auto gmtMatch = QRegularExpression(R"(^Detected GMT: (\d+)$)").match(gmtLog.msg);
        output << gmtLog.msg << Qt::endl;
        if (gmtLog.level != LOG_DEBUG || !gmtMatch.hasMatch()) return 1;
        const auto gmt = gmtMatch.captured(1).toLongLong();
        if (gmt < before || gmt > now.toSecsSinceEpoch()) return 1;
        return correct ? 0 : 1;
    }
    ScrobbleTest tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_scrobble.moc"
