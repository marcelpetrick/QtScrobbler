/***************************************************************************
 *   Copyright (C) 2010 by Robert Keevil                                   *
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

#ifndef APP_H
#define APP_H

#include <QObject>
#include <QtCore/QCoreApplication>

#include "common.h"
#include "libscrobble.h"

class app : public QCoreApplication
{
    Q_OBJECT
private:
    Scrobble *scrob;
    void print_usage();

    QString config_path;
    QString path;
    bool do_time;
    bool do_tzoverride;
    int do_db;
    int do_file;
#ifdef HAVE_MTP
    int do_mtp;
#endif
    int do_now;
    int do_help;
    int new_time;
    double tz;
    LOG_LEVEL verbosity;

    SCROBBLE_METHOD method;

public:
    app(int&, char**);
    app();
    bool parse_cmd(int argc, char** argv);
    
public slots:
    void parsed(bool);
    void scrobbled(bool);
    void run();
};

#endif // APP_H
