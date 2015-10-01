/***************************************************************************
 *   Copyright (C) 2006-2009 by Robert Keevil                              *
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

#include "common.h"

// global function, used by libscrobble to sort entries chronologically
bool entry_less_than(const scrob_entry &s1, const scrob_entry &s2)
    { return s1.when < s2.when; }

CHECK_RESULT check_path(SCROBBLE_METHOD method, QString folder_path)
{
    if (folder_path.isEmpty())
        return CHECK_FAIL;

    QString file_path;
    switch (method)
    {
        case SCROBBLE_LOG:
            if (QFile(folder_path + "/.scrobbler.log").exists())
            {
                file_path = folder_path + "/.scrobbler.log";
                return CHECK_LOG;
            }
            if (QFile(folder_path+"/.scrobbler-timeless.log").exists())
            {
                file_path = folder_path + "/.scrobbler-timeless.log";
                return CHECK_LOG_TIMELESS;
            }
            return CHECK_FAIL;
        break;
        case SCROBBLE_IPOD:
            if (QFile(folder_path + "/iPod_Control/iTunes/Play Counts").exists())
            {
                if (QFile(folder_path + "/iPod_Control/iTunes/iTunesDB").exists())
                {
                    return CHECK_ITUNES;
                }
                else if (QFile(folder_path + "/iPod_Control/iTunes/iTunesCDB").exists())
                {
                    return CHECK_ITUNES_COMPRESSED;
                }
                else
                {
                    //emit add_log(LOG_ERROR, tr("Could not find iTunes Play Counts file"));
                    return CHECK_FAIL;
                }
            }
            else
            {
                //emit add_log(LOG_ERROR, tr("Could not find iTunes Database file"));
                return CHECK_FAIL;
            }
        break;
#ifdef HAVE_MTP
        case SCROBBLE_MTP:
            return CHECK_MTP;
        break;
#endif
        default:
            return CHECK_FAIL;
        break;
    }
    return CHECK_FAIL;
}

