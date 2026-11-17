#include "parse-log.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class ParseLogTest : public QObject
{
    Q_OBJECT

    static bool writeLog(const QString &directory, const QByteArray &contents,
                         const QString &name = ".scrobbler.log")
    {
        QFile file(directory + '/' + name);
        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
    }

private slots:
    void example()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QFile::copy(EXAMPLE_LOG, directory.filePath(".scrobbler.log")));
        Parse_Log parser;
        QList<scrob_entry> entries;
        connect(&parser, &Parse_Log::entry, this, [&](scrob_entry entry) { entries.append(entry); });
        QSignalSpy completed(&parser, &Parse_Log::open_finished);
        QSignalSpy baseCompleted(&parser, &Parse::open_finished);
        int baseEntries = 0;
        connect(&parser, &Parse::entry, this, [&](scrob_entry) { ++baseEntries; });

        parser.open(directory.path(), 3600);

        QCOMPARE(baseCompleted.count(), 1);
        QCOMPARE(baseEntries, 3);
        QCOMPARE(completed.count(), 1);
        QCOMPARE(completed.first().at(0).toBool(), true);
        QCOMPARE(completed.first().at(1).toString(), "Successfully parsed 3 log entries");
        QCOMPARE(entries.size(), 3);
        QCOMPARE(entries[0].artist, "Example Artist");
        QCOMPARE(entries[0].album, "Example Album");
        QCOMPARE(entries[0].title, "Morning Signal");
        QCOMPARE(entries[0].tracknum, 1);
        QCOMPARE(entries[0].length, 210);
        QCOMPARE(entries[0].played, QChar('L'));
        QCOMPARE(entries[0].when, 1794902400); // #TZ/UTC overrides supplied offset.
        QCOMPARE(entries[0].mb_track_id, "00000000-0000-0000-0000-000000000000");
        QCOMPARE(entries[1].title, "Evening Echo");
        QCOMPARE(entries[1].when, 1794902700);
        QCOMPARE(entries[2].artist, "Fictional Ensemble");
        QCOMPARE(entries[2].when, 1794903000);
        QCOMPARE(parser.get_method(), SCROBBLE_LOG);

        QSignalSpy cleared(&parser, &Parse_Log::clear_finished);
        parser.clear();
        QCOMPARE(cleared.count(), 1);
        QCOMPARE(cleared.first().at(0).toBool(), true);
        QVERIFY(!QFile::exists(directory.filePath(".scrobbler.log")));
        QVERIFY(QFile::exists(EXAMPLE_LOG));
        parser.clear();
        QCOMPARE(cleared.count(), 2);
        QCOMPARE(cleared.last().at(0).toBool(), false);
    }

    void versionAndTimezone_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::addColumn<int>("offset");
        QTest::addColumn<int>("expectedTime");
        QTest::addColumn<QString>("expectedId");
        const QByteArray record = "\n#CLIENT/test\nArtist\tAlbum\tTitle\t2\t180\tL\t1700000000";
        QTest::newRow("1.0-unknown") << QByteArray("#AUDIOSCROBBLER/1.0\n#TZ/UNKNOWN") + record + '\n'
                                  << 3600 << 1699996400 << QString();
        QTest::newRow("1.0-utc") << QByteArray("#AUDIOSCROBBLER/1.0\n#TZ/UTC") + record + '\n'
                              << -3600 << 1700000000 << QString();
        QTest::newRow("1.1-invalid-id") << QByteArray("#AUDIOSCROBBLER/1.1\n#TZ/UNKNOWN") + record + "\tinvalid\n"
                                     << -3600 << 1700003600 << QString();
        QTest::newRow("1.1-empty-id") << QByteArray("#AUDIOSCROBBLER/1.1\n#TZ/UTC") + record + "\t\n"
                                   << 0 << 1700000000 << QString();
    }

    void versionAndTimezone()
    {
        QFETCH(QByteArray, contents);
        QFETCH(int, offset);
        QFETCH(int, expectedTime);
        QFETCH(QString, expectedId);
        QTemporaryDir directory;
        QVERIFY(writeLog(directory.path(), contents, ".scrobbler-timeless.log"));
        Parse_Log parser;
        QList<scrob_entry> entries;
        connect(&parser, &Parse_Log::entry, this, [&](scrob_entry entry) { entries.append(entry); });
        QSignalSpy completed(&parser, &Parse_Log::open_finished);
        parser.open(directory.path(), offset);
        QCOMPARE(completed.count(), 1);
        QVERIFY(completed.first().at(0).toBool());
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().when, expectedTime);
        QCOMPARE(entries.first().mb_track_id, expectedId);
    }

    void invalidHeader_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::addColumn<QString>("message");
        QTest::newRow("short") << QByteArray("bad\n") << "Parsing failed - log file is empty or corrupt";
        QTest::newRow("version") << QByteArray("#AUDIOSCROBBLER/9.0\n#TZ/UTC\n#CLIENT/test\n")
                                << "Unknown log version: 9.0";
        QTest::newRow("missing-format") << QByteArray("wrong\n#TZ/UTC\n#CLIENT/test\n")
                                       << "Parsing failed - missing #AUDIOSCROBBLER header";
        QTest::newRow("unknown-tz") << QByteArray("#AUDIOSCROBBLER/1.0\n#TZ/CET\n#CLIENT/test\n")
                                   << "Unknown log TZ: CET";
        QTest::newRow("missing-tz") << QByteArray("#AUDIOSCROBBLER/1.0\nwrong\n#CLIENT/test\n")
                                   << "Parsing failed - missing #TZ header";
        QTest::newRow("missing-client") << QByteArray("#AUDIOSCROBBLER/1.0\n#TZ/UTC\nwrong\n")
                                       << "Parsing failed - missing #CLIENT header";
    }

    void invalidHeader()
    {
        QFETCH(QByteArray, contents);
        QFETCH(QString, message);
        QTemporaryDir directory;
        QVERIFY(writeLog(directory.path(), contents));
        Parse_Log parser;
        QSignalSpy completed(&parser, &Parse_Log::open_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.count(), 1);
        QCOMPARE(completed.first().at(0).toBool(), false);
        QCOMPARE(completed.first().at(1).toString(), message);
        QSignalSpy cleared(&parser, &Parse_Log::clear_finished);
        parser.clear();
        QCOMPARE(cleared.count(), 1);
        QCOMPARE(cleared.first().at(0).toBool(), false);
        QCOMPARE(cleared.first().at(1).toString(), "Asked to clear without successfully parsing");
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void missingFile()
    {
        QTemporaryDir directory;
        Parse_Log parser;
        QSignalSpy completed(&parser, &Parse_Log::open_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.count(), 1);
        QCOMPARE(completed.first().at(0).toBool(), false);
        QCOMPARE(completed.first().at(1).toString(),
                 QString("Parsing failed - could not find a logfile at %1 to open").arg(directory.path()));
    }
};

QTEST_GUILESS_MAIN(ParseLogTest)
#include "test_parse_log.moc"
