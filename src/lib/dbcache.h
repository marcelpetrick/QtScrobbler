/***************************************************************************
 *   Copyright (C) 2010 Robert Keevil                                      *
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

#ifndef DBCACHE_H
#define DBCACHE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "common.h"


#define DB_CURRENT_SCHEMA 1
#define MAX_TRACK_LENGTH_AGE 15778800 // 6 months in seconds

class DBCache : public QObject
{
    Q_OBJECT
public:
    explicit DBCache(QObject *parent = 0);
    ~DBCache();
    bool init();
    int get_track_length(QString artist, QString title);
    void set_track_length(QString artist, QString title, int length);

signals:
    void add_log(LOG_LEVEL, QString);

public slots:

private:
    QSqlDatabase db;
    QSqlQuery query;
    QString path;
    bool create();
};

#endif // DBCACHE_H
