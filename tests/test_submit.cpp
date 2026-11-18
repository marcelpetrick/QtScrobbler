// SPDX-License-Identifier: GPL-2.0-only
#include <QCryptographicHash>
#include <QNetworkProxy>
#include <QProcess>
#include <QSignalSpy>
#include <QUrlQuery>
#include <QtTest>

#include "http_fixture.h"
#include "submit.h"

class SubmitTest : public QObject {
    Q_OBJECT

    static QList<scrob_entry> tracks(int count) {
        QList<scrob_entry> entries;
        for (int i = 0; i < count; ++i) {
            scrob_entry entry{};
            entry.artist = QString::fromUtf8("Artist & É");
            entry.album = "Album = Demo";
            entry.title = QString("Track %1").arg(i);
            entry.tracknum = i;
            entry.length = 180 + i;
            entry.when = 1700000000 + i;
            entry.played = 'L';
            entry.mb_track_id = i == 0 ? "00000000-0000-0000-0000-000000000000" : "invalid";
            entries.append(entry);
        }
        return entries;
    }

    static Submit::submit_context context(HttpFixture& server, QList<scrob_entry>& entries, QMutex& mutex) {
        return {"Test", "user & name +/é", "fictional-hash", server.host(), &entries, true, 0, &mutex};
    }

   private slots:
    void init() { QTest::failOnWarning(QRegularExpression(".*")); }

    void initTestCase() { QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy); }

    void batching_data() {
        QTest::addColumn<int>("count");
        for (int count : {1, 49, 50, 51, 100, 101}) QTest::newRow(qPrintable(QString::number(count))) << count;
    }

    void batching() {
        QFETCH(int, count);
        HttpFixture server;
        QVERIFY(server.isListening());
        server.respond = [&](const HttpFixture::Request& request) {
            return HttpFixture::Response{request.method == "GET" ? server.handshake() : QByteArray("OK\n")};
        };
        auto entries = tracks(count);
        QMutex mutex;
        Submit submit(0);
        QVERIFY(submit.init(context(server, entries, mutex)));
        QSignalSpy finished(&submit, &Submit::finished);
        QSignalSpy progress(&submit, &Submit::progress);
        submit.do_submit();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        QCOMPARE(finished.first().at(0).toBool(), true);
        QCOMPARE(finished.first().at(1).toString(), QString());
        QVERIFY(submit.submission_successful());
        QCOMPARE(server.requests.size(), 1 + (count + 49) / 50);
        QCOMPARE(progress.count(), (count + 49) / 50);
        for (int batch = 0; batch < progress.size(); ++batch) {
            const auto event = progress[batch];
            QCOMPARE(event[0].toInt(), 0);
            QCOMPARE(event[1].toInt(), qMin(count, (batch + 1) * 50));
            QCOMPARE(event[2].toInt(), count);
            const auto request = server.requests[batch + 1];
            QCOMPARE(request.method, "POST");
            QCOMPARE(request.headers.value("user-agent"), QByteArray(CLIENT_USERAGENT));
            QUrlQuery form(QString::fromLatin1(request.body));
            const int inBatch = qMin(50, count - batch * 50);
            for (int row = 0; row < inBatch; ++row) {
                const auto& entry = entries[batch * 50 + row];
                const auto key = QString("[%1]").arg(row);
                QCOMPARE(form.queryItemValue("a" + key, QUrl::FullyDecoded), entry.artist);
                QCOMPARE(form.queryItemValue("t" + key, QUrl::FullyDecoded), entry.title);
                QCOMPARE(form.queryItemValue("b" + key, QUrl::FullyDecoded), entry.album);
                QCOMPARE(form.queryItemValue("l" + key), QString::number(entry.length));
                QCOMPARE(form.queryItemValue("i" + key), QString::number(entry.when));
            }
            QVERIFY(!form.hasQueryItem(QString("t[%1]").arg(inBatch)));
        }
        for (const auto& entry : entries) {
            QCOMPARE(entry.sent[0], SENT_SUCCESS);
            QCOMPARE(entry.sent[1], SENT_UNSENT);
        }
        const auto& handshake = server.requests.first();
        QCOMPARE(handshake.method, "GET");
        QCOMPARE(handshake.headers.value("user-agent"), QByteArray(CLIENT_USERAGENT));
        QUrlQuery query(QUrl::fromEncoded(handshake.target));
        QCOMPARE(query.queryItemValue("hs"), "true");
        QCOMPARE(query.queryItemValue("p"), PROTOCOL_VERSION);
        QCOMPARE(query.queryItemValue("c"), CLIENT_ID);
        QCOMPARE(query.queryItemValue("v"), CLIENT_VERSION);
        QCOMPARE(query.queryItemValue("u", QUrl::FullyDecoded), "user & name +/é");
        const auto timestamp = query.queryItemValue("t").toLatin1();
        QVERIFY(qAbs(timestamp.toLongLong() - QDateTime::currentSecsSinceEpoch()) < 5);
        QCOMPARE(query.queryItemValue("a").toLatin1(),
                 QCryptographicHash::hash("fictional-hash" + timestamp, QCryptographicHash::Md5).toHex());
        const auto& post = server.requests[1];
        QCOMPARE(post.method, "POST");
        QCOMPARE(post.target, "/submit");
        QCOMPARE(post.headers.value("content-type"), "application/x-www-form-urlencoded");
        QUrlQuery body(QString::fromLatin1(post.body));
        QCOMPARE(body.queryItemValue("s"), "session");
        QCOMPARE(body.queryItemValue("a[0]", QUrl::FullyDecoded), entries[0].artist);
        QCOMPARE(body.queryItemValue("b[0]", QUrl::FullyDecoded), entries[0].album);
        QCOMPARE(body.queryItemValue("t[0]", QUrl::FullyDecoded), "Track 0");
        QCOMPARE(body.queryItemValue("m[0]"), entries[0].mb_track_id);
        QCOMPARE(body.queryItemValue("n[0]"), "");
        QCOMPARE(body.queryItemValue("l[0]"), "180");
        QCOMPARE(body.queryItemValue("i[0]"), "1700000000");
        QCOMPARE(body.queryItemValue("o[0]"), "P");
        QCOMPARE(body.queryItemValue("r[0]"), "");
        QCOMPARE(body.queryItemValue("portable"), "1");
        if (count > 1) {
            QCOMPARE(body.queryItemValue("m[1]"), "");
            QCOMPARE(body.queryItemValue("n[1]"), "1");
        }
    }

    void handshakeFailure_data() {
        QTest::addColumn<QByteArray>("response");
        QTest::addColumn<QString>("message");
        QTest::newRow("banned") << QByteArray("BANNED\n") << "client software banned";
        QTest::newRow("authentication") << QByteArray("BADAUTH\n") << "authentication problem";
        QTest::newRow("time") << QByteArray("BADTIME\n") << "clock is incorrect";
        QTest::newRow("failed") << QByteArray("FAILED busy\n") << "Handshake Failed";
        QTest::newRow("unknown") << QByteArray("UNKNOWN\n") << "Unknown handshake response";
        QTest::newRow("empty") << QByteArray() << "Bad handshake response";
        QTest::newRow("truncated") << QByteArray("OK\n") << "Bad handshake response";
        QTest::newRow("invalid-url") << QByteArray("OK\nsession\nnow\nfile:///tmp/not-http\n")
                                     << "Bad handshake response";
        QTest::newRow("malformed-endpoint")
            << QByteArray("OK\nsession\nhttp://now\nhttp://[invalid]/submit\n") << "Bad handshake response";
        QTest::newRow("endpoint-without-host")
            << QByteArray("OK\nsession\nhttp://now\nhttp:/submit\n") << "Bad handshake response";
        QTest::newRow("unsupported-protocol")
            << QByteArray("OK\nsession\nhttp://now\nftp://127.0.0.1/submit\n") << "Bad handshake response";
        QTest::newRow("missing-session") << QByteArray("OK\n\nhttp://now\nhttp://submit\n") << "Bad handshake response";
    }

    void handshakeFailure() {
        QFETCH(QByteArray, response);
        QFETCH(QString, message);
        HttpFixture server;
        QVERIFY(server.isListening());
        server.respond = [&](const auto&) { return HttpFixture::Response{response}; };
        auto entries = tracks(1);
        QMutex mutex;
        Submit submit(0);
        QVERIFY(submit.init(context(server, entries, mutex)));
        QSignalSpy failure(&submit, &Submit::signalHandshakeFailure);
        submit.do_submit();
        QTRY_COMPARE_WITH_TIMEOUT(failure.count(), 1, 1000);
        QVERIFY2(failure.first().at(0).toString().contains(message), qPrintable(failure.first().at(0).toString()));
        QCOMPARE(server.requests.size(), 1);
        QCOMPARE(entries[0].sent[0], SENT_UNSENT);
        QVERIFY(!submit.submission_successful());
    }

    void responseFailure_data() {
        QTest::addColumn<QByteArray>("response");
        QTest::newRow("failed") << QByteArray("FAILED busy\n");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("unknown") << QByteArray("UNKNOWN\n");
        QTest::newRow("not-ok") << QByteArray("NOTOK\n");
    }

    void responseFailure() {
        QFETCH(QByteArray, response);
        HttpFixture server;
        QVERIFY(server.isListening());
        server.respond = [&](const auto& request) {
            return HttpFixture::Response{request.method == "GET" ? server.handshake() : response};
        };
        auto entries = tracks(1);
        QMutex mutex;
        Submit submit(0);
        QVERIFY(submit.init(context(server, entries, mutex)));
        QSignalSpy finished(&submit, &Submit::finished);
        submit.do_submit();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);
        QCOMPARE(finished.first().at(0).toBool(), false);
        QCOMPARE(entries[0].sent[0], SENT_FAILED);
        QVERIFY(!submit.submission_successful());
    }

    void responseSizeBoundary_data() {
        QTest::addColumn<bool>("duringPost");
        QTest::addColumn<bool>("oversized");
        QTest::newRow("handshake-at-limit") << false << false;
        QTest::newRow("handshake-over-limit") << false << true;
        QTest::newRow("submission-at-limit") << true << false;
        QTest::newRow("submission-over-limit") << true << true;
    }

    void responseSizeBoundary() {
        QFETCH(bool, duringPost);
        QFETCH(bool, oversized);
        constexpr qsizetype limit = 64 * 1024;
        HttpFixture server;
        server.respond = [&](const auto& request) {
            const bool isPost = request.method == "POST";
            QByteArray body = isPost ? QByteArray("OK\n") : server.handshake();
            if (isPost == duringPost) body = body.leftJustified(limit + oversized, ' ');
            return HttpFixture::Response{body};
        };
        auto entries = tracks(1);
        QMutex mutex;
        Submit submit(0);
        QVERIFY(submit.init(context(server, entries, mutex)));
        QSignalSpy finished(&submit, &Submit::finished);
        QSignalSpy failure(&submit, &Submit::signalHandshakeFailure);
        QSignalSpy progress(&submit, &Submit::progress);
        submit.do_submit();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size() + failure.size(), 1, 1000);

        if (!duringPost && oversized) {
            QCOMPARE(failure.size(), 1);
            QCOMPARE(failure.first().first().toString(), QString("%1: Bad handshake response").arg(SITE_NAME[0]));
            QCOMPARE(server.requests.size(), 1);
            QCOMPARE(progress.size(), 0);
            QCOMPARE(entries[0].sent[0], SENT_UNSENT);
        } else {
            QCOMPARE(finished.size(), 1);
            QCOMPARE(finished.first()[0].toBool(), !oversized);
            QCOMPARE(
                finished.first()[1].toString(),
                oversized ? QString("%1: Server returned an error after sending data").arg(SITE_NAME[0]) : QString());
            QCOMPARE(server.requests.size(), 2);
            QCOMPARE(server.requests.last().method, "POST");
            QCOMPARE(progress.size(), 1);
            QCOMPARE(entries[0].sent[0], oversized ? SENT_FAILED : SENT_SUCCESS);
        }
        QCOMPARE(submit.submission_successful(), !oversized);
    }

    void oversizedStreamingResponseAbortsBeforeServerFinishes() {
        HttpFixture server;
        server.respond = [&](const auto& request) {
            if (request.method == "GET") return HttpFixture::Response{server.handshake()};
            return HttpFixture::Response{{}, 200, true};
        };
        auto entries = tracks(1);
        QMutex mutex;
        Submit submit(0);
        QVERIFY(submit.init(context(server, entries, mutex)));
        QSignalSpy finished(&submit, &Submit::finished);
        submit.do_submit();
        QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 2, 1000);

        QTcpSocket* connection = nullptr;
        for (auto* socket : server.findChildren<QTcpSocket*>())
            if (socket->state() == QAbstractSocket::ConnectedState) connection = socket;
        QVERIFY(connection);
        QSignalSpy disconnected(connection, &QTcpSocket::disconnected);
        constexpr qsizetype chunkSize = 64 * 1024;
        const QByteArray chunk = QByteArray::number(chunkSize, 16) + "\r\n" + QByteArray(chunkSize, ' ') + "\r\n";
        const QByteArray reply = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" + chunk + "1\r\nX\r\n";
        QCOMPARE(connection->write(reply), reply.size());
        // Keep the connection open without the terminating chunk. The client must stop as soon as it exceeds
        // the body limit, without waiting for an end marker or relying on Content-Length.
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(disconnected.size(), 1, 1000);
        QCOMPARE(finished.first()[0].toBool(), false);
        QCOMPARE(finished.first()[1].toString(),
                 QString("%1: Server returned an error after sending data").arg(SITE_NAME[0]));
        QCOMPARE(server.requests.size(), 2);
        QCOMPARE(entries[0].sent[0], SENT_FAILED);
    }

    void retry_data() {
        QTest::addColumn<QByteArray>("response");
        QTest::newRow("bad-session") << QByteArray("BADSESSION\n");
        QTest::newRow("missing-post") << QByteArray("FAILED no POST parameters\n");
    }

    void retry() {
        QFETCH(QByteArray, response);
        HttpFixture server;
        int posts = 0;
        server.respond = [&](const auto& request) {
            if (request.method == "GET") return HttpFixture::Response{server.handshake()};
            return HttpFixture::Response{++posts == 1 ? response : QByteArray("OK\n")};
        };
        auto entries = tracks(1);
        QMutex mutex;
        Submit submit(0);
        QVERIFY(submit.init(context(server, entries, mutex)));
        QSignalSpy finished(&submit, &Submit::finished);
        QSignalSpy progress(&submit, &Submit::progress);
        submit.do_submit();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QVERIFY(finished.first().at(0).toBool());
        QCOMPARE(posts, 2);
        QCOMPARE(server.requests.size(), 4);
        for (const auto& event : progress) QVERIFY(event[1].toInt() <= event[2].toInt());
        QCOMPARE(entries[0].sent[0], SENT_SUCCESS);
    }

    void emptyAndUninitializedDoNotRequest() {
        HttpFixture server;
        auto entries = tracks(0);
        QMutex mutex;
        Submit submit(0);
        submit.do_submit();  // Must not dereference an uninitialized context.
        QVERIFY(!submit.init({}));
        QVERIFY(submit.init(context(server, entries, mutex)));
        QSignalSpy finished(&submit, &Submit::finished);
        submit.do_submit();
        QCOMPARE(finished.size(), 1);
        QVERIFY(!finished.first()[0].toBool());
        QCOMPARE(server.requests.size(), 0);
    }

    void invalidContext_data() {
        QTest::addColumn<int>("site");
        QTest::addColumn<bool>("haveEntries");
        QTest::addColumn<bool>("haveMutex");
        QTest::newRow("negative-site") << -1 << true << true;
        QTest::newRow("site-past-end") << int(CONFIG_NUM_SITES) << true << true;
        QTest::newRow("missing-entries") << 0 << false << true;
        QTest::newRow("missing-mutex") << 0 << true << false;
    }

    void invalidContext() {
        QFETCH(int, site);
        QFETCH(bool, haveEntries);
        QFETCH(bool, haveMutex);
        HttpFixture server;
        auto entries = tracks(1);
        QMutex mutex;
        auto settings = context(server, entries, mutex);
        if (!haveEntries) settings.entries = nullptr;
        if (!haveMutex) settings.mutex = nullptr;
        Submit submit(site);
        QVERIFY(!submit.init(settings));
        submit.do_submit();
        QCOMPARE(server.requests.size(), 0);
        QCOMPARE(entries[0].sent[0], SENT_UNSENT);
        QVERIFY(!submit.submission_successful());
    }

    void noEligibleTracksNeverPostsOrLosesSuccessfulTracks() {
        HttpFixture server;
        server.respond = [&](const auto&) { return HttpFixture::Response{server.handshake()}; };
        auto entries = tracks(2);
        entries[0].sent[0] = SENT_SUCCESS;
        entries[1].sent[0] = SENT_FAILED;
        QMutex mutex;
        Submit submit(0);
        QVERIFY(submit.init(context(server, entries, mutex)));
        QSignalSpy finished(&submit, &Submit::finished);
        QSignalSpy progress(&submit, &Submit::progress);
        submit.do_submit();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 1000);
        QCOMPARE(finished.first()[0].toBool(), false);
        QVERIFY(finished.first()[1].toString().contains("empty submission"));
        QCOMPARE(server.requests.size(), 1);
        QCOMPARE(server.requests.first().method, "GET");
        QCOMPARE(progress.size(), 0);
        QCOMPARE(entries[0].sent[0], SENT_SUCCESS);
        QCOMPARE(entries[1].sent[0], SENT_FAILED);
        QVERIFY(!submit.submission_successful());
    }

    void secureEndpointIsAcceptedAndCancellationKeepsTrack() {
        HttpFixture server;
        const QByteArray endpoint = "https://" + server.host().toLatin1() + "/secure-submit";
        server.respond = [&](const auto&) {
            return HttpFixture::Response{"OK\nsecure & session\n" + server.url("/now") + '\n' + endpoint + '\n'};
        };
        auto entries = tracks(1);
        QMutex mutex;
        Submit submit(0);
        QVERIFY(submit.init(context(server, entries, mutex)));
        QSignalSpy finished(&submit, &Submit::finished);
        QSignalSpy failure(&submit, &Submit::signalHandshakeFailure);
        submit.do_submit();
        QPointer<QNetworkReply> secure;
        auto pendingSecureRequest = [&] {
            for (auto* reply : submit.findChildren<QNetworkReply*>())
                if (reply->url() == QUrl(QString::fromLatin1(endpoint))) secure = reply;
            return bool(secure);
        };
        QTRY_VERIFY_WITH_TIMEOUT(pendingSecureRequest(), 1000);
        QCOMPARE(secure->operation(), QNetworkAccessManager::PostOperation);
        submit.cancel_submission();
        QCOMPARE(failure.size(), 0);
        QCOMPARE(finished.size(), 0);
        QVERIFY(!submit.submission_successful());
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].sent[0], SENT_FAILED);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(secure.isNull());
    }

    void retriesAreBoundedAndKeepSuccessfulTracks() {
        HttpFixture server;
        int posts = 0;
        server.respond = [&](const auto& request) {
            if (request.method == "GET") return HttpFixture::Response{server.handshake()};
            return HttpFixture::Response{++posts == 1 ? QByteArray("OK\n") : QByteArray("BADSESSION\n")};
        };
        auto entries = tracks(51);
        QMutex mutex;
        Submit submit(0);
        QVERIFY(submit.init(context(server, entries, mutex)));
        QSignalSpy finished(&submit, &Submit::finished);
        QSignalSpy progress(&submit, &Submit::progress);
        submit.do_submit();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
        QVERIFY(!finished.first()[0].toBool());
        QVERIFY(finished.first()[1].toString().contains("retry limit"));
        QCOMPARE(posts, 4);
        QCOMPARE(server.requests.size(), 7);  // Three handshakes, four posts.
        for (int i = 0; i < 50; ++i) QCOMPARE(entries[i].sent[0], SENT_SUCCESS);
        QCOMPARE(entries[50].sent[0], SENT_FAILED);
        for (const auto& event : progress) QVERIFY(event[1].toInt() <= event[2].toInt());
    }

    void cancellation_data() {
        QTest::addColumn<bool>("duringPost");
        QTest::newRow("handshake") << false;
        QTest::newRow("submission") << true;
    }

    void cancellation() {
        QFETCH(bool, duringPost);
        HttpFixture server;
        server.respond = [&](const auto& request) {
            if (duringPost && request.method == "GET") return HttpFixture::Response{server.handshake()};
            return HttpFixture::Response{{}, 200, true};
        };
        auto entries = tracks(1);
        QMutex mutex;
        Submit submit(0);
        QVERIFY(submit.init(context(server, entries, mutex)));
        QSignalSpy finished(&submit, &Submit::finished);
        QSignalSpy handshakeFailure(&submit, &Submit::signalHandshakeFailure);
        submit.do_submit();
        QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), duringPost ? 2 : 1, 1000);
        QPointer<QNetworkReply> pending;
        for (auto* manager : submit.findChildren<QNetworkAccessManager*>())
            for (auto* reply : manager->findChildren<QNetworkReply*>())
                if (!reply->isFinished()) pending = reply;
        QVERIFY(pending);
        submit.cancel_submission();
        QVERIFY(pending->isFinished());
        QCOMPARE(pending->error(), QNetworkReply::OperationCanceledError);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(pending.isNull());
        submit.do_submit();
        QCOMPARE(server.requests.size(), duringPost ? 2 : 1);
        QCOMPARE(finished.size(), 0);
        QCOMPARE(handshakeFailure.size(), 0);
        QCOMPARE(entries[0].sent[0], duringPost ? SENT_FAILED : SENT_UNSENT);
        QVERIFY(!submit.submission_successful());
    }

    void networkFailure_data() {
        QTest::addColumn<bool>("duringPost");
        QTest::newRow("handshake") << false;
        QTest::newRow("submission") << true;
    }

    void networkFailure() {
        QFETCH(bool, duringPost);
        HttpFixture server;
        server.respond = [&](const auto& request) {
            if (duringPost && request.method == "GET") return HttpFixture::Response{server.handshake()};
            return HttpFixture::Response{QByteArray("offline"), 503};
        };
        auto entries = tracks(1);
        QMutex mutex;
        Submit submit(0);
        QVERIFY(submit.init(context(server, entries, mutex)));
        QSignalSpy finished(&submit, &Submit::finished);
        QSignalSpy handshakeFailure(&submit, &Submit::signalHandshakeFailure);
        submit.do_submit();
        if (duringPost) {
            QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 1000);
            QVERIFY(!finished.first()[0].toBool());
        } else {
            QTRY_COMPARE_WITH_TIMEOUT(handshakeFailure.size(), 1, 1000);
        }
        QVERIFY(!submit.submission_successful());
        QCOMPARE(entries[0].sent[0], duringPost ? SENT_FAILED : SENT_UNSENT);
    }

    void managersAreOwned() {
        Submit submit(0);
        QCOMPARE(submit.findChildren<QNetworkAccessManager*>().size(), 2);
    }
};

QTEST_GUILESS_MAIN(SubmitTest)
#include "test_submit.moc"
