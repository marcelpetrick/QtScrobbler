// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2026 Marcel Petrick

#include "gettrackinfo.h"

#include <QPointer>
#include <QSignalSpy>
#include <QtTest>
#include <cstring>
#include <memory>

namespace {
class FakeReply : public QNetworkReply
{
public:
    FakeReply(const QNetworkRequest &request, QNetworkAccessManager::Operation operation, QObject *parent)
        : QNetworkReply(parent)
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(operation);
        open(QIODevice::ReadOnly);
    }

    void complete(const QByteArray &body, NetworkError error = NoError)
    {
        if (isFinished())
            return;
        contents = body;
        if (error != NoError)
            setError(error, "synthetic failure");
        setFinished(true);
        emit readyRead();
        emit finished();
    }

    void abort() override { complete({}, OperationCanceledError); }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override
    {
        return contents.size() - position + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char *destination, qint64 maximum) override
    {
        const qint64 count = qMin<qint64>(maximum, contents.size() - position);
        if (count <= 0)
            return -1;
        std::memcpy(destination, contents.constData() + position, count);
        position += count;
        return count;
    }

private:
    QByteArray contents;
    qsizetype position = 0;
};

class FakeManager : public QNetworkAccessManager
{
public:
    struct Request
    {
        Operation operation;
        QNetworkRequest request;
        QByteArray body;
    };
    QList<Request> requests;
    QList<QPointer<FakeReply>> replies;

protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *outgoing) override
    {
        requests.append({operation, request, outgoing ? outgoing->readAll() : QByteArray()});
        auto *reply = new FakeReply(request, operation, this);
        replies.append(reply);
        return reply;
    }
};

scrob_entry track()
{
    scrob_entry entry{};
    entry.artist = QString::fromUtf8("Björk & ?");
    entry.title = QString::fromUtf8("Song / + ? 測試");
    return entry;
}

QByteArray durationXml(const QByteArray &milliseconds)
{
    return "<lfm status=\"ok\"><track><duration>" + milliseconds + "</duration></track></lfm>";
}

void flushDeletes()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
} // namespace

class GetTrackInfoTest : public QObject
{
    Q_OBJECT

private slots:
    void init() { QTest::failOnWarning(QRegularExpression(".*")); }

    void exactRequestAndSuccess()
    {
        FakeManager manager;
        const QUrl endpoint("https://example.invalid/track-info?contract=1");
        GetTrackInfo client(&manager, endpoint);
        QSignalSpy finished(&client, &GetTrackInfo::finished);
        client.get(7, track(), 0);
        QCOMPARE(manager.requests.size(), 1);
        const auto &sent = manager.requests.first();
        QCOMPARE(sent.operation, QNetworkAccessManager::PostOperation);
        QCOMPARE(sent.request.url(), endpoint);
        QCOMPARE(sent.request.rawHeader("User-Agent"), QByteArray(CLIENT_USERAGENT));
        QCOMPARE(sent.request.header(QNetworkRequest::ContentTypeHeader).toString(),
                 "application/x-www-form-urlencoded");
        QCOMPARE(sent.body, QByteArray("method=track.getinfo&api_key=") + API_KEY
                 + "&artist=Bj%C3%B6rk%20%26%20%3F&track=Song%20%2F%20%2B%20%3F%20%E6%B8%AC%E8%A9%A6&autocorrect=1");
        manager.replies.first()->complete(durationXml("210999"));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 1000);
        QCOMPARE(finished.first().at(0).toInt(), 7);
        QCOMPARE(finished.first().at(1).toBool(), true);
        QCOMPARE(finished.first().at(2).toInt(), 210);
        QVERIFY(finished.first().at(3).toInt() >= 0);
        flushDeletes();
        QVERIFY(manager.replies.first().isNull());
    }

    void durationBoundaries_data()
    {
        QTest::addColumn<QByteArray>("xml");
        QTest::addColumn<bool>("success");
        QTest::addColumn<int>("seconds");
        QTest::newRow("one-second") << durationXml("1000") << true << 1;
        QTest::newRow("fraction-truncated") << durationXml("1999") << true << 1;
        QTest::newRow("whitespace") << durationXml(" 210000\n") << true << 210;
        QTest::newRow("max-int-seconds") << durationXml("2147483647999") << true << 2147483647;
        QTest::newRow("seconds-overflow") << durationXml("2147483648000") << false << 0;
        QTest::newRow("integer-overflow") << durationXml("9223372036854775808") << false << 0;
        QTest::newRow("sub-second") << durationXml("999") << false << 0;
        QTest::newRow("zero") << durationXml("0") << false << 0;
        QTest::newRow("negative") << durationXml("-1000") << false << 0;
        QTest::newRow("non-numeric") << durationXml("unknown") << false << 0;
        QTest::newRow("decimal") << durationXml("1234.5") << false << 0;
        QTest::newRow("empty-duration") << durationXml("") << false << 0;
    }

    void durationBoundaries()
    {
        QFETCH(QByteArray, xml);
        QFETCH(bool, success);
        QFETCH(int, seconds);
        FakeManager manager;
        GetTrackInfo client(&manager, QUrl("https://example.invalid/"));
        QSignalSpy finished(&client, &GetTrackInfo::finished);
        client.get(123, track(), 0);
        manager.replies.first()->complete(xml);
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(0).toInt(), 123);
        QCOMPARE(finished.first().at(1).toBool(), success);
        QCOMPARE(finished.first().at(2).toInt(), seconds);
        QVERIFY(finished.first().at(3).toInt() >= 0);
        flushDeletes();
        QVERIFY(manager.replies.first().isNull());
    }

    void malformedXml_data()
    {
        QTest::addColumn<QByteArray>("xml");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("whitespace") << QByteArray(" \n\t");
        QTest::newRow("not-xml") << QByteArray("not XML");
        QTest::newRow("truncated-root") << QByteArray("<lfm status=\"ok\">");
        QTest::newRow("truncated-after-duration") << QByteArray("<lfm status=\"ok\"><track><duration>210000</duration>");
        QTest::newRow("missing-status") << QByteArray("<lfm><track><duration>1000</duration></track></lfm>");
        QTest::newRow("failed-status") << QByteArray("<lfm status=\"failed\"><error code=\"6\">not found</error></lfm>");
        QTest::newRow("status-prefix") << QByteArray("<lfm status=\"okay\"><track><duration>1000</duration></track></lfm>");
        QTest::newRow("wrong-root") << QByteArray("<other status=\"ok\"><track><duration>1000</duration></track></other>");
        QTest::newRow("missing-track") << QByteArray("<lfm status=\"ok\"/>");
        QTest::newRow("duration-outside-track") << QByteArray("<lfm status=\"ok\"><duration>1000</duration></lfm>");
        QTest::newRow("missing-duration") << QByteArray("<lfm status=\"ok\"><track><name>name</name></track></lfm>");
        QTest::newRow("nested-duration") << durationXml("<value>1000</value>");
        QTest::newRow("duplicate-duration") << QByteArray("<lfm status=\"ok\"><track><duration>1000</duration><duration>2000</duration></track></lfm>");
        QTest::newRow("duplicate-track") << QByteArray("<lfm status=\"ok\"><track><duration>1000</duration></track><track><duration>2000</duration></track></lfm>");
        QTest::newRow("junk-after-document") << durationXml("1000") + "junk";
        QTest::newRow("two-documents") << durationXml("1000") + durationXml("2000");
    }

    void malformedXml()
    {
        QFETCH(QByteArray, xml);
        FakeManager manager;
        GetTrackInfo client(&manager, QUrl("https://example.invalid/"));
        QSignalSpy finished(&client, &GetTrackInfo::finished);
        QList<QString> errors;
        connect(&client, &GetTrackInfo::add_log, this, [&](LOG_LEVEL level, const QString &message) {
            if (level == LOG_ERROR)
                errors.append(message);
        });
        client.get(42, track(), 0);
        manager.replies.first()->complete(xml);
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(0).toInt(), 42);
        QCOMPARE(finished.first().at(1).toBool(), false);
        QCOMPARE(finished.first().at(2).toInt(), 0);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(errors.first(), xml.trimmed().isEmpty() ? "API: Empty XML" : "API: Invalid track duration XML");
        flushDeletes();
        QVERIFY(manager.replies.first().isNull());
    }

    void unrelatedMetadataIsSkipped()
    {
        FakeManager manager;
        GetTrackInfo client(&manager, QUrl("https://example.invalid/"));
        QSignalSpy finished(&client, &GetTrackInfo::finished);
        client.get(8, track(), 0);
        manager.replies.first()->complete(
            "<lfm status=\"ok\"><ignored><duration>9999</duration></ignored>"
            "<track><name>Song</name><artist><name>Artist</name></artist><duration>1000</duration></track></lfm>");
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(1).toBool(), true);
        QCOMPARE(finished.first().at(2).toInt(), 1);
    }

    void repeatedAndOverlappingRequests()
    {
        FakeManager manager;
        GetTrackInfo client(&manager, QUrl("https://example.invalid/"));
        QSignalSpy finished(&client, &GetTrackInfo::finished);
        client.get(1, track(), 0);
        client.get(2, track(), 0);
        QCOMPARE(manager.replies.size(), 2);
        manager.replies[1]->complete(durationXml("2000"));
        manager.replies[0]->complete(durationXml("1000"));
        QCOMPARE(finished.size(), 2);
        QCOMPARE(finished[0].at(0).toInt(), 2);
        QCOMPARE(finished[0].at(2).toInt(), 2);
        QCOMPARE(finished[1].at(0).toInt(), 1);
        QCOMPARE(finished[1].at(2).toInt(), 1);
        client.get(3, track(), 0);
        manager.replies[2]->complete("<lfm status=\"ok\"><track/>");
        client.get(4, track(), 0);
        manager.replies[3]->complete(durationXml("4000"));
        QCOMPARE(finished.size(), 4);
        QCOMPARE(finished[2].at(0).toInt(), 3);
        QCOMPARE(finished[2].at(1).toBool(), false);
        QCOMPARE(finished[3].at(0).toInt(), 4);
        QCOMPARE(finished[3].at(1).toBool(), true);
        QCOMPARE(finished[3].at(2).toInt(), 4);
    }

    void networkErrors_data()
    {
        QTest::addColumn<int>("error");
        QTest::newRow("refused") << int(QNetworkReply::ConnectionRefusedError);
        QTest::newRow("timeout") << int(QNetworkReply::TimeoutError);
        QTest::newRow("cancelled") << int(QNetworkReply::OperationCanceledError);
        QTest::newRow("not-found") << int(QNetworkReply::ContentNotFoundError);
    }

    void networkErrors()
    {
        QFETCH(int, error);
        FakeManager manager;
        GetTrackInfo client(&manager, QUrl("https://example.invalid/"));
        QSignalSpy finished(&client, &GetTrackInfo::finished);
        QString message;
        connect(&client, &GetTrackInfo::add_log, this, [&](LOG_LEVEL level, const QString &text) {
            if (level == LOG_ERROR)
                message = text;
        });
        client.get(17, track(), 0);
        manager.replies.first()->complete(durationXml("1000"), QNetworkReply::NetworkError(error));
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(0).toInt(), 17);
        QCOMPARE(finished.first().at(1).toBool(), false);
        QCOMPARE(finished.first().at(2).toInt(), 0);
        QCOMPARE(message, "API: Failed to download track length: synthetic failure");
        flushDeletes();
        QVERIFY(manager.replies.first().isNull());
    }

    void delayedRequestKeepsEventLoopResponsive()
    {
        FakeManager manager;
        GetTrackInfo client(&manager, QUrl("https://example.invalid/"));
        QSignalSpy finished(&client, &GetTrackInfo::finished);
        client.get(55, track(), 20);
        QCOMPARE(manager.requests.size(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(manager.requests.size(), 1, 1000);
        manager.replies.first()->complete(durationXml("1000"));
        QCOMPARE(finished.first().at(0).toInt(), 55);
    }

    void negativeDelayIsImmediate()
    {
        FakeManager manager;
        GetTrackInfo client(&manager, QUrl("https://example.invalid/"));
        client.get(1, track(), -1);
        QCOMPARE(manager.requests.size(), 1);
        manager.replies.first()->complete(durationXml("1000"));
    }

    void externalManagerAndReplyLifetimes()
    {
        auto manager = std::make_unique<FakeManager>();
        GetTrackInfo client(manager.get(), QUrl("https://example.invalid/"));
        QSignalSpy finished(&client, &GetTrackInfo::finished);
        client.get(27, track(), 0);
        manager.reset();
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(0).toInt(), 27);
        QCOMPARE(finished.first().at(1).toBool(), false);
        client.get(28, track(), 0);
        QCOMPARE(finished.size(), 2);
        QCOMPARE(finished.last().at(0).toInt(), 28);
        QCOMPARE(finished.last().at(1).toBool(), false);
    }

    void destructionAbortsOnlyOwnedRequests()
    {
        FakeManager manager;
        QNetworkRequest unrelatedRequest(QUrl("https://example.invalid/unrelated"));
        QNetworkReply *unrelated = manager.get(unrelatedRequest);
        auto client = std::make_unique<GetTrackInfo>(&manager, QUrl("https://example.invalid/track"));
        client->get(1, track(), 0);
        QPointer<FakeReply> owned = manager.replies[1];
        client.reset();
        QVERIFY(!unrelated->isFinished());
        QVERIFY(owned->isFinished());
        QCOMPARE(owned->error(), QNetworkReply::OperationCanceledError);
        flushDeletes();
        QVERIFY(owned.isNull());
        QVERIFY(manager.replies.first());
    }

    void cancellationSuppressesPreviousGeneration()
    {
        FakeManager manager;
        GetTrackInfo client(&manager, QUrl("https://example.invalid/"));
        QSignalSpy finished(&client, &GetTrackInfo::finished);
        client.get(1, track(), 0);
        client.get(2, track(), 10);
        client.cancel_pending();
        QCOMPARE(finished.size(), 0);
        QVERIFY(manager.replies.first()->isFinished());
        QCOMPARE(manager.replies.first()->error(), QNetworkReply::OperationCanceledError);
        QTimer barrier;
        barrier.setSingleShot(true);
        QSignalSpy elapsed(&barrier, &QTimer::timeout);
        barrier.start(30);
        QVERIFY(elapsed.wait(1000));
        QCOMPARE(manager.requests.size(), 1); // Canceled delay never posts its request.
        QCOMPARE(finished.size(), 0);
        client.get(3, track(), 0);
        manager.replies.last()->complete(durationXml("3000"));
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(0).toInt(), 3);
        QCOMPARE(finished.first().at(2).toInt(), 3);
    }

    void defaultManagerAndThreadAreOwned()
    {
        auto *client = new GetTrackInfo;
        QPointer<QNetworkAccessManager> manager = client->findChild<QNetworkAccessManager *>();
        QVERIFY(manager);
        QCOMPARE(manager->parent(), client);
        client->start();
        QTRY_VERIFY_WITH_TIMEOUT(client->isRunning(), 1000);
        delete client; // Destructor must stop/join the running QThread safely.
        QVERIFY(manager.isNull());
    }
};

QTEST_GUILESS_MAIN(GetTrackInfoTest)
#include "test_gettrackinfo.moc"
