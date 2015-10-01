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

#ifndef GETTRACKINFO_H
#define GETTRACKINFO_H

#include <QObject>
#include <QXmlStreamReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTime>
#include <QThread>
#include "common.h"

class GetTrackInfo : public QThread
{
    Q_OBJECT
public:
    explicit GetTrackInfo(QObject *parent = 0);
    ~GetTrackInfo();

protected:
    void run();

signals:
    void finished(int index, bool success, int time, int elapsed);
    void add_log(LOG_LEVEL, QString);

public slots:
    void get(int index, scrob_entry info, int wait);

private slots:
    void get_finished(QNetworkReply *reply);

private:
    QXmlStreamReader *reader;
    QUrl url;
    QNetworkAccessManager *manager;
    QNetworkRequest request;
    QTime time;
    int index;
    scrob_entry track_info;
};

#endif // GETTRACKINFO_H
