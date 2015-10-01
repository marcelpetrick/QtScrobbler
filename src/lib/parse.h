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

#ifndef PARSE_H
#define PARSE_H

#include <QtCore>
#include "common.h"

class Parse : public QObject
{
    Q_OBJECT
public:
    Parse() { parsed = false; }
    virtual ~Parse() { }
    virtual void open(QString, int) = 0;
    virtual void clear() = 0;
    virtual SCROBBLE_METHOD get_method() { return SCROBBLE_NONE; }

signals:
    void open_finished(bool success, QString error_msg);
    void clear_finished(bool success, QString error_msg);
    void add_log(LOG_LEVEL, QString);
    void entry(scrob_entry);

protected:
    bool parsed;
};

#endif
