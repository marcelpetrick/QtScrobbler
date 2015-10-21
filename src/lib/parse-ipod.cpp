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

Parse_Ipod::Parse_Ipod() :
    entries(0),
    playcounts(0),
    tracks(0),
    playcounts_file(QString())
{
    qDebug() << "Parse_Ipod::Parse_Ipod";
    compressed = false;
}

Parse_Ipod::~Parse_Ipod()
{
    qDebug() << "Parse_Ipod::~Parse_Ipod";
}

void Parse_Ipod::open(QString folder_path, int tz)
{
    compressed = false;
    // ensure we set compressed if needed
    CHECK_RESULT chk = check_path(SCROBBLE_IPOD, folder_path);
    if (chk == CHECK_ITUNES_COMPRESSED)
        compressed = true;

    QString db_filename = folder_path;
    if (compressed)
        db_filename += "/iPod_Control/iTunes/iTunesCDB";
    else
        db_filename += "/iPod_Control/iTunes/iTunesDB";

    QFile input_file(db_filename);

    if (!input_file.open(QIODevice::ReadOnly))
    {
        emit open_finished(false,
            tr("Parsing failed - unable to open iTunesDB at %1")
            .arg(db_filename));
        return;
    }
    QByteArray data = input_file.readAll();
    input_file.close();

    if (compressed)
        open_compressed(folder_path, tz, data);
    else
        open_plain(folder_path, tz, data);

}

void Parse_Ipod::open_compressed(QString folder_path, int tz, QByteArray &data)
{
    // read required header info, discard, and decompress remainder

    qint32 magic;
    qint32 headersize;
    qint32 compressedsize;

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> magic;

    if (magic != 0x6462686D) // "mhbd"
    {
        emit open_finished(false,
            tr("Parsing failed - not an iTunesCDB file"));
        return;
    }

    stream >> headersize;
    stream >> compressedsize;

    // remove header
    data.remove(0, headersize);

    // big endian size of the uncompressed data
    QByteArray be_size;
    be_size.resize(4);

    // guess at the uncompressed size - based on only one iTunesCDB seen so far
    int len = data.length() * 10;

    be_size[0] = (len & 0xff000000) >> 24;
    be_size[1] = (len & 0x00ff0000) >> 16;
    be_size[2] = (len & 0x0000ff00) >> 8;
    be_size[3] = (len & 0x000000ff);

    data.prepend(be_size);
    qDebug() << data;
    QByteArray plain = qUncompress(data);

/*
    QFile out("/tmp/test.dat");
    if (!out.open(QIODevice::ReadWrite))
    {
        emit add_log(LOG_ERROR, "failed to open");
        return;
    }
    int ret = out.write(plain);
    out.close();
*/
    parse_db(folder_path, tz, plain);
}

void Parse_Ipod::open_plain(QString folder_path, int tz, QByteArray &data)
{
    // read the info we want from the header, and then discard it
    quint32 type;
    quint32 headerlen;

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> type;
    if ( type != 0x6462686D ) {
        emit open_finished(false,
            tr("Parsing failed - not an iTunesDB file"));
        return;
    }

    stream >> headerlen;
    data.remove(0, headerlen);

    parse_db(folder_path, tz, data);
}

void Parse_Ipod::parse_db(QString folder_path, int tz, QByteArray &data)
{
    // Unix time - number of seconds elapsed since January 1, 1970
    // Apple stores time information as number of seconds elapsed
    // since January 1, 1904
    // This value is the difference in seconds between these points
    const quint32 APPLE_TIME = 2082844800;

    int entries = 0;
    int play_counts = 0;
    qint32 songs;
    quint32 type;
    quint32 headerlen;
    quint32 blocktype;
    quint32 blocklen;
    quint32 numtracks;
    quint32 tracklen;
    quint32 tracknum;
    quint32 playcount;
    quint32 bookmark;
    quint32 dateplayed;
    quint32 entrylen;
    QList<scrob_entry> iTunesTable;

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    int position = -1;
    bool finished = false;
    while( !stream.atEnd() && !finished ) {
      stream >> blocktype;
      switch(blocktype) {
        case 0x616C686D: {    // mhla
            stream >> blocklen;
            stream.skipRawData(blocklen - 4);
        }
        case 0x6169686D: {    // mhia
            stream.skipRawData(4);
            stream >> blocklen;
            stream.skipRawData(blocklen - 8);
        }
        case 0x6473686D: {    // mhsd
            stream >> blocklen;
            stream.skipRawData(8);
            stream.skipRawData(blocklen - 16);
        }
        break;
        case 0x746C686D: {    // mhlt
            stream >> blocklen;
            stream >> numtracks;
            stream.skipRawData(blocklen - 12); // skip the rest
        }
        break;
        case 0x7469686D: {    // mhit
            scrob_entry tmp;
            iTunesTable.push_back(tmp);
            position++;
            stream >> blocklen;

            if(blocklen < 48) {
                emit add_log(LOG_INFO,
                    "mhit seems very small, we won't find length nor tracknum");
                break;
            }
            stream.skipRawData(32);
            stream >> tracklen;
            stream >> tracknum;
            iTunesTable[position].length = tracklen / 1000;
            iTunesTable[position].tracknum = tracknum;
            stream.skipRawData(blocklen - 48);	// can skip rest
        }
        break;
        case 0x646F686D: {     // mhod
            stream.skipRawData(4);
            stream >> blocklen;
            stream >> type;

            switch ( type )
            {
                case MHOD_TITLE:    // we just need title, album and artist
                case MHOD_ALBUM:
                case MHOD_ARTIST: {

                uint ucs2len = ( blocklen - 40 ) / sizeof( quint16 );
                quint16 * buffer = new quint16 [ ucs2len + 1 ];

                stream.skipRawData(24);
                for ( uint i = 0; i < ucs2len; ++i )
                {
                        stream >> buffer[ i ];
                }

                buffer[ ucs2len ] = 0;
                QString temp = QString::fromUtf16(buffer);
                delete [] buffer;


                switch (type)
                {
                    case MHOD_TITLE: iTunesTable[position].title = temp;
                        break;
                    case MHOD_ALBUM: iTunesTable[position].album = temp;
                        break;
                    case MHOD_ARTIST: iTunesTable[position].artist = temp;
                        break;
                }
                break;
                }

                default:
                    stream.skipRawData(blocklen - 16); // skip the whole block
                break;
            }
        }
        break;
        case 0x696c686D: // mhli
            {
                stream >> blocklen;
                stream.skipRawData(blocklen - 8);
            }
            break;
        case 0x6969686D: // mhii
            {
                stream >> blocklen;
                stream.skipRawData(blocklen - 8);
            }
            break;
        case 0x706C686D: // mhlp
        case 0x7079686D: // mhyp
        case 0x7069686D: // mhip
        {
             finished = true;		// we don't need more info
        }
        break;

        default: {
            emit open_finished(false,
                               tr("Unknown blocktype: %1%2%3%4 (0x%5)")
                               .arg((char)(blocktype & 0xFF))
                               .arg((char)((blocktype & 0xFF00) >> 8))
                               .arg((char)((blocktype & 0xFF0000) >> 16))
                               .arg((char)((blocktype & 0xFF000000) >> 24))
                               .arg(blocktype, 0, 16));
            return;
        }
      }
    }


    playcounts_file = folder_path + "/iPod_Control/iTunes/Play Counts";
    QFile input_file(playcounts_file);
    if (!input_file.open(QFile::ReadOnly))
    {
        emit open_finished(false,
            tr("Parsing failed - unable to open Play Counts at %1")
            .arg(playcounts_file));
        return;
    }

    stream.setDevice(&input_file);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> type;
    if ( type != 0x7064686D ) {
        emit open_finished(false,
            tr("Parsing failed - invalid Play Counts file %1")
            .arg(playcounts_file));
        input_file.close();
        return;
    }

    stream >> headerlen;
    stream >> entrylen;
    stream >> songs;
    stream.skipRawData(headerlen - 16);

    if (songs == 0)
    {
        emit open_finished(false,
            tr("No tracks have been played - possible Apple bug"));
        input_file.close();
        return;
    }

    if (position + 1 != songs)
    {
        emit open_finished(false,
            tr("Different number of songs in Play Counts and iTunesDB"));
        input_file.close();
        return;
    }

    for (int x=0; x < songs; x++) {
        iTunesTable[x].mb_track_id = "";

        stream >> playcount;

        emit add_log(LOG_TRACE,
            QString("%1 : %2 : %3 : %4 : %5 : %6")
            .arg(iTunesTable[x].artist)
            .arg(iTunesTable[x].title)
            .arg(iTunesTable[x].album)
            .arg(iTunesTable[x].tracknum)
            .arg(iTunesTable[x].length)
            .arg(playcount)
            );

        if (playcount > 0 && !iTunesTable[x].artist.isEmpty()
            && !iTunesTable[x].title.isEmpty()) {
            play_counts++;

            stream >> dateplayed;
            stream >> bookmark;

            iTunesTable[x].when = dateplayed - APPLE_TIME - tz;
            iTunesTable[x].played = 'L';
            for (unsigned int j = 0; j < playcount; j++)
            {
                entries++;
                // Only use the last playcount for one instance
                // we'll recalc the rest afterwards
                // via Scrobble::check_timestamps()
                if (j > 0)
                    iTunesTable[x].when = 0;
                emit entry(iTunesTable[x]);
            }
            stream.skipRawData(entrylen - 12);
        } else {
            iTunesTable[x].played = 'S';
            stream.skipRawData(entrylen - 4);
        }
    }
    input_file.close();

    iTunesTable.clear();

    emit add_log(LOG_INFO,
                 tr("Found %1 entries, %2 of %3 tracks had playcount information")
                 .arg(entries)
                 .arg(play_counts)
                 .arg(numtracks));

    if (entries > 0)
    {
        parsed = true;
        emit open_finished(true, "Successfully parsed iTunesDB");
    }
    else
        emit open_finished(false, "Parsed iTunesDB, but no entries were found");
}

void Parse_Ipod::clear()
{
    if (!parsed)
    {
        emit clear_finished(false,
                            tr("Asked to clear without successfully parsing"));
        return;
    }
    if (QFile(playcounts_file).remove())
        emit clear_finished(true,
                            tr("Deleted log file %1").arg(playcounts_file));
    else
        emit clear_finished(false,
                            tr("Failed to remove log file %1")
                            .arg(playcounts_file));
}
