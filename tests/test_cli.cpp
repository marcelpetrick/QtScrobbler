// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2026 Marcel Petrick
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QProcess>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtTest>

#include "common.h"
#include "test_environment.h"

struct HttpRequest {
    QByteArray method;
    QByteArray target;
    QHash<QByteArray, QByteArray> headers;
    QByteArray body;
};

class FakeScrobbleServer : public QTcpServer {
   public:
    QList<HttpRequest> requests;
    QByteArray handshakeResult;
    QByteArray submissionResult = "OK\n";
    QByteArray metadataResult = "<lfm status=\"ok\"><track><duration>235000</duration></track></lfm>";
    bool listenLocally() { return listen(QHostAddress::LocalHost, 0); }
    QString host() const { return QString("127.0.0.1:%1").arg(serverPort()); }

    FakeScrobbleServer() {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                QTcpSocket* socket = nextPendingConnection();
                buffers.insert(socket, QByteArray());
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    QByteArray& buffer = buffers[socket];
                    buffer += socket->readAll();
                    const qsizetype end = buffer.indexOf("\r\n\r\n");
                    if (end < 0) return;
                    HttpRequest request;
                    const QList<QByteArray> lines = buffer.left(end).split('\n');
                    const QList<QByteArray> first = lines.first().trimmed().split(' ');
                    if (first.size() != 3) return;
                    request.method = first[0];
                    request.target = first[1];
                    for (qsizetype i = 1; i < lines.size(); ++i) {
                        const QByteArray line = lines[i].trimmed();
                        const qsizetype colon = line.indexOf(':');
                        if (colon > 0)
                            request.headers.insert(line.left(colon).toLower(), line.mid(colon + 1).trimmed());
                    }
                    const int size = request.headers.value("content-length").toInt();
                    if (buffer.size() - end - 4 < size) return;
                    request.body = buffer.mid(end + 4, size);
                    requests.append(request);
                    QByteArray response;
                    if (request.body.startsWith("method=track.getinfo"))
                        response = metadataResult;
                    else if (request.method == "GET") {
                        response = handshakeResult.isNull()
                                       ? QByteArray("OK\nfixture-session\nhttp://") + host().toLatin1() +
                                             "/now\nhttp://" + host().toLatin1() + "/submit\n"
                                       : handshakeResult;
                    } else
                        response = submissionResult;
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " +
                                  QByteArray::number(response.size()) + "\r\nConnection: close\r\n\r\n" + response);
                    socket->disconnectFromHost();
                    buffer.clear();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

   private:
    QHash<QTcpSocket*, QByteArray> buffers;
};

class CliTest : public QObject {
    Q_OBJECT
    struct Result {
        bool started = false;
        bool timedOut = false;
        int exitCode = -1;
        QProcess::ExitStatus exitStatus = QProcess::CrashExit;
        QByteArray output;
        QByteArray error;
    };

    static Result run(const QStringList& arguments, const QString& home) {
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("HOME", home);
        environment.insert("XDG_CONFIG_HOME", home);
        environment.insert("XDG_CACHE_HOME", home);
        environment.insert("LC_ALL", "C.UTF-8");
        environment.insert("TZ", "UTC");
        environment.remove("POSIXLY_CORRECT");
        process.setProcessEnvironment(environment);
        process.start(CLI_BINARY, arguments);
        Result result;
        result.started = process.waitForStarted(2000);
        QElapsedTimer timer;
        timer.start();
        while (process.state() != QProcess::NotRunning && timer.elapsed() < 5000)
            QTest::qWait(1);  // Runs the parent's loopback HTTP server while the child works.
        result.timedOut = process.state() != QProcess::NotRunning;
        if (result.timedOut) {
            process.kill();
            process.waitForFinished(2000);
        }
        result.exitStatus = process.exitStatus();
        result.exitCode = process.exitCode();
        result.output = test_environment::lines(process.readAllStandardOutput());
        result.error = test_environment::lines(process.readAllStandardError());
        return result;
    }

    static bool write(const QString& path, const QByteArray& contents) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
    }

    static QString config(const QString& directory, const QString& host = QString()) {
        const QString path = directory + "/fixture.ini";
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue("Custom/enabled", !host.isEmpty());
        settings.setValue("Custom/conf_name", "Offline fixture");
        settings.setValue("Custom/handshake_host", host);
        settings.setValue("Custom/username", "Demo User +/é");
        settings.setValue("Custom/password_hash", "0123456789abcdef0123456789abcdef");
        settings.sync();
        return settings.status() == QSettings::NoError ? path : QString();
    }

    static QByteArray log(qint64 timestamp, bool utc = true) {
        return QByteArray("#AUDIOSCROBBLER/1.1\n#TZ/") + (utc ? "UTC" : "UNKNOWN") +
               "\n#CLIENT/QtScrobbler-offline-test\n" +
               "Fictional Élan & Friends\tDemo Album\tSample + / Tune\t2\t180\tL\t" + QByteArray::number(timestamp) +
               "\t00000000-0000-0000-0000-000000000000\n";
    }

    static QByteArray diagnostics(const Result& result) { return result.output + '\n' + result.error; }

   private slots:
    void help_data() {
        QTest::addColumn<QStringList>("arguments");
        QTest::newRow("long") << QStringList{"--help"};
        QTest::newRow("short") << QStringList{"-h"};
        QTest::newRow("valid-long-options") << QStringList{"--timezone=-2.5", "--verbose=3", "--recalc=0", "--help"};
        QTest::newRow("valid-short-options") << QStringList{"-t", "2.5", "-v", "0", "-r", "2147483647", "-h"};
        QTest::newRow("verbosity-upper") << QStringList{"-v", "3", "-h"};
        QTest::newRow("timezone-lower") << QStringList{"-t", "-24", "-h"};
        QTest::newRow("timezone-upper") << QStringList{"-t", "24", "-h"};
        QTest::newRow("now") << QStringList{"--now", "--help"};
#ifdef HAVE_MTP
        QTest::newRow("mtp-help") << QStringList{"--mtp", "--help"};
#endif
    }

    void help() {
        QFETCH(QStringList, arguments);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const Result result = run(arguments, directory.path());
        QVERIFY2(result.started && !result.timedOut, diagnostics(result).constData());
        QCOMPARE(result.exitStatus, QProcess::NormalExit);
        QCOMPARE(result.exitCode, 0);
        QVERIFY(result.output.startsWith(QByteArray("scrobbler (v") + CLIENT_VERSION + "): a Last.fm uploader\n"));
        QVERIFY(result.output.contains("-c, --config=PATH"));
        QVERIFY(result.output.contains("-f, --file"));
        QVERIFY(result.output.contains("--location=DIR"));
        QVERIFY(result.output.contains("--timezone=HOURS"));
        QVERIFY(!result.output.contains("--timestamp"));
        QVERIFY(result.output.contains("--recalc=TIME"));
        QVERIFY(result.output.contains("-d, --database"));
        QCOMPARE(result.error, QByteArray());
    }

    void invalidArguments_data() {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<QByteArray>("message");
        for (const QString option :
             {"--unknown", "-X", "--config", "--location", "--verbose", "--timezone", "--recalc"})
            QTest::newRow(qPrintable(option))
                << QStringList{option} << QByteArray("Invalid option or missing argument: ") + option.toUtf8() + '\n';
        QTest::newRow("extra") << QStringList{"unexpected", "another"}
                               << QByteArray("Unknown option argument: unexpected another\n");
        QTest::newRow("after-separator") << QStringList{"--", "--help"}
                                         << QByteArray("Unknown option argument: --help\n");
        QTest::newRow("empty-config") << QStringList{"--config=", "--help"}
                                      << QByteArray("Configuration path must not be empty\n");
        for (const QString value : {"bad", "1.5", "1x", "-1", "4", "2147483648"})
            QTest::newRow(qPrintable("verbosity-" + value))
                << QStringList{"--verbose=" + value, "--help"}
                << QByteArray("Verbosity must be an integer between 0 and 3\n");
        for (const QString value : {"bad", "1x", "nan", "inf", "-inf", "-24.01", "24.01"})
            QTest::newRow(qPrintable("timezone-" + value))
                << QStringList{"--timezone=" + value, "--help"}
                << QByteArray("Timezone must be a finite number between -24 and 24 hours\n");
        for (const QString value : {"bad", "1.5", "1x", "-1", "2147483648"})
            QTest::newRow(qPrintable("recalc-" + value))
                << QStringList{"--recalc=" + value, "--help"}
                << QByteArray("Recalculation time must be an integer between 0 and 2147483647\n");
        QTest::newRow("conflicting-time")
            << QStringList{"-n", "-r", "1"} << QByteArray("You can only use one of -n and -r\n");
        QTest::newRow("no-method") << QStringList{} << QByteArray("Error - no (single) method specified\n");
        QTest::newRow("conflicting-methods")
            << QStringList{"-f", "-d"} << QByteArray("Error - no (single) method specified\n");
        QTest::newRow("conflicting-long-methods")
            << QStringList{"--file", "--database"} << QByteArray("Error - no (single) method specified\n");
#ifdef HAVE_MTP
        QTest::newRow("conflicting-mtp-file")
            << QStringList{"-m", "-f"} << QByteArray("Error - no (single) method specified\n");
        QTest::newRow("conflicting-mtp-db")
            << QStringList{"--mtp", "--database"} << QByteArray("Error - no (single) method specified\n");
#endif
    }

    void invalidArguments() {
        QFETCH(QStringList, arguments);
        QFETCH(QByteArray, message);
        QTemporaryDir directory;
        const Result result = run(arguments, directory.path());
        QVERIFY2(result.started && !result.timedOut, diagnostics(result).constData());
        QCOMPARE(result.exitStatus, QProcess::NormalExit);
        QCOMPARE(result.exitCode, 2);
        QCOMPARE(result.error, message);
        if (message == "Error - no (single) method specified\n") {
            QVERIFY(result.output.startsWith(QByteArray("scrobbler (v") + CLIENT_VERSION + ")"));
            QVERIFY(result.output.contains("Methods (choose exactly one):\n"));
        } else {
            QCOMPARE(result.output, QByteArray());
        }
    }

    void badConfiguration_data() {
        QTest::addColumn<QString>("kind");
        QTest::newRow("missing") << "missing";
        QTest::newRow("directory") << "directory";
        QTest::newRow("corrupt") << "corrupt";
#ifdef HAVE_MTP
        QTest::newRow("mtp-safe-config-error") << "mtp";
#endif
    }

    void badConfiguration() {
        QFETCH(QString, kind);
        QTemporaryDir directory;
        QString file = directory.filePath("missing.ini");
        if (kind == "directory") file = directory.path();
        if (kind == "corrupt") QVERIFY(write(file, "[broken\nkey=value\n"));
        QStringList arguments{kind == "mtp" ? "-m" : "-f", "-c", file, "-l", directory.path()};
        const Result result = run(arguments, directory.path());
        QVERIFY2(result.started && !result.timedOut, diagnostics(result).constData());
        QCOMPARE(result.exitStatus, QProcess::NormalExit);
        QCOMPARE(result.exitCode, 1);
        QCOMPARE(result.error, (kind == "corrupt" ? QByteArray("Invalid configuration file: ")
                                                  : QByteArray("Unable to read configuration file: ")) +
                                   file.toUtf8() + '\n');
    }

    void parserFailures_data() {
        QTest::addColumn<QString>("kind");
        QTest::newRow("missing-log") << "missing";
        QTest::newRow("short-log") << "short";
        QTest::newRow("bad-header") << "header";
        QTest::newRow("no-records") << "empty";
        QTest::newRow("missing-ipod") << "ipod";
    }

    void parserFailures() {
        QFETCH(QString, kind);
        QTemporaryDir directory;
        if (kind == "short") QVERIFY(write(directory.filePath(".scrobbler.log"), "broken\n"));
        if (kind == "header") QVERIFY(write(directory.filePath(".scrobbler.log"), "wrong\n#TZ/UTC\n#CLIENT/test\n"));
        if (kind == "empty")
            QVERIFY(write(directory.filePath(".scrobbler.log"), "#AUDIOSCROBBLER/1.0\n#TZ/UTC\n#CLIENT/test\n"));
        const Result result = run({kind == "ipod" ? "-d" : "-f", "-l", directory.path()}, directory.path());
        QVERIFY2(result.started && !result.timedOut, diagnostics(result).constData());
        QCOMPARE(result.exitStatus, QProcess::NormalExit);
        QCOMPARE(result.exitCode, 1);
        QVERIFY(result.error.startsWith("Error parsing data: Parsing failed - "));
        QVERIFY(!result.output.contains("Submission complete"));
    }

    void noEnabledSite() {
        QTemporaryDir directory;
        QVERIFY(write(directory.filePath(".scrobbler.log"), log(QDateTime::currentSecsSinceEpoch() - 60)));
        const QString configuration = config(directory.path());
        QVERIFY(!configuration.isEmpty());
        const Result result = run({"-f", "-l", directory.path(), "-c", configuration}, directory.path());
        QVERIFY2(result.started && !result.timedOut, diagnostics(result).constData());
        QCOMPARE(result.exitStatus, QProcess::NormalExit);
        QCOMPARE(result.exitCode, 1);
        QCOMPARE(result.error,
                 QByteArray("Submission failed: No sites were configured and/or enabled. Unable to submit\n"));
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void upload_data() {
        QTest::addColumn<QString>("mode");
        for (const QString mode :
             {"short", "long", "timezone", "utc-overrides-timezone", "recalc", "now", "quiet", "trace"})
            QTest::newRow(qPrintable(mode)) << mode;
    }

    void upload() {
        QFETCH(QString, mode);
        QTemporaryDir directory;
        FakeScrobbleServer server;
        QVERIFY(server.listenLocally());
        const qint64 timestamp = QDateTime::currentSecsSinceEpoch() - 60;
        const QString file = directory.filePath(".scrobbler.log");
        QVERIFY(write(file, log(timestamp, mode != "timezone")));
        const QString configuration = config(directory.path(), server.host());
        QVERIFY(!configuration.isEmpty());
        QStringList arguments =
            mode == "long" ? QStringList{"--file", "--location=" + directory.path(), "--config=" + configuration}
                           : QStringList{"-f", "-l", directory.path(), "-c", configuration};
        if (mode == "timezone" || mode == "utc-overrides-timezone") arguments << "-t" << "2.5";
        const qint64 reference = timestamp + 1000;
        if (mode == "recalc") arguments << "-r" << QString::number(reference);
        if (mode == "now") arguments << "-n";
        if (mode == "quiet") arguments << "-v" << "0";
        if (mode == "trace") arguments << "--verbose=3";
        const qint64 startedAt = QDateTime::currentSecsSinceEpoch();
        const Result result = run(arguments, directory.path());
        const qint64 finishedAt = QDateTime::currentSecsSinceEpoch();
        QVERIFY2(result.started && !result.timedOut, diagnostics(result).constData());
        QCOMPARE(result.exitStatus, QProcess::NormalExit);
        QCOMPARE(result.exitCode, 0);
        QVERIFY(result.output.endsWith("Submission complete\n"));
        if (mode == "quiet") {
            QCOMPARE(result.output, QByteArray("Submission complete\n"));
        } else {
            QVERIFY(result.output.contains("INFO : Successfully parsed 1 log entries\n"));
            QCOMPARE(result.output.contains("DEBUG: Log CLIENT: QtScrobbler-offline-test\n"), mode == "trace");
        }
        QCOMPARE(result.error, QByteArray());
        QCOMPARE(server.requests.size(), 2);
        const HttpRequest& handshake = server.requests[0];
        QCOMPARE(handshake.method, QByteArray("GET"));
        const QUrlQuery handshakeQuery(QUrl::fromEncoded("http://fixture" + handshake.target));
        QCOMPARE(handshakeQuery.queryItemValue("hs"), "true");
        QCOMPARE(handshakeQuery.queryItemValue("p"), QString(PROTOCOL_VERSION));
        QCOMPARE(handshakeQuery.queryItemValue("c"), QString(CLIENT_ID));
        QCOMPARE(handshakeQuery.queryItemValue("v"), QString(CLIENT_VERSION));
        QCOMPARE(handshakeQuery.queryItemValue("u", QUrl::FullyDecoded), "Demo User +/é");
        QCOMPARE(handshakeQuery.queryItemValue("a"),
                 QString::fromLatin1(QCryptographicHash::hash("0123456789abcdef0123456789abcdef" +
                                                                  handshakeQuery.queryItemValue("t").toLatin1(),
                                                              QCryptographicHash::Md5)
                                         .toHex()));
        QCOMPARE(handshake.headers.value("user-agent"), QByteArray(CLIENT_USERAGENT));
        const HttpRequest& submission = server.requests[1];
        QCOMPARE(submission.method, QByteArray("POST"));
        QCOMPARE(submission.target, QByteArray("/submit"));
        QCOMPARE(submission.headers.value("content-type"), QByteArray("application/x-www-form-urlencoded"));
        QCOMPARE(submission.headers.value("user-agent"), QByteArray(CLIENT_USERAGENT));
        const QUrlQuery data(QString::fromUtf8(submission.body));
        QCOMPARE(data.queryItemValue("s"), "fixture-session");
        QCOMPARE(data.queryItemValue("a[0]", QUrl::FullyDecoded), "Fictional Élan & Friends");
        QCOMPARE(data.queryItemValue("t[0]", QUrl::FullyDecoded), "Sample + / Tune");
        QCOMPARE(data.queryItemValue("b[0]", QUrl::FullyDecoded), "Demo Album");
        QCOMPARE(data.queryItemValue("n[0]"), "2");
        QCOMPARE(data.queryItemValue("l[0]"), "180");
        QCOMPARE(data.queryItemValue("o[0]"), "P");
        QCOMPARE(data.queryItemValue("m[0]"), "00000000-0000-0000-0000-000000000000");
        const qint64 submittedTime = data.queryItemValue("i[0]").toLongLong();
        if (mode == "now")
            QVERIFY(submittedTime >= startedAt - 180 && submittedTime <= finishedAt - 180);
        else
            QCOMPARE(submittedTime, mode == "recalc"     ? reference - 180
                                    : mode == "timezone" ? timestamp - 9000
                                                         : timestamp);
        QVERIFY(!QFile::exists(file));
    }

    void missingDurationIsResolvedBeforeSubmission() {
        QTemporaryDir directory;
        FakeScrobbleServer server;
        QVERIFY(server.listenLocally());
        QByteArray contents = log(QDateTime::currentSecsSinceEpoch() - 60);
        contents.replace("\t180\t", "\t0\t");
        QVERIFY(write(directory.filePath(".scrobbler.log"), contents));
        const QString configuration = config(directory.path(), server.host());
        QVERIFY(!configuration.isEmpty());
        QSettings settings(configuration, QSettings::IniFormat);
        settings.setValue("proxy/proxy_host", "127.0.0.1");
        settings.setValue("proxy/proxy_port", server.serverPort());
        settings.sync();
        QCOMPARE(settings.status(), QSettings::NoError);
        const Result result = run({"-f", "-l", directory.path(), "-c", configuration}, directory.path());
        QVERIFY2(result.started && !result.timedOut, diagnostics(result).constData());
        QCOMPARE(result.exitStatus, QProcess::NormalExit);
        QVERIFY2(result.exitCode == 0, diagnostics(result).constData());
        QCOMPARE(result.error, QByteArray());
        QCOMPARE(server.requests.size(), 3);
        QVERIFY(server.requests[0].body.startsWith("method=track.getinfo"));
        const QUrlQuery submitted(QString::fromUtf8(server.requests.last().body));
        QCOMPARE(submitted.queryItemValue("l[0]"), "235");
        QCOMPARE(submitted.queryItemValue("t[0]", QUrl::FullyDecoded), "Sample + / Tune");
        QVERIFY(!QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void rejectedSubmission() {
        QTemporaryDir directory;
        FakeScrobbleServer server;
        QVERIFY(server.listenLocally());
        server.handshakeResult = "BADAUTH\n";
        QVERIFY(write(directory.filePath(".scrobbler.log"), log(QDateTime::currentSecsSinceEpoch() - 60)));
        const QString configuration = config(directory.path(), server.host());
        const Result result = run({"-f", "-l", directory.path(), "-c", configuration}, directory.path());
        QVERIFY2(result.started && !result.timedOut, diagnostics(result).constData());
        QCOMPARE(result.exitStatus, QProcess::NormalExit);
        QCOMPARE(result.exitCode, 1);
        QVERIFY(result.error.startsWith("Submission failed: "));
        QVERIFY(!result.output.contains("Submission complete"));
        QCOMPARE(server.requests.size(), 1);
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }
};

QTEST_GUILESS_MAIN(CliTest)
#include "test_cli.moc"
