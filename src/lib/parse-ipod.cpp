/***************************************************************************
 *   Copyright (C) 2006-2009 by Tomasz Moń                                 *
 *   Copyright (C) 2006-2010 by Robert Keevil                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; under version 2 of the License.         *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "parse-ipod.h"

#include <zlib.h>

#include <limits>

namespace {
// Bound untrusted file reads and decompression independently. The decoded body
// cap excludes the database header and accommodates large legacy music libraries.
constexpr qsizetype MaximumDatabaseBytes = 256 * 1024 * 1024;
}  // namespace

Parse_Ipod::Parse_Ipod() : entries(0), playcounts(0), tracks(0), playcounts_file(QString()) { compressed = false; }

Parse_Ipod::~Parse_Ipod() {}

void Parse_Ipod::open(QString folder_path, int tz) {
    parsed = false;
    playcounts_file.clear();
    compressed = false;
    // ensure we set compressed if needed
    CHECK_RESULT chk = check_path(SCROBBLE_IPOD, folder_path);
    if (chk == CHECK_ITUNES_COMPRESSED) compressed = true;

    QString db_filename = folder_path;
    if (compressed)
        db_filename += "/iPod_Control/iTunes/iTunesCDB";
    else
        db_filename += "/iPod_Control/iTunes/iTunesDB";

    QFile input_file(db_filename);

    if (!input_file.open(QIODevice::ReadOnly)) {
        emit open_finished(false, tr("Parsing failed - unable to open iTunesDB at %1").arg(db_filename));
        return;
    }
    if (input_file.size() > MaximumDatabaseBytes) {
        emit open_finished(false, tr("Parsing failed - iTunesDB file exceeds 256 MiB"));
        return;
    }
    QByteArray data = input_file.readAll();
    input_file.close();

    if (compressed)
        open_compressed(folder_path, tz, data);
    else
        open_plain(folder_path, tz, data);
}

void Parse_Ipod::open_compressed(QString folder_path, int tz, QByteArray& data) {
    QDataStream header(data);
    header.setByteOrder(QDataStream::LittleEndian);
    quint32 magic = 0;
    quint32 headersize = 0;
    quint32 compressedsize = 0;
    header >> magic;
    if (magic != 0x6462686d)  // mhbd
    {
        emit open_finished(false, tr("Parsing failed - not an iTunesCDB file"));
        return;
    }
    header >> headersize >> compressedsize;
    if (header.status() != QDataStream::Ok || headersize < 12 || headersize > quint64(data.size())) {
        emit open_finished(false, tr("Parsing failed - invalid iTunesCDB header"));
        return;
    }

    z_stream compressed_stream{};
    compressed_stream.next_in = reinterpret_cast<Bytef*>(data.data() + headersize);
    compressed_stream.avail_in = data.size() - headersize;
    if (inflateInit(&compressed_stream) != Z_OK) {
        emit open_finished(false, tr("Parsing failed - unable to initialize iTunesCDB decompression"));
        return;
    }
    QByteArray plain;
    char buffer[64 * 1024];
    int result = Z_OK;
    do {
        compressed_stream.next_out = reinterpret_cast<Bytef*>(buffer);
        compressed_stream.avail_out = sizeof(buffer);
        result = inflate(&compressed_stream, Z_NO_FLUSH);
        const qsizetype decoded = sizeof(buffer) - compressed_stream.avail_out;
        if (decoded > MaximumDatabaseBytes - plain.size()) {
            inflateEnd(&compressed_stream);
            emit open_finished(false, tr("Parsing failed - decompressed iTunesCDB exceeds 256 MiB"));
            return;
        }
        plain.append(buffer, decoded);
    } while (result == Z_OK);
    inflateEnd(&compressed_stream);
    if (result != Z_STREAM_END || plain.isEmpty()) {
        emit open_finished(false, tr("Parsing failed - invalid compressed iTunesCDB data"));
        return;
    }
    parse_db(folder_path, tz, plain);
}

void Parse_Ipod::open_plain(QString folder_path, int tz, QByteArray& data) {
    // read the info we want from the header, and then discard it
    quint32 type = 0;
    quint32 headerlen = 0;

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> type;
    if (type != 0x6462686D) {
        emit open_finished(false, tr("Parsing failed - not an iTunesDB file"));
        return;
    }

    stream >> headerlen;
    if (stream.status() != QDataStream::Ok || headerlen < 8 || headerlen > quint64(data.size())) {
        emit open_finished(false, tr("Parsing failed - invalid iTunesDB header"));
        return;
    }
    data.remove(0, headerlen);

    parse_db(folder_path, tz, data);
}

void Parse_Ipod::parse_db(QString folder_path, int tz, QByteArray& data) {
    // Apple timestamps begin on 1904-01-01; Unix timestamps begin on 1970-01-01.
    const quint32 APPLE_TIME = 2082844800;
    // A corrupt play count must not expand into billions of UI rows.
    const quint64 MAX_IMPORTED_ENTRIES = 1000000;
    QList<scrob_entry> iTunesTable;
    quint32 numtracks = 0;
    bool have_track_list = false;
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    const auto invalid_block = [this]() {
        emit open_finished(false, tr("Parsing failed - invalid or truncated iTunesDB block"));
    };
    const auto skip = [&stream](qint64 count) {
        return stream.status() == QDataStream::Ok && count >= 0 && count <= stream.device()->bytesAvailable() &&
               stream.skipRawData(count) == count;
    };
    const auto block_fits = [&stream](quint32 length, quint32 consumed) {
        return stream.status() == QDataStream::Ok && length >= consumed &&
               quint64(length - consumed) <= quint64(stream.device()->bytesAvailable());
    };

    // This stream reads an immutable QByteArray through QBuffer. After
    // block_fits validates status and the complete remaining block size, advancing
    // within that block cannot encounter file I/O errors or run past the buffer.
    // Keep the checked skip helper for unvalidated prefixes and Play Counts I/O.
    const auto advance_block = [&stream](quint32 count) { stream.skipRawData(count); };

    bool finished = false;
    while (!stream.atEnd() && !finished) {
        quint32 blocktype = 0;
        quint32 blocklen = 0;
        stream >> blocktype;
        if (stream.status() != QDataStream::Ok) {
            invalid_block();
            return;
        }
        switch (blocktype) {
            case 0x616c686d:  // mhla: artist list header
                stream >> blocklen;
                if (!block_fits(blocklen, 8)) {
                    invalid_block();
                    return;
                }
                advance_block(blocklen - 8);  // mhla header was bounded above.
                break;
            case 0x6169686d:  // mhia: artist record with total block length
                if (!skip(4)) {
                    invalid_block();
                    return;
                }
                stream >> blocklen;
                if (!block_fits(blocklen, 12)) {
                    invalid_block();
                    return;
                }
                advance_block(blocklen - 12);  // mhia total length includes the 12 consumed bytes.
                break;
            case 0x6473686d:  // mhsd: dataset header
                stream >> blocklen;
                if (!block_fits(blocklen, 8) || blocklen < 16) {
                    invalid_block();
                    return;
                }
                advance_block(blocklen - 8);  // Complete dataset header is present.
                break;
            case 0x746c686d:  // mhlt: track list
                stream >> blocklen >> numtracks;
                if (!block_fits(blocklen, 12)) {
                    invalid_block();
                    return;
                }
                advance_block(blocklen - 12);  // mhlt count and remaining header are bounded.
                have_track_list = true;
                break;
            case 0x7469686d:  // mhit: track header
            {
                stream >> blocklen;
                if (!have_track_list || !block_fits(blocklen, 8) || blocklen < 48) {
                    invalid_block();
                    return;
                }
                advance_block(32);  // A bounded mhit header has at least 40 bytes left.
                quint32 tracklen = 0;
                quint32 tracknum = 0;
                stream >> tracklen >> tracknum;
                if (tracknum > quint32(std::numeric_limits<int>::max())) {
                    invalid_block();
                    return;
                }
                advance_block(blocklen - 48);  // The two fields consumed the next eight bytes.
                scrob_entry item{};
                item.length = tracklen / 1000;
                item.tracknum = tracknum;
                iTunesTable.append(item);
                break;
            }
            case 0x646f686d:  // mhod: metadata
            {
                if (!skip(4)) {
                    invalid_block();
                    return;
                }
                quint32 type = 0;
                stream >> blocklen >> type;
                if (!block_fits(blocklen, 16)) {
                    invalid_block();
                    return;
                }
                if (type == MHOD_TITLE || type == MHOD_ALBUM || type == MHOD_ARTIST) {
                    if (iTunesTable.isEmpty() || blocklen < 40 || (blocklen - 40) % 2 != 0) {
                        invalid_block();
                        return;
                    }
                    advance_block(24);  // Metadata size is bounded and at least 40 bytes.
                    const quint32 length = (blocklen - 40) / sizeof(quint16);
                    QList<char16_t> text;
                    text.reserve(length + 1);
                    for (quint32 i = 0; i < length; ++i) {
                        quint16 character = 0;
                        stream >> character;
                        text.append(char16_t(character));
                    }
                    text.append(0);
                    const QString value = QString::fromUtf16(text.constData());
                    if (type == MHOD_TITLE)
                        iTunesTable.last().title = value;
                    else if (type == MHOD_ALBUM)
                        iTunesTable.last().album = value;
                    else
                        iTunesTable.last().artist = value;
                } else
                    advance_block(blocklen - 16);  // Unknown metadata retains the same complete-block bound.
                break;
            }
            case 0x696c686d:  // mhli
            case 0x6969686d:  // mhii
                stream >> blocklen;
                if (!block_fits(blocklen, 8)) {
                    invalid_block();
                    return;
                }
                advance_block(blocklen - 8);  // Artwork headers use the same complete-block bound.
                break;
            case 0x706c686d:  // mhlp
            case 0x7079686d:  // mhyp
            case 0x7069686d:  // mhip
                finished = true;
                break;
            default:
                emit open_finished(false, tr("Unknown blocktype: %1%2%3%4 (0x%5)")
                                              .arg(char(blocktype & 0xff))
                                              .arg(char((blocktype >> 8) & 0xff))
                                              .arg(char((blocktype >> 16) & 0xff))
                                              .arg(char((blocktype >> 24) & 0xff))
                                              .arg(blocktype, 0, 16));
                return;
        }
    }
    if (!have_track_list || numtracks != quint64(iTunesTable.size())) {
        emit open_finished(false, tr("Different number of tracks in track list and iTunesDB"));
        return;
    }

    playcounts_file = folder_path + "/iPod_Control/iTunes/Play Counts";
    QFile input_file(playcounts_file);
    if (!input_file.open(QFile::ReadOnly)) {
        emit open_finished(false, tr("Parsing failed - unable to open Play Counts at %1").arg(playcounts_file));
        return;
    }
    stream.setDevice(&input_file);
    stream.resetStatus();
    stream.setByteOrder(QDataStream::LittleEndian);
    quint32 type = 0;
    quint32 headerlen = 0;
    quint32 entrylen = 0;
    qint32 songs = 0;
    stream >> type;
    const auto invalid_counts = [this]() {
        emit open_finished(false, tr("Parsing failed - invalid Play Counts file %1").arg(playcounts_file));
    };
    if (type != 0x7064686d) {
        invalid_counts();
        return;
    }
    stream >> headerlen >> entrylen >> songs;
    if (stream.status() != QDataStream::Ok || headerlen < 16 || headerlen > quint64(input_file.size()) ||
        entrylen < 12 || songs < 0 || quint64(songs) * entrylen > quint64(input_file.size()) - headerlen ||
        !skip(headerlen - 16)) {
        invalid_counts();
        return;
    }
    if (songs == 0) {
        emit open_finished(false, tr("No tracks have been played - possible Apple bug"));
        return;
    }
    if (songs != iTunesTable.size()) {
        emit open_finished(false, tr("Different number of songs in Play Counts and iTunesDB"));
        return;
    }

    quint64 entries = 0;
    int play_counts = 0;
    QList<quint32> counts;
    counts.reserve(songs);
    for (int x = 0; x < songs; ++x) {
        quint32 playcount = 0;
        quint32 dateplayed = 0;
        quint32 bookmark = 0;
        stream >> playcount >> dateplayed >> bookmark;
        if (!skip(entrylen - 12)) {
            invalid_counts();
            return;
        }
        auto& item = iTunesTable[x];
        if (playcount == 0 || item.artist.isEmpty() || item.title.isEmpty()) playcount = 0;
        entries += playcount;
        if (entries > MAX_IMPORTED_ENTRIES) {
            emit open_finished(false, tr("Parsing failed - excessive iPod play count"));
            return;
        }
        item.when = dateplayed - APPLE_TIME - tz;
        item.played = 'L';
        counts.append(playcount);
        if (playcount > 0) ++play_counts;
    }
    // Emit only after the entire input has been validated; failed imports must not
    // leave partially accepted tracks in the controller.
    for (int x = 0; x < songs; ++x) {
        for (quint32 j = 0; j < counts[x]; ++j) {
            if (j > 0) iTunesTable[x].when = 0;
            emit entry(iTunesTable[x]);
        }
    }
    emit add_log(
        LOG_INFO,
        tr("Found %1 entries, %2 of %3 tracks had playcount information").arg(entries).arg(play_counts).arg(numtracks));
    if (entries > 0) {
        parsed = true;
        emit open_finished(true, "Successfully parsed iTunesDB");
    } else
        emit open_finished(false, "Parsed iTunesDB, but no entries were found");
}

void Parse_Ipod::clear() {
    if (!parsed) {
        emit clear_finished(false, tr("Asked to clear without successfully parsing"));
        return;
    }
    if (QFile(playcounts_file).remove()) {
        parsed = false;
        emit clear_finished(true, tr("Deleted log file %1").arg(playcounts_file));
    } else
        emit clear_finished(false, tr("Failed to remove log file %1").arg(playcounts_file));
}
