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
#endif

class Parse_MTP : public Parse
{
    Q_OBJECT
public:
    Parse_MTP();
    ~Parse_MTP();
    virtual void open(QString, int);
    virtual void clear();
    virtual SCROBBLE_METHOD get_method() { return SCROBBLE_MTP; }

signals:
    void open_finished(bool success, QString error_msg);
    void clear_finished(bool success, QString error_msg);
    void add_log(LOG_LEVEL, QString);
    void entry(scrob_entry);

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
    void mtp_trackinfo(LIBMTP_mtpdevice_t *, LIBMTP_track_t *, bool);
    void mtp_iterate(bool);
#endif
};

#endif
