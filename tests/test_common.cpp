#include <QTemporaryDir>
#include <QtTest>
#include <limits>

#include "common.h"

class CommonTest : public QObject {
    Q_OBJECT

    static bool touch(const QString& path) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly);
    }

   private slots:
    void init() { QTest::failOnWarning(QRegularExpression(".*")); }

    void orderingBoundaries_data() {
        QTest::addColumn<int>("first");
        QTest::addColumn<int>("second");
        QTest::addColumn<bool>("expected");
        QTest::newRow("minimum-before-maximum")
            << std::numeric_limits<int>::min() << std::numeric_limits<int>::max() << true;
        QTest::newRow("maximum-after-minimum")
            << std::numeric_limits<int>::max() << std::numeric_limits<int>::min() << false;
        QTest::newRow("timeless-before-timed") << 0 << 1 << true;
        QTest::newRow("negative-before-timeless") << -1 << 0 << true;
        QTest::newRow("equal-timeless") << 0 << 0 << false;
        QTest::newRow("equal-minimum") << std::numeric_limits<int>::min() << std::numeric_limits<int>::min() << false;
    }

    void orderingBoundaries() {
        QFETCH(int, first);
        QFETCH(int, second);
        QFETCH(bool, expected);
        scrob_entry left{};
        scrob_entry right{};
        left.when = first;
        right.when = second;
        left.artist = "Z";
        right.artist = "A";
        left.title = "A";
        right.title = "Z";
        left.tracknum = 99;
        right.tracknum = 1;
        QCOMPARE(entry_less_than(left, right), expected);
    }

    void chronologicalOrdering() {
        scrob_entry earlier{};
        scrob_entry later{};
        earlier.when = -1;
        later.when = 1;
        QVERIFY(entry_less_than(earlier, later));
        QVERIFY(!entry_less_than(later, earlier));
        QVERIFY(!entry_less_than(earlier, earlier));
        earlier.when = later.when;
        QVERIFY(!entry_less_than(earlier, later));
    }

    void logDiscovery() {
        QTemporaryDir directory;
        QCOMPARE(check_path(SCROBBLE_LOG, ""), CHECK_FAIL);
        QCOMPARE(check_path(SCROBBLE_LOG, directory.path()), CHECK_FAIL);
        QVERIFY(touch(directory.filePath(".scrobbler-timeless.log")));
        QCOMPARE(check_path(SCROBBLE_LOG, directory.path()), CHECK_LOG_TIMELESS);
        QVERIFY(touch(directory.filePath(".scrobbler.log")));
        QCOMPARE(check_path(SCROBBLE_LOG, directory.path()), CHECK_LOG);
        QCOMPARE(check_path(SCROBBLE_NONE, directory.path()), CHECK_FAIL);
    }

    void ipodDiscovery() {
        QTemporaryDir directory;
        const QString root = directory.filePath("iPod_Control/iTunes");
        QVERIFY(QDir().mkpath(root));
        QCOMPARE(check_path(SCROBBLE_IPOD, directory.path()), CHECK_FAIL);
        QVERIFY(touch(root + "/Play Counts"));
        QCOMPARE(check_path(SCROBBLE_IPOD, directory.path()), CHECK_FAIL);
        QVERIFY(touch(root + "/iTunesCDB"));
        QCOMPARE(check_path(SCROBBLE_IPOD, directory.path()), CHECK_ITUNES_COMPRESSED);
        QVERIFY(touch(root + "/iTunesDB"));
        QCOMPARE(check_path(SCROBBLE_IPOD, directory.path()), CHECK_ITUNES);
        QVERIFY(QFile::remove(root + "/Play Counts"));
        QCOMPARE(check_path(SCROBBLE_IPOD, directory.path()), CHECK_FAIL);
    }

    void mtpDiscovery() {
#ifdef HAVE_MTP
        QCOMPARE(check_path(SCROBBLE_MTP, "device"), CHECK_MTP);
        QCOMPARE(check_path(SCROBBLE_MTP, ""), CHECK_FAIL);
#else
        QSKIP("MTP disabled in this build; exercised in the mandatory MTP-enabled build");
#endif
    }
};

QTEST_GUILESS_MAIN(CommonTest)
#include "test_common.moc"
