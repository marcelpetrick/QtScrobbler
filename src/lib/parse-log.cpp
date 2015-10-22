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

#include "parse-log.h"

Parse_Log::Parse_Log()
{
}

Parse_Log::~Parse_Log()
{
}

void Parse_Log::open(QString folder_path, int tz)
{
    CHECK_RESULT chk = check_path(SCROBBLE_LOG, folder_path);
    if (chk == CHECK_FAIL)
    {
        emit open_finished(false,
             tr("Parsing failed - could not find a logfile at %1 to open")
             .arg(folder_path));
        return;
    }

    if (chk == CHECK_LOG)
        file_path = folder_path + "/.scrobbler.log";
    else
        file_path = folder_path + "/.scrobbler-timeless.log";

    emit add_log(LOG_INFO, QString("Opening log file: %1").arg(file_path));
    emit add_log(LOG_INFO, QString("UTC Offset: %1").arg(QString::number(tz)));

    QFile log_file(file_path);
    if (!log_file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        emit open_finished(false,
            tr("Parsing failed - unable to read file %1").arg(file_path));
        return;
    }

    QList<QByteArray> const log_lines = log_file.readAll().split( '\n' );
    if (log_lines.size() < 4)
    {
        log_file.close();
        emit open_finished(false,
            tr("Parsing failed - log file is empty or corrupt"));
        return;
    }

    bool have_mb = false;

    // check the first header line, and get + check the version.
    QString line = log_lines[0];

    if (line.startsWith("#AUDIOSCROBBLER/")
        && line.length() > 16)
    {
        QString ver = line.mid(16);
        emit add_log(LOG_DEBUG, "Log version: " + ver);
        if (ver == "1.0")
            have_mb = false;
        else if (ver == "1.1")
            have_mb = true;
        else
        {
            log_file.close();
            emit open_finished(false,
                tr("Unknown log version: %1").arg(ver));
            return;
        }
    }
    else
    {
        emit open_finished(false,
            tr("Parsing failed - missing #AUDIOSCROBBLER header"));
        log_file.close();
        return;
    }

    // check the second header line, and get the TZ info.
    line = log_lines[1];
    if (line.startsWith("#TZ/") && line.length() > 4)
    {
        QString log_tz = line.mid(4);
        emit add_log(LOG_DEBUG, "Log TZ: " + log_tz);
        if (log_tz.startsWith("UNKNOWN"))
        {
            // we are fine, continue as-is
        }
        else if (log_tz.startsWith("UTC"))
        {
            tz = 0;
            emit add_log(LOG_INFO, "Overriding detected timezone");
        }
        else
        {
            log_file.close();
            emit open_finished(false, tr("Unknown log TZ: ") + log_tz);
            return;
        }
    }
    else
    {
        log_file.close();
        emit open_finished(false,
            tr("Parsing failed - missing #TZ header"));
        return;
    }

    // check the third header line, and get the client version info.
    line = log_lines[2];
    if (line.startsWith("#CLIENT/") && line.length() > 8)
    {
        QString client = line.mid(8);
        emit add_log(LOG_DEBUG, "Log CLIENT: " + client);
    }
    else
    {
        log_file.close();
        emit open_finished(false,
            tr("Parsing failed - missing #CLIENT header"));
        return;
    }

    int entries = 0;

    // read the rest of the file after the header
    for (int i = 3; i < log_lines.size(); i++)
    {
        line = log_lines[i];
        if (!line.startsWith( '#' ) && line.length() > 0)
        {
            QStringList const log_entry = line.split( '\t' );
            if (log_entry.size() == ((have_mb)?8:7))
            {
                //right number of tabs in the line
                scrob_entry temp_entry;

                temp_entry.artist = QString::fromUtf8(log_entry[0].toLocal8Bit());
                temp_entry.album = QString::fromUtf8(log_entry[1].toLocal8Bit());
                temp_entry.title = QString::fromUtf8(log_entry[2].toLocal8Bit());
                temp_entry.tracknum = log_entry[3].toInt();
                temp_entry.length = log_entry[4].toInt();
                temp_entry.played = log_entry[5][0];
                temp_entry.when = log_entry[6].toInt() - tz;
                if (have_mb && log_entry[7].length() == 36)
                    temp_entry.mb_track_id = log_entry[7];
                else
                    temp_entry.mb_track_id = "";

                entries++;

                emit entry(temp_entry);
            }
        }
    }

    log_file.close();

    if (entries > 0)
    {
        parsed = true;
        emit open_finished(true, tr("Successfully parsed %1 log entries").arg(entries));
    }
}

void Parse_Log::clear()
{
    if (!parsed)
    {
        emit clear_finished(false,
            tr("Asked to clear without successfully parsing"));
        return;
    }
    if (QFile(file_path).remove())
        emit clear_finished(true,
            tr("Deleted log file %1").arg(file_path));
    else
        emit clear_finished(false,
            tr("Failed to remove log file %1").arg(file_path));
}
