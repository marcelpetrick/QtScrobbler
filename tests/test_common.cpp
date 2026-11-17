#include "common.h"

#include <QTemporaryDir>
#include <QtTest>

class CommonTest : public QObject
{
    Q_OBJECT

    static bool touch(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly);
    }

private slots:
    void chronologicalOrdering()
    {
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

    void logDiscovery()
    {
        QTemporaryDir directory;
        QCOMPARE(check_path(SCROBBLE_LOG, ""), CHECK_FAIL);
        QCOMPARE(check_path(SCROBBLE_LOG, directory.path()), CHECK_FAIL);
        QVERIFY(touch(directory.filePath(".scrobbler-timeless.log")));
        QCOMPARE(check_path(SCROBBLE_LOG, directory.path()), CHECK_LOG_TIMELESS);
        QVERIFY(touch(directory.filePath(".scrobbler.log")));
        QCOMPARE(check_path(SCROBBLE_LOG, directory.path()), CHECK_LOG);
        QCOMPARE(check_path(SCROBBLE_NONE, directory.path()), CHECK_FAIL);
    }

    void ipodDiscovery()
    {
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

    void mtpDiscovery()
    {
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
