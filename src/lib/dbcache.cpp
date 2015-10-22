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

#include "dbcache.h"

#include <QSettings>
#include <QDir>
#include <QSqlRecord>
#include <QSqlError>
#include <QDateTime>

DBCache::DBCache(QObject *parent) :
    QObject(parent)
{
    // let QSettings do the heavy lifing and work out a suitable path for us
    QSettings *settings = new QSettings(QSettings::IniFormat,
                                        QSettings::UserScope,
                                        "qtscrob", "foo");
    path = QFileInfo(settings->fileName()).absoluteDir().path();
    delete settings;
}

DBCache::~DBCache()
{
    if (db.isOpen())
    {
        query.clear();
        db.close();
    }
}

bool DBCache::init()
{
    if (!QDir(path).exists())
    {
        // try to create the path. This is the same as the default conf path,
        // but if the user is using a custom config file location this may
        // never have been created.
        QDir dir;
        if (!dir.mkpath(path))
        {
            emit add_log(LOG_INFO, "DB: path could not be created: " + path);
            return false;
        }
    }

    if (!db.isDriverAvailable("QSQLITE"))
    {
        emit add_log(LOG_INFO, "DB: driver not available");
        return false;
    }

    QString filename = QString("%1/qtscrob.db").arg(path);
    db = QSqlDatabase::addDatabase("QSQLITE", "db_qtscrob");
    db.setDatabaseName(filename);
    bool ok = db.open();
    if (!ok)
    {
        emit add_log(LOG_INFO, QString("DB: failed to open: %1")
                     .arg(db.lastError().text()));
        return false;
    }
    query = QSqlQuery(db);

    QStringList tables = db.tables();
    emit add_log(LOG_DEBUG, QString("DB: tables: %1").arg(tables.join(",")));

    if (tables.contains("schema"))
    {
        query.exec("SELECT value FROM schema WHERE name='version'");
        if (query.first())
        {
            emit add_log(LOG_DEBUG, QString("DB: version: %1")
                         .arg(query.value(0).toString()));
        }
    }
    else
    {
        if (!create())
        {
            emit add_log(LOG_INFO, "DB: failed to create a new DB");
            return false;
        }
    }

    return true;
}

bool DBCache::create()
{
    bool ok = query.exec("create table schema "
               "(name TEXT NOT NULL, value TEXT NOT NULL, PRIMARY KEY(name))");
    if (!ok)
    {
        emit add_log(LOG_INFO, QString("DB: failed to create table schema: %1")
                     .arg(query.lastError().text()));
        return false;
    }

    ok = query.exec(QString("insert into schema (name,value) "
                       "values ('version', '%1')").arg(DB_CURRENT_SCHEMA));
    if (!ok)
    {
        emit add_log(LOG_INFO,
                     QString("DB: failed to insert version schema: %1")
                     .arg(query.lastError().text()));
        return false;
    }

    ok = query.exec("create table track_length "
               "(artist TEXT NOT NULL, title TEXT NOT NULL, "
               "length INT NOT NULL, timestamp INT NOT NULL, "
               "PRIMARY KEY(artist, title))");
    if (!ok)
    {
        emit add_log(LOG_INFO,
                     QString("DB: failed to create table track_length: %1")
                     .arg(query.lastError().text()));
        return false;
    }

    return true;
}

int DBCache::get_track_length(QString artist, QString title)
{
    query.prepare("SELECT length, timestamp FROM track_length "
                  "WHERE artist=:artist AND title=:title");
    query.bindValue(":artist", artist);
    query.bindValue(":title", title);
    query.exec();

    if (query.first())
    {
        int ret = query.value(0).toInt();
        uint age = query.value(1).toInt();
        uint now = QDateTime::currentDateTime().toTime_t();
        if (age > (now - MAX_TRACK_LENGTH_AGE))
            return ret;
    }

    return -1;
}

void DBCache::set_track_length(QString artist, QString title, int length)
{
    uint time = QDateTime::currentDateTime().toTime_t();
    // use prepare to ensure the strings are correctly escaped
    query.prepare("replace into track_length (artist,title,length,timestamp) "
                       "values (:artist,:title,:length,:timestamp)");
    query.bindValue(":artist", artist);
    query.bindValue(":title", title);
    query.bindValue(":length", length);
    query.bindValue(":timestamp", time);
    query.exec();
}
