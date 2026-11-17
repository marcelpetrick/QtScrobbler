#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "parse-log.h"

class ParseLogTest : public QObject {
    Q_OBJECT

    static bool writeLog(const QString& directory, const QByteArray& contents, const QString& name = ".scrobbler.log") {
        QFile file(directory + '/' + name);
        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
    }

   private slots:
    void init() { QTest::failOnWarning(QRegularExpression(".*")); }

    void example() {
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
        for (const auto& entry : entries)
            for (const auto state : entry.sent) QCOMPARE(state, SENT_UNSENT);
        QCOMPARE(entries[0].artist, "Example Artist");
        QCOMPARE(entries[0].album, "Example Album");
        QCOMPARE(entries[0].title, "Morning Signal");
        QCOMPARE(entries[0].tracknum, 1);
        QCOMPARE(entries[0].length, 210);
        QCOMPARE(entries[0].played, QChar('L'));
        QCOMPARE(entries[0].when, 1794902400);  // #TZ/UTC overrides supplied offset.
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

    void versionAndTimezone_data() {
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

    void versionAndTimezone() {
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

    void invalidHeader_data() {
        QTest::addColumn<QByteArray>("contents");
        QTest::addColumn<QString>("message");
        QTest::newRow("short") << QByteArray("bad\n") << "Parsing failed - log file is empty or corrupt";
        QTest::newRow("version") << QByteArray("#AUDIOSCROBBLER/9.0\n#TZ/UTC\n#CLIENT/test\n")
                                 << "Unknown log version: 9.0";
        QTest::newRow("format-prefix-without-version") << QByteArray("#AUDIOSCROBBLER/\n#TZ/UTC\n#CLIENT/test\n")
                                                       << "Parsing failed - missing #AUDIOSCROBBLER header";
        QTest::newRow("timezone-prefix-without-value")
            << QByteArray("#AUDIOSCROBBLER/1.0\n#TZ/\n#CLIENT/test\n") << "Parsing failed - missing #TZ header";
        QTest::newRow("client-prefix-without-value")
            << QByteArray("#AUDIOSCROBBLER/1.0\n#TZ/UTC\n#CLIENT/\n") << "Parsing failed - missing #CLIENT header";
        QTest::newRow("missing-format") << QByteArray("wrong\n#TZ/UTC\n#CLIENT/test\n")
                                        << "Parsing failed - missing #AUDIOSCROBBLER header";
        QTest::newRow("unknown-tz") << QByteArray("#AUDIOSCROBBLER/1.0\n#TZ/CET\n#CLIENT/test\n")
                                    << "Unknown log TZ: CET";
        QTest::newRow("missing-tz") << QByteArray("#AUDIOSCROBBLER/1.0\nwrong\n#CLIENT/test\n")
                                    << "Parsing failed - missing #TZ header";
        QTest::newRow("missing-client") << QByteArray("#AUDIOSCROBBLER/1.0\n#TZ/UTC\nwrong\n")
                                        << "Parsing failed - missing #CLIENT header";
    }

    void invalidHeader() {
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

    void noValidRecords() {
        QTemporaryDir directory;
        QVERIFY(writeLog(directory.path(), "#AUDIOSCROBBLER/1.0\n#TZ/UTC\n#CLIENT/test\n# comment\n"));
        Parse_Log parser;
        QSignalSpy completed(&parser, &Parse_Log::open_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.count(), 1);
        QCOMPARE(completed.first().at(0).toBool(), false);
        QCOMPARE(completed.first().at(1).toString(), "Parsing failed - no valid log entries");
        QSignalSpy cleared(&parser, &Parse_Log::clear_finished);
        parser.clear();
        QCOMPARE(cleared.count(), 1);
        QCOMPARE(cleared.first().at(0).toBool(), false);
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void failedReopenCannotClearSource() {
        QTemporaryDir validDirectory;
        QTemporaryDir invalidDirectory;
        QVERIFY(QFile::copy(EXAMPLE_LOG, validDirectory.filePath(".scrobbler.log")));
        QVERIFY(writeLog(invalidDirectory.path(), "bad\n"));
        Parse_Log parser;
        QSignalSpy completed(&parser, &Parse_Log::open_finished);
        parser.open(validDirectory.path(), 0);
        QCOMPARE(completed.count(), 1);
        QVERIFY(completed.first().at(0).toBool());
        parser.open(invalidDirectory.path(), 0);
        QCOMPARE(completed.count(), 2);
        QVERIFY(!completed.last().at(0).toBool());
        QSignalSpy cleared(&parser, &Parse_Log::clear_finished);
        parser.clear();
        QCOMPARE(cleared.count(), 1);
        QVERIFY(!cleared.first().at(0).toBool());
        QVERIFY(QFile::exists(validDirectory.filePath(".scrobbler.log")));
        QVERIFY(QFile::exists(invalidDirectory.filePath(".scrobbler.log")));
    }

    void malformedRowsAreSkippedWithoutLosingValidTracks() {
        QTemporaryDir directory;
        const QByteArray log =
            "#AUDIOSCROBBLER/1.1\n#TZ/UTC\n#CLIENT/fixture\n"
            "# A comment with\tseveral\ttabs\n\n"
            "missing columns\n"
            "Artist\tAlbum\tTitle\t2\t180\tL\t1700000000\n"  // Missing 1.1 MBID column.
            "Artist\tAlbum\tTitle\t2\t180\tL\t1700000000\textra\textra\n"
            "Björk 示例\t海辺\tValid Track\t0\t0\tS\t0\t\n"
            "Another Artist\t\tOther Track\t9\t123\tL\t1700000001\t"
            "11111111-2222-3333-4444-555555555555";  // Last row need not end with newline.
        QVERIFY(writeLog(directory.path(), log));
        Parse_Log parser;
        QList<scrob_entry> entries;
        connect(&parser, &Parse::entry, this, [&](scrob_entry item) { entries.append(item); });
        QSignalSpy completed(&parser, &Parse::open_finished);
        parser.open(directory.path(), 3600);
        QCOMPARE(completed.size(), 1);
        QVERIFY(completed[0][0].toBool());
        QCOMPARE(completed[0][1].toString(), "Successfully parsed 2 log entries");
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].artist, QString::fromUtf8("Björk 示例"));
        QCOMPARE(entries[0].album, QString::fromUtf8("海辺"));
        QCOMPARE(entries[0].title, "Valid Track");
        QCOMPARE(entries[0].tracknum, 0);
        QCOMPARE(entries[0].length, 0);
        QCOMPARE(entries[0].played, QChar('S'));
        QCOMPARE(entries[0].when, 0);
        QVERIFY(entries[0].mb_track_id.isEmpty());
        QCOMPARE(entries[1].artist, "Another Artist");
        QVERIFY(entries[1].album.isEmpty());
        QCOMPARE(entries[1].title, "Other Track");
        QCOMPARE(entries[1].tracknum, 9);
        QCOMPARE(entries[1].length, 123);
        QCOMPARE(entries[1].played, QChar('L'));
        QCOMPARE(entries[1].when, 1700000001);
        QCOMPARE(entries[1].mb_track_id, "11111111-2222-3333-4444-555555555555");
    }

    void emptyPlayedFlag_data() {
        QTest::addColumn<QByteArray>("version");
        QTest::addColumn<QByteArray>("suffix");
        QTest::newRow("version-1.0") << QByteArray("1.0") << QByteArray();
        QTest::newRow("version-1.1") << QByteArray("1.1") << QByteArray("\t");
    }

    void emptyPlayedFlag() {
        QFETCH(QByteArray, version);
        QFETCH(QByteArray, suffix);
        QTemporaryDir directory;
        QVERIFY(writeLog(directory.path(),
                         "#AUDIOSCROBBLER/" + version +
                             "\n#TZ/UTC\n#CLIENT/fixture\nArtist\tAlbum\tTitle\t2\t180\t\t1700000000" + suffix + '\n'));
        Parse_Log parser;
        int emitted = 0;
        connect(&parser, &Parse::entry, this, [&](scrob_entry) { ++emitted; });
        QSignalSpy completed(&parser, &Parse::open_finished);
        QSignalSpy cleared(&parser, &Parse::clear_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 1);
        QVERIFY(!completed[0][0].toBool());
        QCOMPARE(completed[0][1].toString(), "Parsing failed - no valid log entries");
        QCOMPARE(emitted, 0);
        parser.clear();
        QVERIFY(!cleared[0][0].toBool());
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
    }

    void unreadableFile() {
        QTemporaryDir directory;
        const QString file = directory.filePath(".scrobbler.log");
        QVERIFY(QDir().mkdir(file));
        Parse_Log parser;
        QSignalSpy completed(&parser, &Parse::open_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 1);
        QVERIFY(!completed[0][0].toBool());
        QCOMPARE(completed[0][1].toString(), "Parsing failed - unable to read file " + file);
    }

    void clearFailureCanBeRetried() {
        QTemporaryDir directory;
        const QString file = directory.filePath(".scrobbler.log");
        QVERIFY(QFile::copy(EXAMPLE_LOG, file));
        Parse_Log parser;
        QSignalSpy completed(&parser, &Parse::open_finished);
        QSignalSpy cleared(&parser, &Parse::clear_finished);
        parser.open(directory.path(), 0);
        QVERIFY(completed[0][0].toBool());
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkdir(file));
        parser.clear();
        QCOMPARE(cleared.size(), 1);
        QVERIFY(!cleared[0][0].toBool());
        QCOMPARE(cleared[0][1].toString(), "Failed to remove log file " + file);
        QVERIFY(QDir().rmdir(file));
        QVERIFY(QFile::copy(EXAMPLE_LOG, file));
        parser.clear();
        QCOMPARE(cleared.size(), 2);
        QVERIFY(cleared[1][0].toBool());
        QCOMPARE(cleared[1][1].toString(), "Deleted log file " + file);
        QVERIFY(!QFile::exists(file));
    }

    void successfulClearDoesNotAuthorizeDeletingNewLog() {
        QTemporaryDir directory;
        const QString file = directory.filePath(".scrobbler.log");
        QVERIFY(QFile::copy(EXAMPLE_LOG, file));
        Parse_Log parser;
        QSignalSpy cleared(&parser, &Parse::clear_finished);
        parser.open(directory.path(), 0);
        parser.clear();
        QCOMPARE(cleared.size(), 1);
        QVERIFY(cleared[0][0].toBool());
        QVERIFY(QFile::copy(EXAMPLE_LOG, file));
        parser.clear();
        QCOMPARE(cleared.size(), 2);
        QVERIFY(!cleared[1][0].toBool());
        QCOMPARE(cleared[1][1].toString(), "Asked to clear without successfully parsing");
        QVERIFY(QFile::exists(file));
    }

    void missingFile() {
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
