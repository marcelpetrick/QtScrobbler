/***************************************************************************
 *   Copyright (C) 2009 by Robert Keevil                                   *
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

#ifndef PARSE_MTP_H
#define PARSE_MTP_H

#include <QtCore>
#include "parse.h"

#ifdef HAVE_WPD
#include <windows.h>
#include <PortableDevice.h>
#include <PortableDeviceApi.h>
#endif
#ifdef HAVE_LIBMTP
#include <libmtp.h>
#include <functional>
#endif

class Parse_MTP : public Parse
{
    Q_OBJECT
public:
    Parse_MTP();
#ifdef HAVE_LIBMTP
    // The default backend calls libmtp. Tests can substitute device operations
    // while still exercising this adapter and its ownership/error handling.
    // Callbacks must remain valid for the adapter lifetime. Friendly names use
    // malloc ownership; each listed track and connected list is released through
    // its matching callback, including partial results returned with errors.
    struct Backend {
        std::function<decltype(LIBMTP_Init)> init = LIBMTP_Init;
        std::function<decltype(LIBMTP_Get_Connected_Devices)> connected = LIBMTP_Get_Connected_Devices;
        std::function<decltype(LIBMTP_Release_Device_List)> release = LIBMTP_Release_Device_List;
        std::function<decltype(LIBMTP_Get_Friendlyname)> friendlyName = LIBMTP_Get_Friendlyname;
        std::function<decltype(LIBMTP_Get_Tracklisting_With_Callback)> listing = LIBMTP_Get_Tracklisting_With_Callback;
        std::function<decltype(LIBMTP_destroy_track_t)> destroyTrack = LIBMTP_destroy_track_t;
        std::function<decltype(LIBMTP_Update_Track_Metadata)> update = LIBMTP_Update_Track_Metadata;
        std::function<decltype(LIBMTP_Get_Errorstack)> errors = LIBMTP_Get_Errorstack;
        std::function<decltype(LIBMTP_Clear_Errorstack)> clearErrors = LIBMTP_Clear_Errorstack;
    };
    explicit Parse_MTP(Backend backend);
#endif
    ~Parse_MTP();
    virtual void open(QString, int);
    virtual void clear();
    virtual SCROBBLE_METHOD get_method() { return SCROBBLE_MTP; }

private:
    int entries;
    int playcounts;
    int tracks;

#ifdef HAVE_WPD
    IPortableDeviceManager *pdm;
    QList<PWSTR> mtp_devices;
    void mtp_device_name(DWORD, LPCWSTR);
    bool is_device_mtp(DWORD, LPCWSTR);
    void mtp_do_storage(bool);
    void mtp_recurse_storage(PCWSTR, IPortableDeviceContent *, bool);
    void mtp_get_metadata(PCWSTR, IPortableDeviceContent *, bool);
    int tz_offset;
#endif
#ifdef HAVE_LIBMTP
    LIBMTP_mtpdevice_t *mtp_device_list;
    Backend backend;
    QList<scrob_entry> pending_entries;
    bool mtp_trackinfo(LIBMTP_mtpdevice_t *, LIBMTP_track_t *, bool);
    bool mtp_iterate(bool);
#endif
};

#endif
