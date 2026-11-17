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
#include <utility>
#include "parse-mtp.h"

Parse_MTP::Parse_MTP() : Parse_MTP(Backend{})
{
}

Parse_MTP::Parse_MTP(Backend operations) : mtp_device_list(nullptr), backend(std::move(operations))
{
    backend.init();
}

Parse_MTP::~Parse_MTP()
{
    if (mtp_device_list)
        backend.release(mtp_device_list);
}

void Parse_MTP::open(QString file_path, int tz)
{
    (void)file_path;
    (void)tz;
    parsed = false;
    pending_entries.clear();
    tracks = playcounts = entries = 0;
    if (mtp_device_list)
    {
        backend.release(mtp_device_list);
        mtp_device_list = nullptr;
    }
    emit add_log(LOG_DEBUG, "MTP: Attempting to connect device(s)");
    const auto result = backend.connected(&mtp_device_list);
    if (result != LIBMTP_ERROR_NONE || !mtp_device_list)
    {
        if (mtp_device_list)
        {
            backend.release(mtp_device_list);
            mtp_device_list = nullptr;
        }
        QString error;
        switch (result)
        {
        case LIBMTP_ERROR_NO_DEVICE_ATTACHED:
            error = tr("MTP: No devices have been found");
            break;
        case LIBMTP_ERROR_CONNECTING:
            error = tr("MTP: Error connecting.");
            break;
        case LIBMTP_ERROR_MEMORY_ALLOCATION:
            error = tr("MTP: Memory Allocation Error.");
            break;
        default:
            error = tr("MTP: Unknown error!");
            break;
        }
        emit open_finished(false, error);
        return;
    }
    emit add_log(LOG_INFO, "MTP: Successfully connected");
    if (!mtp_iterate(false))
    {
        pending_entries.clear();
        emit open_finished(false, tr("MTP: Failed to read tracks"));
        return;
    }
    parsed = true;
    for (const auto &item : std::as_const(pending_entries))
        emit entry(item);
    pending_entries.clear();
    emit add_log(LOG_INFO, QString("Found %1 entries, %2 of %3 tracks had playcount information")
                 .arg(entries).arg(playcounts).arg(tracks));
    emit open_finished(true, tr("MTP: Finished parsing tracks"));
}

void Parse_MTP::clear()
{
    if (!parsed)
    {
        emit clear_finished(false, tr("Asked to clear without successfully parsing"));
        return;
    }
    if (!mtp_iterate(true))
    {
        emit clear_finished(false, tr("MTP: Failed to clear tracks"));
        return;
    }
    parsed = false;
    emit clear_finished(true, tr("MTP: Finished clearing tracks"));
}

bool Parse_MTP::mtp_iterate(bool clear_tracks)
{
    bool success = true;
    for (auto *device = mtp_device_list; device; device = device->next)
    {
        char *friendlyname = backend.friendlyName(device);
        emit add_log(LOG_DEBUG, friendlyname ? "MTP: Friendly name: " + QString::fromUtf8(friendlyname)
                                            : QString("MTP: Friendly name: (NULL)"));
        free(friendlyname);
        backend.clearErrors(device);
        auto *track = backend.listing(device, nullptr, nullptr);
        if (backend.errors(device))
        {
            emit add_log(LOG_ERROR, tr("MTP: Failed to retrieve track listing"));
            success = false;
        }
        if (!track)
            emit add_log(LOG_INFO, "MTP: No tracks.");
        while (track)
        {
            auto *next = track->next;
            // A partial listing must be freed even if libmtp reported a read error.
            if (success && !mtp_trackinfo(device, track, clear_tracks))
                success = false;
            backend.destroyTrack(track);
            track = next;
        }
    }
    return success;
}

bool Parse_MTP::mtp_trackinfo(LIBMTP_mtpdevice_t *device, LIBMTP_track_t *track, bool clear_track)
{
    tracks++;
    emit add_log(LOG_TRACE, QString("%1 : %2 : %3 : %4 : %5 : %6")
                 .arg(QString::fromUtf8(track->artist)).arg(QString::fromUtf8(track->title))
                 .arg(QString::fromUtf8(track->album)).arg(track->tracknumber)
                 .arg(track->duration / 1000).arg(track->usecount));
    if (!track->artist || !track->title || !track->usecount)
        return true;
    if (clear_track)
    {
        track->usecount = 0;
        if (backend.update(device, track) != 0)
        {
            emit add_log(LOG_ERROR, tr("MTP: Failed to update track metadata"));
            return false;
        }
        return true;
    }
    // Match the iPod importer cap: malformed counts must not allocate billions
    // of rows. No entries are emitted until every device has been read.
    if (track->usecount > 1000000U - pending_entries.size())
    {
        emit add_log(LOG_ERROR, tr("MTP: Excessive play count"));
        return false;
    }
    ++playcounts;
    scrob_entry item{};
    item.artist = QString::fromUtf8(track->artist);
    item.title = QString::fromUtf8(track->title);
    item.album = QString::fromUtf8(track->album);
    item.tracknum = track->tracknumber;
    item.length = track->duration / 1000;
    item.played = 'L';
    item.when = 0;
    for (unsigned int i = 0; i < track->usecount; ++i)
    {
        ++entries;
        pending_entries.append(item);
    }
    return true;
}
