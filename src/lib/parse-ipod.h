/***************************************************************************
 *   Copyright (C) 2009-2010 by Robert Keevil                              *
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

#ifndef PARSE_IPOD_H
#define PARSE_IPOD_H

#include <QtCore>
#include "parse.h"

#define MHOD_TITLE 1
#define MHOD_ALBUM 3
#define MHOD_ARTIST 4

class Parse_Ipod : public Parse
{
    Q_OBJECT
public:
    Parse_Ipod();
    ~Parse_Ipod();
    virtual void open(QString, int);
    virtual void clear();
    virtual SCROBBLE_METHOD get_method() { return SCROBBLE_IPOD; }

signals:
    void open_finished(bool success, QString error_msg);
    void clear_finished(bool success, QString error_msg);
    void add_log(LOG_LEVEL, QString);
    void entry(scrob_entry);

private:
    int entries;
    int playcounts;
    int tracks;
    QString playcounts_file;
    bool compressed;
    void open_plain(QString folder_path, int tz, QByteArray &data);
    void open_compressed(QString folder_path, int tz, QByteArray &data);
    void parse_db(QString folder_path, int tz, QByteArray &data);
};

#endif
