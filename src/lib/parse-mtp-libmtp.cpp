/***************************************************************************
 *   Copyright (C) 2008-2013 by Robert Keevil                              *
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

#include <cstdlib>
#include "parse-mtp.h"

Parse_MTP::Parse_MTP()
{
    qDebug() << "Parse_MTP::Parse_MTP";
    mtp_device_list = NULL;
    LIBMTP_Init();
}

Parse_MTP::~Parse_MTP()
{
    qDebug() << "Parse_MTP::~Parse_MTP";
    if (mtp_device_list != NULL)
    {
        LIBMTP_Release_Device_List(mtp_device_list);
    }
}

void Parse_MTP::open(QString file_path, int tz)
{
    tracks = playcounts = entries = 0;

    (void)file_path;
    (void)tz;

    emit add_log(LOG_DEBUG, "MTP: Attempting to connect device(s)");

    switch(LIBMTP_Get_Connected_Devices(&mtp_device_list))
    {
    case LIBMTP_ERROR_NO_DEVICE_ATTACHED:
        mtp_device_list = NULL;
        emit open_finished(false, tr("MTP: No devices have been found"));
        return;
    case LIBMTP_ERROR_CONNECTING:
        mtp_device_list = NULL;
        emit open_finished(false, tr("MTP: Error connecting."));
        return;
    case LIBMTP_ERROR_MEMORY_ALLOCATION:
        mtp_device_list = NULL;
        emit open_finished(false, tr("MTP: Memory Allocation Error."));
        return;

    // Unknown general errors - This should never execute
    case LIBMTP_ERROR_GENERAL:
    default:
        mtp_device_list = NULL;
        emit open_finished(false, tr("MTP: Unknown error!"));
    return;

    // Successfully connected at least one device, so continue
    case LIBMTP_ERROR_NONE:
        emit add_log(LOG_INFO, "MTP: Successfully connected");
        mtp_iterate(false);
    
        emit add_log(LOG_INFO, QString("Found %1 entries, %2 of %3 tracks had playcount information")
                     .arg(entries)
                     .arg(playcounts)
                     .arg(tracks));
    
        emit open_finished(true, tr("MTP: Finished parsing tracks"));
    }
}

void Parse_MTP::clear()
{
    mtp_iterate(true);
    emit clear_finished(true, tr("MTP: Finished clearing tracks"));
}

void Parse_MTP::mtp_iterate(bool clear_tracks)
{
// This is mostly taken straight from libmtp examples/tracks.c

    LIBMTP_mtpdevice_t *iter;
    LIBMTP_track_t *tracks;


    // iterate through connected MTP devices
    for(iter = mtp_device_list; iter != NULL; iter = iter->next)
    {
        char *friendlyname;
        // Echo the friendly name so we know which device we are working with

        friendlyname = LIBMTP_Get_Friendlyname(iter);
        if (friendlyname == NULL)
        {
            add_log(LOG_DEBUG, "MTP: Friendly name: (NULL)");
        } else {
            add_log(LOG_DEBUG, "MTP: Friendly name: " + QString(friendlyname));
            free(friendlyname);
        }

        // Get track listing.
        tracks = LIBMTP_Get_Tracklisting_With_Callback(iter, NULL, NULL);
        if (tracks == NULL)
        {
            add_log(LOG_INFO, "MTP: No tracks.");
        } else {
            LIBMTP_track_t *track;
            track = tracks;
            while (track != NULL)
            {
                mtp_trackinfo(iter, track, clear_tracks);
                LIBMTP_track_t tmp = track;
                track = track->next;
                LIBMTP_destroy_track_t(tmp);
            }
        }
    }
}

void Parse_MTP::mtp_trackinfo(LIBMTP_mtpdevice_t *device, LIBMTP_track_t *track, bool clear_track)
{
    tracks++;

    emit add_log(LOG_TRACE,
        QString("%1 : %2 : %3 : %4 : %5 : %6")
        .arg(track->artist)
        .arg(track->title)
        .arg(track->album)
        .arg(track->tracknumber)
        .arg(track->duration/1000)
        .arg(track->usecount)
        );

    if (track->artist != NULL // check if missing
        && track->title != NULL // check if missing
        && track->usecount > 0 // check track has been played
        )
    {
        if (clear_track)
        {
            track->usecount = 0;
            LIBMTP_Update_Track_Metadata(device, track);
        }
        else
        {
            playcounts++;

            scrob_entry tmp;
            tmp.artist = QString::fromUtf8(track->artist);
            tmp.title = QString::fromUtf8(track->title);
            tmp.album = (track->album==NULL)?"":QString::fromUtf8(track->album);
            tmp.tracknum = track->tracknumber;
            tmp.length = track->duration/1000;
            tmp.played = 'L';
            tmp.when = 0; // no idea when it was played :(
            tmp.mb_track_id = "";

            for (unsigned int i = 0; i < track->usecount; i++)
            {
                entries++;
                emit entry(tmp);
            }
        }
    }
}
