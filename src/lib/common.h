/***************************************************************************
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

#ifndef COMMMON_H
#define COMMMON_H

#include <QtCore>

#define CLIENT_ID "qts"
#define CLIENT_VERSION "0.11"
#define CLIENT_USERAGENT "QTScrobbler v" CLIENT_VERSION
#define PROTOCOL_VERSION "1.2"
#define API_KEY "992b6748eea766062671eccb419aecd0"
#define API_URL "http://ws.audioscrobbler.com/2.0/"

#if defined(HAVE_WPD) && defined(HAVE_LIBMTP)
#error Cannot have both WPD and Libmtp at the same time
#endif

#if defined(HAVE_WPD) || defined(HAVE_LIBMTP)
#define HAVE_MTP       // to enable MTP support
#endif

#ifdef HAVE_WMDM
#include <windows.h>
#include "initguid.h"
#include "mswmdm_i.c"
#include "mswmdm.h"
#include "sac.h"
#include "scclient.h"
#endif
#ifdef HAVE_LIBMTP
#include <libmtp.h>
#endif

enum CONFIG_SITES {
    CONFIG_LASTFM = 0,
    CONFIG_LIBREFM,
    CONFIG_CUSTOM,
    CONFIG_NUM_SITES
};

const QString SITE_NAME[CONFIG_NUM_SITES] = {
    "Last.fm",
    "Libre.fm",
    QCoreApplication::translate("Scrobble", "Custom")
};

enum SCROBBLE_METHOD {
    SCROBBLE_NONE = -1,
    SCROBBLE_LOG,
    SCROBBLE_IPOD,
#ifdef HAVE_MTP
    SCROBBLE_MTP,
#endif
    // Add new method above this
    SCROBBLE_NUM_METHODS
};

enum CHECK_RESULT {
    CHECK_FAIL,
    CHECK_LOG,
    CHECK_LOG_TIMELESS,
    CHECK_ITUNES,
    CHECK_ITUNES_COMPRESSED,
#ifdef HAVE_MTP
    CHECK_MTP
#endif
};

enum SENT_STATE {
    SENT_UNSENT,
    SENT_FAILED,
    SENT_SUCCESS
};

struct scrob_entry{
    QString artist;
    QString album;
    QString title;
    int tracknum;
    int length;
    QChar played;
    int when;    // times in GMT
    QString mb_track_id;
    SENT_STATE sent[CONFIG_NUM_SITES];
};

CHECK_RESULT check_path(SCROBBLE_METHOD, QString);
bool entry_less_than(const scrob_entry &s1, const scrob_entry &s2);

enum LOG_LEVEL {
    LOG_ERROR,
    LOG_INFO,
    LOG_DEBUG,
    LOG_TRACE,
    LOG_NUM_LEVELS
};

const LOG_LEVEL LOG_DEFAULT = LOG_INFO;

const QString LOG_LEVEL_NAMES[LOG_NUM_LEVELS] = {
    QCoreApplication::translate("Scrobble", "ERROR"),
    QCoreApplication::translate("Scrobble", "INFO "),
    QCoreApplication::translate("Scrobble", "DEBUG"),
    QCoreApplication::translate("Scrobble", "TRACE")
};

struct log_entry {
    LOG_LEVEL level;
    QString msg;
};

#endif
