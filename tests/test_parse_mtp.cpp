// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2026 Marcel Petrick
#include <QSignalSpy>
#include <QtTest>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "parse-mtp.h"

namespace {
struct Track {
    const char* artist = "Björk 示例";
    const char* title = "Test Title";
    const char* album = "Test Album";
    uint32_t count = 2;
    uint32_t id = 7;
};

struct FakeMtp {
    LIBMTP_mtpdevice_t devices[2]{};
    LIBMTP_error_t error{};
    LIBMTP_error_number_t discovery = LIBMTP_ERROR_NONE;
    QList<Track> tracks[2]{{Track{}}, {Track{}}};
    bool secondDevice = false;
    bool nullDevice = false;
    bool partialDiscovery = false;
    bool named = true;
    int readFailureDevice = -1;
    int updateFailure = -1;
    int initialized = 0;
    int released = 0;
    int destroyed = 0;
    int listings = 0;
    int errorsCleared = 0;
    bool wrongArguments = false;
    QList<uint32_t> updated;

    Parse_MTP::Backend backend() {
        Parse_MTP::Backend result;
        result.init = [&] { ++initialized; };
        result.connected = [&](LIBMTP_mtpdevice_t** output) {
            devices[0].next = secondDevice ? &devices[1] : nullptr;
            *output = !nullDevice && (discovery == LIBMTP_ERROR_NONE || partialDiscovery) ? devices : nullptr;
            return discovery;
        };
        result.release = [&](LIBMTP_mtpdevice_t* input) {
            wrongArguments |= input != devices;
            ++released;
        };
        result.friendlyName = [&](LIBMTP_mtpdevice_t*) -> char* {
            return named ? strdup("Fictional Player") : nullptr;
        };
        result.clearErrors = [&](LIBMTP_mtpdevice_t* device) {
            ++errorsCleared;
            device->errorstack = nullptr;
        };
        result.errors = [](LIBMTP_mtpdevice_t* device) { return device->errorstack; };
        result.listing = [&](LIBMTP_mtpdevice_t* device, LIBMTP_progressfunc_t callback, const void* data) {
            wrongArguments |= callback != nullptr || data != nullptr;
            ++listings;
            const int index = device == devices ? 0 : 1;
            if (index == readFailureDevice) device->errorstack = &error;
            LIBMTP_track_t* first = nullptr;
            LIBMTP_track_t** next = &first;
            for (const auto& spec : tracks[index]) {
                auto* track = LIBMTP_new_track_t();
                track->artist = spec.artist ? strdup(spec.artist) : nullptr;
                track->title = spec.title ? strdup(spec.title) : nullptr;
                track->album = spec.album ? strdup(spec.album) : nullptr;
                track->usecount = spec.count;
                track->tracknumber = 9;
                track->duration = 180999;
                track->item_id = spec.id;
                *next = track;
                next = &track->next;
            }
            return first;
        };
        result.destroyTrack = [&](LIBMTP_track_t* track) {
            ++destroyed;
            LIBMTP_destroy_track_t(track);
        };
        result.update = [&](LIBMTP_mtpdevice_t* device, const LIBMTP_track_t* track) {
            wrongArguments |= (device != devices && device != devices + 1) || track->usecount != 0;
            updated.append(track->item_id);
            return updated.size() == updateFailure ? -1 : 0;
        };
        return result;
    }
};
}  // namespace

class ParseMtpTest : public QObject {
    Q_OBJECT
   private slots:
    void init() { QTest::failOnWarning(QRegularExpression(".*")); }

    void defaultsRetainRealAdapter() {
        // Initialize the real library without opening or discovering devices.
        Parse_MTP parser;
        QCOMPARE(parser.get_method(), SCROBBLE_MTP);
        // Verify discovery/update remain connected to the real adapter symbols.
        const Parse_MTP::Backend backend;
        QVERIFY(backend.init.target<decltype(&LIBMTP_Init)>());
        QVERIFY(*backend.init.target<decltype(&LIBMTP_Init)>() == &LIBMTP_Init);
        QVERIFY(backend.connected.target<decltype(&LIBMTP_Get_Connected_Devices)>());
        QVERIFY(*backend.connected.target<decltype(&LIBMTP_Get_Connected_Devices)>() == &LIBMTP_Get_Connected_Devices);
        QVERIFY(backend.update.target<decltype(&LIBMTP_Update_Track_Metadata)>());
        QVERIFY(*backend.update.target<decltype(&LIBMTP_Update_Track_Metadata)>() == &LIBMTP_Update_Track_Metadata);
    }

    void discoveryErrors_data() {
        QTest::addColumn<int>("code");
        QTest::addColumn<QString>("message");
        QTest::newRow("no-device") << int(LIBMTP_ERROR_NO_DEVICE_ATTACHED) << "MTP: No devices have been found";
        QTest::newRow("connection") << int(LIBMTP_ERROR_CONNECTING) << "MTP: Error connecting.";
        QTest::newRow("allocation") << int(LIBMTP_ERROR_MEMORY_ALLOCATION) << "MTP: Memory Allocation Error.";
        QTest::newRow("general") << int(LIBMTP_ERROR_GENERAL) << "MTP: Unknown error!";
        QTest::newRow("other") << int(LIBMTP_ERROR_USB_LAYER) << "MTP: Unknown error!";
        QTest::newRow("success-without-list") << int(LIBMTP_ERROR_NONE) << "MTP: Unknown error!";
    }

    void discoveryErrors() {
        QFETCH(int, code);
        QFETCH(QString, message);
        FakeMtp fake;
        fake.discovery = LIBMTP_error_number_t(code);
        fake.nullDevice = code == LIBMTP_ERROR_NONE;
        Parse_MTP parser(fake.backend());
        QSignalSpy completed(&parser, &Parse::open_finished);
        QSignalSpy cleared(&parser, &Parse::clear_finished);
        parser.open("ignored", 0);
        QCOMPARE(completed.size(), 1);
        QCOMPARE(completed[0][0].toBool(), false);
        QCOMPARE(completed[0][1].toString(), message);
        QCOMPARE(fake.listings, 0);
        QCOMPARE(fake.initialized, 1);
        parser.clear();
        QCOMPARE(cleared.size(), 1);
        QCOMPARE(cleared[0][0].toBool(), false);
        QCOMPARE(cleared[0][1].toString(), "Asked to clear without successfully parsing");
    }

    void importsExactFieldsAndFreesEveryTrack() {
        FakeMtp fake;
        fake.secondDevice = true;
        fake.tracks[0] = {Track{},
                          {nullptr, "Missing Artist", nullptr, 2, 8},
                          {"Artist", nullptr, nullptr, 1, 9},
                          {"Artist", "Unplayed", "", 0, 10}};
        fake.tracks[1] = {{"Other Artist", "Other Title", nullptr, 1, 11}};
        QList<scrob_entry> entries;
        {
            Parse_MTP parser(fake.backend());
            connect(&parser, &Parse::entry, this, [&](scrob_entry item) { entries.append(item); });
            QSignalSpy completed(&parser, &Parse::open_finished);
            parser.open("ignored", 3600);
            QCOMPARE(parser.get_method(), SCROBBLE_MTP);
            QCOMPARE(completed.size(), 1);
            QCOMPARE(completed[0][0].toBool(), true);
            QCOMPARE(completed[0][1].toString(), "MTP: Finished parsing tracks");
            QCOMPARE(fake.destroyed, 5);
            QCOMPARE(fake.listings, 2);
            QCOMPARE(fake.errorsCleared, 2);
            QCOMPARE(fake.released, 0);
        }
        QCOMPARE(fake.released, 1);
        QCOMPARE(entries.size(), 3);
        for (int index = 0; index < 2; ++index) {
            const auto& entry = entries[index];
            QCOMPARE(entry.artist, QString::fromUtf8("Björk 示例"));
            QCOMPARE(entry.title, "Test Title");
            QCOMPARE(entry.album, "Test Album");
            QCOMPARE(entry.tracknum, 9);
            QCOMPARE(entry.length, 180);
            QCOMPARE(entry.when, 0);
            QCOMPARE(entry.played, QChar('L'));
            QVERIFY(entry.mb_track_id.isEmpty());
            for (const auto sent : entry.sent) QCOMPARE(sent, SENT_UNSENT);
        }
        QCOMPARE(entries[2].artist, "Other Artist");
        QCOMPARE(entries[2].title, "Other Title");
        QVERIFY(entries[2].album.isEmpty());
        QVERIFY(!fake.wrongArguments);
    }

    void emptyDeviceAndUnnamedPlayer() {
        FakeMtp fake;
        fake.tracks[0].clear();
        fake.named = false;
        Parse_MTP parser(fake.backend());
        QSignalSpy completed(&parser, &Parse::open_finished);
        QSignalSpy cleared(&parser, &Parse::clear_finished);
        parser.open("", 0);
        QCOMPARE(completed.size(), 1);
        QVERIFY(completed[0][0].toBool());
        parser.clear();
        QCOMPARE(cleared.size(), 1);
        QVERIFY(cleared[0][0].toBool());
        QCOMPARE(fake.listings, 2);
        QCOMPARE(fake.destroyed, 0);
        QVERIFY(fake.updated.isEmpty());
    }

    void readErrorRejectsPartialImport_data() {
        QTest::addColumn<bool>("partialListing");
        QTest::newRow("empty-error") << false;
        QTest::newRow("partial-error") << true;
    }

    void readErrorRejectsPartialImport() {
        QFETCH(bool, partialListing);
        FakeMtp fake;
        fake.secondDevice = true;
        fake.readFailureDevice = 1;
        if (!partialListing) fake.tracks[1].clear();
        Parse_MTP parser(fake.backend());
        int emitted = 0;
        connect(&parser, &Parse::entry, this, [&](scrob_entry) { ++emitted; });
        QSignalSpy completed(&parser, &Parse::open_finished);
        QSignalSpy cleared(&parser, &Parse::clear_finished);
        parser.open("", 0);
        QCOMPARE(completed.size(), 1);
        QVERIFY(!completed[0][0].toBool());
        QCOMPARE(completed[0][1].toString(), "MTP: Failed to read tracks");
        QCOMPARE(emitted, 0);
        QCOMPARE(fake.destroyed, partialListing ? 2 : 1);
        parser.clear();
        QVERIFY(!cleared[0][0].toBool());
        QVERIFY(fake.updated.isEmpty());
    }

    void clearsOnlyPlayedTracksAndReportsFailure() {
        FakeMtp fake;
        fake.tracks[0] = {
            Track{}, {"Artist", "Unplayed", "", 0, 8}, {nullptr, "Missing", "", 1, 9}, {"Artist", "Other", "", 1, 10}};
        Parse_MTP parser(fake.backend());
        QSignalSpy completed(&parser, &Parse::open_finished);
        QSignalSpy cleared(&parser, &Parse::clear_finished);
        parser.open("", 0);
        QVERIFY(completed[0][0].toBool());
        fake.updateFailure = 1;
        parser.clear();
        QCOMPARE(cleared.size(), 1);
        QVERIFY(!cleared[0][0].toBool());
        QCOMPARE(cleared[0][1].toString(), "MTP: Failed to clear tracks");
        QCOMPARE(fake.updated, QList<uint32_t>{7});
        QCOMPARE(fake.destroyed, 8);
        fake.updateFailure = -1;
        parser.clear();
        QCOMPARE(cleared.size(), 2);
        QVERIFY(cleared[1][0].toBool());
        QCOMPARE(cleared[1][1].toString(), "MTP: Finished clearing tracks");
        QCOMPARE(fake.updated, (QList<uint32_t>{7, 7, 10}));
        QCOMPARE(fake.destroyed, 12);
        parser.clear();
        QCOMPARE(cleared.size(), 3);
        QVERIFY(!cleared[2][0].toBool());
        QCOMPARE(fake.listings, 3);
        QVERIFY(!fake.wrongArguments);
    }

    void clearReadFailure() {
        FakeMtp fake;
        Parse_MTP parser(fake.backend());
        parser.open("", 0);
        fake.readFailureDevice = 0;
        QSignalSpy cleared(&parser, &Parse::clear_finished);
        parser.clear();
        QCOMPARE(cleared.size(), 1);
        QVERIFY(!cleared[0][0].toBool());
        QVERIFY(fake.updated.isEmpty());
        QCOMPARE(fake.destroyed, 2);
    }

    void reopenReleasesListsAndInvalidatesClear() {
        FakeMtp fake;
        {
            Parse_MTP parser(fake.backend());
            QSignalSpy completed(&parser, &Parse::open_finished);
            for (int index = 0; index < 4; ++index) {
                parser.open("", 0);
                QCOMPARE(completed.size(), index + 1);
                QVERIFY(completed.last()[0].toBool());
                QCOMPARE(fake.released, index);
            }
            fake.discovery = LIBMTP_ERROR_CONNECTING;
            fake.partialDiscovery = true;
            parser.open("", 0);
            QVERIFY(!completed.last()[0].toBool());
            QCOMPARE(fake.released, 5);
            QSignalSpy cleared(&parser, &Parse::clear_finished);
            parser.clear();
            QVERIFY(!cleared[0][0].toBool());
        }
        QCOMPARE(fake.released, 5);
        QVERIFY(!fake.wrongArguments);
    }

    void excessiveCountsAreRejectedBeforeEmission() {
        FakeMtp fake;
        fake.tracks[0] = {{"Artist", "Title", "", std::numeric_limits<uint32_t>::max(), 1}};
        Parse_MTP parser(fake.backend());
        int emitted = 0;
        connect(&parser, &Parse::entry, this, [&](scrob_entry) { ++emitted; });
        QSignalSpy completed(&parser, &Parse::open_finished);
        parser.open("", 0);
        QCOMPARE(completed.size(), 1);
        QVERIFY(!completed[0][0].toBool());
        QCOMPARE(emitted, 0);
        QCOMPARE(fake.destroyed, 1);
    }
};

QTEST_GUILESS_MAIN(ParseMtpTest)
#include "test_parse_mtp.moc"
