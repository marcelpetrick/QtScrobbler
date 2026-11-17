// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2026 Marcel Petrick

#include "parse-ipod.h"

#include <QDataStream>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {
constexpr quint32 AppleEpoch = 2082844800U;
constexpr quint32 TrackTime = 1700000000U;

void append32(QByteArray &data, quint32 value)
{
    QDataStream stream(&data, QIODevice::WriteOnly | QIODevice::Append);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << value;
}

QByteArray block(const char *tag, quint32 size)
{
    QByteArray result(tag, 4);
    append32(result, size);
    result.resize(size, '\0');
    return result;
}

QByteArray metadata(quint32 type, const QString &text)
{
    QByteArray result("mhod", 4);
    append32(result, 24);
    append32(result, 40 + text.size() * 2);
    append32(result, type);
    result.resize(40, '\0');
    for (const QChar character : text) {
        QDataStream stream(&result, QIODevice::WriteOnly | QIODevice::Append);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << character.unicode();
    }
    return result;
}

void set32(QByteArray &data, qsizetype offset, quint32 value)
{
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.device()->seek(offset);
    stream << value;
}

QByteArray track(const QString &artist = "Demo Artist", const QString &title = "Demo Title",
                 const QString &album = "Demo Album", quint32 milliseconds = 210999, quint32 number = 7)
{
    QByteArray result = block("mhit", 48);
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.device()->seek(40);
    stream << milliseconds << number;
    result += metadata(MHOD_TITLE, title);
    result += metadata(MHOD_ALBUM, album);
    result += metadata(MHOD_ARTIST, artist);
    return result;
}

QByteArray payload(const QList<QByteArray> &tracks)
{
    QByteArray result("mhlt", 4);
    append32(result, 12);
    append32(result, tracks.size());
    for (const auto &item : tracks)
        result += item;
    return result;
}

QByteArray database(const QByteArray &body, bool compressed = false)
{
    const QByteArray contents = compressed ? qCompress(body).mid(4) : body;
    QByteArray result("mhbd", 4);
    append32(result, 12);
    append32(result, compressed ? contents.size() : contents.size() + 12);
    result += contents;
    return result;
}

QByteArray playCounts(const QList<quint32> &counts, quint32 date = AppleEpoch + TrackTime)
{
    QByteArray result("mhdp", 4);
    append32(result, 16);
    append32(result, 12);
    append32(result, counts.size());
    for (const auto count : counts) {
        append32(result, count);
        append32(result, date);
        append32(result, 123);
    }
    return result;
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

bool writeFixture(const QString &directory, const QByteArray &db, const QByteArray &counts,
                  bool compressed = false)
{
    const QString location = directory + "/iPod_Control/iTunes/";
    return QDir().mkpath(location)
        && writeFile(location + (compressed ? "iTunesCDB" : "iTunesDB"), db)
        && writeFile(location + "Play Counts", counts);
}
} // namespace

class ParseIpodTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QTest::failOnWarning(QRegularExpression(".*"));
    }

    void repeatedFormatChanges()
    {
        QTemporaryDir plainDirectory;
        QTemporaryDir compressedDirectory;
        QVERIFY(writeFixture(plainDirectory.path(), database(payload({track("Plain", "First")})),
                             playCounts({1})));
        QVERIFY(writeFixture(compressedDirectory.path(),
                             database(payload({track("Compressed", "Second")}), true),
                             playCounts({1}), true));
        Parse_Ipod parser;
        QSignalSpy completed(&parser, &Parse::open_finished);
        QList<scrob_entry> entries;
        connect(&parser, &Parse::entry, this, [&](scrob_entry item) { entries.append(item); });
        for (int iteration = 0; iteration < 6; ++iteration) {
            const bool compressed = iteration % 2 == 0;
            parser.open(compressed ? compressedDirectory.path() : plainDirectory.path(), 0);
            QCOMPARE(completed.size(), iteration + 1);
            QVERIFY(completed.last().at(0).toBool());
            QCOMPARE(entries.size(), iteration + 1);
            QCOMPARE(entries.last().artist, compressed ? "Compressed" : "Plain");
            QCOMPARE(entries.last().title, compressed ? "Second" : "First");
            QCOMPARE(entries.last().when, static_cast<int>(TrackTime));
        }
    }

    void validDatabase_data()
    {
        QTest::addColumn<bool>("compressed");
        QTest::addColumn<int>("offset");
        QTest::newRow("plain-utc") << false << 0;
        QTest::newRow("plain-positive-offset") << false << 3600;
        QTest::newRow("compressed-negative-offset") << true << -1800;
    }

    void validDatabase()
    {
        QFETCH(bool, compressed);
        QFETCH(int, offset);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto body = payload({track(QString::fromUtf8("Björk 示例"), "Demo Title", "Demo Album")});
        QVERIFY(writeFixture(directory.path(), database(body, compressed), playCounts({2}), compressed));
        Parse_Ipod parser;
        QList<scrob_entry> entries;
        connect(&parser, &Parse::entry, this, [&](scrob_entry item) { entries.append(item); });
        QSignalSpy completed(&parser, &Parse::open_finished);
        QSignalSpy cleared(&parser, &Parse::clear_finished);
        parser.open(directory.path(), offset);
        QCOMPARE(completed.size(), 1);
        QCOMPARE(completed.first().at(0).toBool(), true);
        QCOMPARE(completed.first().at(1).toString(), "Successfully parsed iTunesDB");
        QCOMPARE(parser.get_method(), SCROBBLE_IPOD);
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].artist, QString::fromUtf8("Björk 示例"));
        QCOMPARE(entries[0].title, "Demo Title");
        QCOMPARE(entries[0].album, "Demo Album");
        QCOMPARE(entries[0].length, 210);
        QCOMPARE(entries[0].tracknum, 7);
        QCOMPARE(entries[0].when, static_cast<int>(TrackTime) - offset);
        QCOMPARE(entries[0].played, QChar('L'));
        QCOMPARE(entries[0].mb_track_id, QString());
        for (const auto state : entries[0].sent)
            QCOMPARE(state, SENT_UNSENT);
        QCOMPARE(entries[1].when, 0);
        QCOMPARE(entries[1].title, entries[0].title);
        parser.clear();
        QCOMPARE(cleared.size(), 1);
        QVERIFY(cleared.first().at(0).toBool());
        QVERIFY(!QFile::exists(directory.filePath("iPod_Control/iTunes/Play Counts")));
        QVERIFY(QFile::exists(directory.filePath(compressed ? "iPod_Control/iTunes/iTunesCDB"
                                                          : "iPod_Control/iTunes/iTunesDB")));
        parser.clear();
        QCOMPARE(cleared.size(), 2);
        QCOMPARE(cleared.last().at(0).toBool(), false);
    }

    void skipUnplayedOrIncompleteTracks()
    {
        QTemporaryDir directory;
        const auto body = payload({track(), track("", "No Artist"), track("Artist", ""), track()});
        QVERIFY(writeFixture(directory.path(), database(body), playCounts({0, 2, 1, 1})));
        Parse_Ipod parser;
        QList<scrob_entry> entries;
        connect(&parser, &Parse::entry, this, [&](scrob_entry item) { entries.append(item); });
        QSignalSpy completed(&parser, &Parse::open_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 1);
        QVERIFY(completed.first().at(0).toBool());
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().title, "Demo Title");
        QCOMPARE(entries.first().when, static_cast<int>(TrackTime));
    }

    void optionalBlocksAndPlaylistEnd_data()
    {
        QTest::addColumn<QByteArray>("prefix");
        QTest::addColumn<QByteArray>("suffix");
        QTest::newRow("artist-list") << block("mhla", 12) << QByteArray();
        QByteArray artist = block("mhia", 16);
        set32(artist, 8, 16);
        QTest::newRow("artist-record") << artist << QByteArray();
        QTest::newRow("dataset") << block("mhsd", 16) << QByteArray();
        QTest::newRow("artwork-list") << block("mhli", 8) << QByteArray();
        QTest::newRow("artwork-record") << block("mhii", 8) << QByteArray();
        QTest::newRow("unknown-metadata") << QByteArray() << metadata(999, "discarded");
        QTest::newRow("playlist-list") << QByteArray() << QByteArray("mhlpJUNK");
        QTest::newRow("playlist") << QByteArray() << QByteArray("mhypJUNK");
        QTest::newRow("playlist-item") << QByteArray() << QByteArray("mhipJUNK");
    }

    void optionalBlocksAndPlaylistEnd()
    {
        QFETCH(QByteArray, prefix);
        QFETCH(QByteArray, suffix);
        QTemporaryDir directory;
        QVERIFY(writeFixture(directory.path(), database(prefix + payload({track()}) + suffix), playCounts({1})));
        Parse_Ipod parser;
        QSignalSpy completed(&parser, &Parse::open_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 1);
        QVERIFY(completed.first().at(0).toBool());
    }

    void invalidDatabase_data()
    {
        QTest::addColumn<QByteArray>("db");
        QTest::addColumn<QByteArray>("counts");
        QTest::addColumn<bool>("compressed");
        QTest::addColumn<QString>("errorPart");
        const auto valid = database(payload({track()}));
        QTest::newRow("wrong-db-magic") << QByteArray("NOT-AN-IPOD-DATABASE") << playCounts({1})
                                       << false << "not an iTunesDB file";
        QTest::newRow("wrong-compressed-magic") << QByteArray("NOT-AN-IPOD-DATABASE") << playCounts({1})
                                               << true << "not an iTunesCDB file";
        QTest::newRow("unknown-block") << database(QByteArray("evil")) << playCounts({1})
                                      << false << "Unknown blocktype: evil";
        QTest::newRow("wrong-playcount-magic") << valid << QByteArray("NOPE0123456789012345")
                                              << false << "invalid Play Counts file";
        QTest::newRow("zero-songs") << valid << playCounts({}) << false << "No tracks have been played";
        QTest::newRow("count-mismatch") << valid << playCounts({1, 1})
                                       << false << "Different number of songs";
        QTest::newRow("no-plays") << valid << playCounts({0})
                                 << false << "no entries were found";
    }

    void invalidDatabase()
    {
        QFETCH(QByteArray, db);
        QFETCH(QByteArray, counts);
        QFETCH(bool, compressed);
        QFETCH(QString, errorPart);
        QTemporaryDir directory;
        QVERIFY(writeFixture(directory.path(), db, counts, compressed));
        Parse_Ipod parser;
        QSignalSpy completed(&parser, &Parse::open_finished);
        QSignalSpy cleared(&parser, &Parse::clear_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 1);
        QCOMPARE(completed.first().at(0).toBool(), false);
        QVERIFY2(completed.first().at(1).toString().contains(errorPart),
                 qPrintable(completed.first().at(1).toString()));
        parser.clear();
        QCOMPARE(cleared.size(), 1);
        QCOMPARE(cleared.first().at(0).toBool(), false);
        QCOMPARE(cleared.first().at(1).toString(), "Asked to clear without successfully parsing");
        QVERIFY(QFile::exists(directory.filePath("iPod_Control/iTunes/Play Counts")));
    }

    void missingFiles()
    {
        QTemporaryDir directory;
        Parse_Ipod parser;
        QSignalSpy completed(&parser, &Parse::open_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 1);
        QVERIFY(!completed.first().at(0).toBool());
        QVERIFY(completed.first().at(1).toString().contains("unable to open iTunesDB"));
        const QString location = directory.filePath("iPod_Control/iTunes");
        QVERIFY(QDir().mkpath(location));
        QVERIFY(writeFile(location + "/iTunesDB", database(payload({track()}))));
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 2);
        QVERIFY(!completed.last().at(0).toBool());
        QVERIFY(completed.last().at(1).toString().contains("unable to open Play Counts"));
    }

    void malformedInput_data()
    {
        QTest::addColumn<QByteArray>("db");
        QTest::addColumn<QByteArray>("counts");
        QTest::addColumn<bool>("compressed");
        QTest::addColumn<QString>("errorPart");
        const auto goodCounts = playCounts({1});
        const auto goodPayload = payload({track()});
        const auto goodDb = database(goodPayload);
        const auto add = [&](const char *name, const QByteArray &body) {
            QTest::newRow(name) << database(body) << goodCounts << false << QString("iTunesDB");
        };
        for (const auto size : {0, 7, 0x7fffffff}) {
            QByteArray db = goodDb;
            set32(db, 4, size);
            const QByteArray name = "plain-header-" + QByteArray::number(size);
            QTest::newRow(name.constData()) << db << goodCounts << false << QString("invalid iTunesDB header");
        }
        for (const auto size : {0, 11, 0x7fffffff}) {
            QByteArray db = database(goodPayload, true);
            set32(db, 4, size);
            const QByteArray name = "compressed-header-" + QByteArray::number(size);
            QTest::newRow(name.constData()) << db << goodCounts << true << QString("invalid iTunesCDB header");
        }
        QByteArray truncatedHeader("mhbd", 4);
        QTest::newRow("plain-truncated-header") << truncatedHeader << goodCounts << false
                                              << QString("invalid iTunesDB header");
        QTest::newRow("compressed-truncated-header") << truncatedHeader << goodCounts << true
                                                   << QString("invalid iTunesCDB header");
        QByteArray corruptCompressed("mhbd", 4);
        append32(corruptCompressed, 12);
        append32(corruptCompressed, 5);
        corruptCompressed += "nope!";
        QTest::newRow("corrupt-compressed") << corruptCompressed << goodCounts << true
                                          << QString("invalid compressed iTunesCDB data");
        QByteArray compressedTruncated = database(goodPayload, true);
        compressedTruncated.chop(3);
        QTest::newRow("truncated-compressed") << compressedTruncated << goodCounts << true
                                            << QString("invalid compressed iTunesCDB data");
        QTest::newRow("empty-compressed") << database(QByteArray(), true) << goodCounts << true
                                        << QString("invalid compressed iTunesCDB data");
        add("empty-body", QByteArray());
        add("partial-block-type", goodPayload + "mh");
        add("track-without-list", track());
        add("short-track-header", payload({block("mhit", 47)}));
        add("metadata-without-track", payload({}) + metadata(MHOD_TITLE, "orphan"));
        QByteArray badTrack = track();
        set32(badTrack, 44, 0xffffffff);
        add("overflow-track-number", payload({badTrack}));
        add("truncated-track", payload({track().left(47)}));
        QByteArray badList = goodPayload;
        set32(badList, 4, 11);
        add("short-track-list", badList);
        badList = goodPayload;
        set32(badList, 8, 2);
        add("track-list-count-mismatch", badList);
        add("artist-header-truncated", QByteArray("mhla"));
        add("artist-header-short", block("mhla", 8).replace(4, 4, QByteArray(4, '\0')));
        add("artist-record-truncated", QByteArray("mhia"));
        badList = block("mhia", 12);
        set32(badList, 8, 11);
        add("artist-record-short", badList);
        add("dataset-short", block("mhsd", 15));
        add("artwork-short", QByteArray("mhii") + QByteArray(4, '\0'));
        add("metadata-missing-length", payload({}) + QByteArray("mhod"));
        add("metadata-missing-type", payload({track().left(48)}) + QByteArray("mhod0000"));
        for (const auto size : {15, 39, 41, 0x7fffffff}) {
            QByteArray item = metadata(MHOD_TITLE, "x");
            set32(item, 8, size);
            const QByteArray name = "metadata-size-" + QByteArray::number(size);
            add(name.constData(), payload({track().left(48) + item}));
        }
        const auto addCounts = [&](const char *name, QByteArray counts) {
            QTest::newRow(name) << goodDb << counts << false << QString("invalid Play Counts");
        };
        for (const auto size : {0, 15, 0x7fffffff}) {
            QByteArray counts = goodCounts;
            set32(counts, 4, size);
            const QByteArray name = "counts-header-" + QByteArray::number(size);
            addCounts(name.constData(), counts);
        }
        for (const auto size : {0, 11, 0x7fffffff}) {
            QByteArray counts = goodCounts;
            set32(counts, 8, size);
            const QByteArray name = "counts-record-" + QByteArray::number(size);
            addCounts(name.constData(), counts);
        }
        QByteArray counts = goodCounts;
        set32(counts, 12, 0xffffffff);
        addCounts("negative-song-count", counts);
        QTest::newRow("excessive-play-count") << goodDb << playCounts({1000001}) << false
                                            << QString("excessive iPod play count");
        QTest::newRow("excessive-total-play-count") << database(payload({track(), track()}))
                                                  << playCounts({500001, 500000}) << false
                                                  << QString("excessive iPod play count");
    }

    void malformedInput()
    {
        QFETCH(QByteArray, db);
        QFETCH(QByteArray, counts);
        QFETCH(bool, compressed);
        QFETCH(QString, errorPart);
        QTemporaryDir directory;
        QVERIFY(writeFixture(directory.path(), db, counts, compressed));
        Parse_Ipod parser;
        QSignalSpy completed(&parser, &Parse::open_finished);
        int emitted = 0;
        connect(&parser, &Parse::entry, this, [&](scrob_entry) { ++emitted; });
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 1);
        QCOMPARE(completed.first().at(0).toBool(), false);
        QVERIFY2(completed.first().at(1).toString().contains(errorPart),
                 qPrintable(completed.first().at(1).toString()));
        QCOMPARE(emitted, 0);
    }

    void truncationSweep()
    {
        const QByteArray goodDb = database(payload({track()}));
        const QByteArray goodCounts = playCounts({1});
        QTemporaryDir directory;
        for (int input = 0; input < 2; ++input) {
            const int limit = input == 0 ? goodDb.size() : goodCounts.size();
            for (int length = 0; length < limit; ++length) {
                QVERIFY(writeFixture(directory.path(), input == 0 ? goodDb.left(length) : goodDb,
                                     input == 1 ? goodCounts.left(length) : goodCounts));
                Parse_Ipod parser;
                QSignalSpy completed(&parser, &Parse::open_finished);
                int emitted = 0;
                connect(&parser, &Parse::entry, this, [&](scrob_entry) { ++emitted; });
                parser.open(directory.path(), 0);
                QCOMPARE(completed.size(), 1);
                QVERIFY2(!completed.first().at(0).toBool(), qPrintable(QString("accepted input %1 length %2")
                                                                       .arg(input).arg(length)));
                QCOMPARE(emitted, 0);
            }
        }
    }

    void extendedHeadersAndRecords()
    {
        QByteArray body = payload({track()});
        body.insert(12, QByteArray(4, '\0'));
        set32(body, 4, 16);
        QByteArray counts = playCounts({1});
        counts.insert(16, QByteArray(4, '\0'));
        counts.append(QByteArray(4, '\0'));
        set32(counts, 4, 20);
        set32(counts, 8, 16);
        QTemporaryDir directory;
        QVERIFY(writeFixture(directory.path(), database(body), counts));
        Parse_Ipod parser;
        QList<scrob_entry> entries;
        connect(&parser, &Parse::entry, this, [&](scrob_entry item) { entries.append(item); });
        QSignalSpy completed(&parser, &Parse::open_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 1);
        QVERIFY(completed.first().at(0).toBool());
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].when, static_cast<int>(TrackTime));
    }

    void highlyCompressedValidLibrary()
    {
        const QString title(500000, QChar('x'));
        const QByteArray body = payload({track("Artist", title)});
        const QByteArray db = database(body, true);
        QVERIFY(body.size() > db.size() * 100);
        QTemporaryDir directory;
        QVERIFY(writeFixture(directory.path(), db, playCounts({1}), true));
        Parse_Ipod parser;
        QList<scrob_entry> entries;
        connect(&parser, &Parse::entry, this, [&](scrob_entry item) { entries.append(item); });
        QSignalSpy completed(&parser, &Parse::open_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 1);
        QVERIFY(completed.first().at(0).toBool());
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].title, title);
    }

    void excessiveDecodedSize()
    {
        // The encoded stream is tiny; the decoder must stop as soon as output
        // would exceed the documented 256 MiB body cap.
        QByteArray db;
        {
            const QByteArray excessive(256 * 1024 * 1024 + 1, 'x');
            db = database(excessive, true);
        }
        QTemporaryDir directory;
        QVERIFY(writeFixture(directory.path(), db, playCounts({1}), true));
        Parse_Ipod parser;
        QSignalSpy completed(&parser, &Parse::open_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 1);
        QVERIFY(!completed.first().at(0).toBool());
        QVERIFY(completed.first().at(1).toString().contains("decompressed iTunesCDB exceeds 256 MiB"));
    }

    void excessiveFileSize()
    {
        QTemporaryDir directory;
        QVERIFY(writeFixture(directory.path(), database(payload({track()})), playCounts({1})));
        QFile file(directory.filePath("iPod_Control/iTunes/iTunesDB"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(256 * 1024 * 1024 + 1)); // Sparse file; no large fixture committed.
        file.close();
        Parse_Ipod parser;
        QSignalSpy completed(&parser, &Parse::open_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 1);
        QVERIFY(!completed.first().at(0).toBool());
        QVERIFY(completed.first().at(1).toString().contains("iTunesDB file exceeds 256 MiB"));
    }

    void clearFailureCanBeRetriedSafely()
    {
        QTemporaryDir directory;
        QVERIFY(writeFixture(directory.path(), database(payload({track()})), playCounts({1})));
        Parse_Ipod parser;
        QSignalSpy completed(&parser, &Parse::open_finished);
        QSignalSpy cleared(&parser, &Parse::clear_finished);
        parser.open(directory.path(), 0);
        QVERIFY(completed.first().at(0).toBool());
        const QString path = directory.filePath("iPod_Control/iTunes/Play Counts");
        QVERIFY(QFile::remove(path));
        QVERIFY(QDir().mkdir(path)); // A directory cannot be removed by QFile::remove().
        parser.clear();
        QCOMPARE(cleared.size(), 1);
        QVERIFY(!cleared.first().at(0).toBool());
        QVERIFY(cleared.first().at(1).toString().contains("Failed to remove log file"));
        QVERIFY(QDir().rmdir(path));
        QVERIFY(writeFile(path, playCounts({1})));
        parser.clear();
        QCOMPARE(cleared.size(), 2);
        QVERIFY(cleared.last().at(0).toBool());
    }

    void truncatedPlayCountsMustNotEmitTracks()
    {
        QTemporaryDir directory;
        QByteArray truncated = playCounts({1});
        truncated.chop(8); // A count without the required timestamp or bookmark.
        QVERIFY(writeFixture(directory.path(), database(payload({track()})), truncated));
        Parse_Ipod parser;
        QList<scrob_entry> entries;
        connect(&parser, &Parse::entry, this, [&](scrob_entry item) { entries.append(item); });
        QSignalSpy completed(&parser, &Parse::open_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 1);
        QCOMPARE(completed.first().at(0).toBool(), false);
        QCOMPARE(entries.size(), 0);
    }

    void failedReopenMustNotPermitClear()
    {
        QTemporaryDir directory;
        QVERIFY(writeFixture(directory.path(), database(payload({track()})), playCounts({1})));
        Parse_Ipod parser;
        QSignalSpy completed(&parser, &Parse::open_finished);
        QSignalSpy cleared(&parser, &Parse::clear_finished);
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 1);
        QVERIFY(completed.first().at(0).toBool());
        const QString location = directory.filePath("iPod_Control/iTunes");
        QVERIFY(writeFile(location + "/iTunesDB", "invalid-database"));
        parser.open(directory.path(), 0);
        QCOMPARE(completed.size(), 2);
        QVERIFY(!completed.last().at(0).toBool());
        parser.clear();
        QCOMPARE(cleared.size(), 1);
        QCOMPARE(cleared.first().at(0).toBool(), false);
        QVERIFY(QFile::exists(location + "/Play Counts"));
    }
};

QTEST_GUILESS_MAIN(ParseIpodTest)
#include "test_parse_ipod.moc"
