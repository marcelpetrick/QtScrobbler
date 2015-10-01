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

#ifndef PARSE_LOG_H
#define PARSE_LOG_H

#include <QtCore>
#include "parse.h"

class Parse_Log : public Parse
{
    Q_OBJECT
public:
    Parse_Log();
    ~Parse_Log();
    virtual void open(QString, int);
    virtual void clear();
    virtual SCROBBLE_METHOD get_method() { return SCROBBLE_LOG; }

signals:
    void open_finished(bool success, QString error_msg);
    void clear_finished(bool success, QString error_msg);
    void add_log(LOG_LEVEL, QString);
    void entry(scrob_entry);

private:
    QString file_path;
};

#endif
