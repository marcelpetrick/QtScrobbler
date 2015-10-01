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

#include "gettrackinfo.h"

GetTrackInfo::GetTrackInfo(QObject *parent) :
    QThread(parent)
{
    reader = new QXmlStreamReader();
    manager = new QNetworkAccessManager();

    request.setUrl(QUrl(API_URL));
    request.setRawHeader("User-Agent", CLIENT_USERAGENT);

    connect(manager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(get_finished(QNetworkReply*)));
    time.start();
}

GetTrackInfo::~GetTrackInfo()
{
    manager->disconnect();
    manager->deleteLater();
    delete reader;
}

void GetTrackInfo::get(int index, scrob_entry info, int wait)
{
    // As per section 4.4 of the API, do not perform more than 5 requests
    // per second
    if (wait)
        msleep(wait);

    this->index = index;
    track_info = info;
    reader->clear();

    emit add_log(LOG_TRACE, QString("API: Fetching time for %1 (%2 by %3)")
                 .arg(index).arg(track_info.title, track_info.artist));
    QString data = "method=track.getinfo";
    data += "&api_key=" + QString(API_KEY);
    data += "&artist=" + QUrl::toPercentEncoding(track_info.artist);
    data += "&track=" + QUrl::toPercentEncoding(track_info.title);
    data += "&autocorrect=1";
    time.restart();
    manager->post(request, QByteArray(data.toLocal8Bit()));
}

void GetTrackInfo::run()
{
    exec();
}

void GetTrackInfo::get_finished(QNetworkReply *reply)
{
    int elapsed = time.elapsed();
    if ( reply->error() != QNetworkReply::NoError ) {
        emit add_log(LOG_ERROR, tr("API: Failed to download track length: %1")
                     .arg(reply->errorString()));
        emit finished(index, false, 0, elapsed);
        return;
    }

    QString result(reply->readAll());
    reader->addData(result);
    reader->readNext();
    if (reader->atEnd())
    {
        emit add_log(LOG_ERROR, tr("API: Empty XML"));
        emit finished(index, false, 0, elapsed);
        return;
    }
    QString status;
    int length = 0;
    while (!reader->atEnd())
    {
        if (reader->isStartElement())
        {
            if (reader->name() == "lfm")
            {
                QXmlStreamAttributes attributes = reader->attributes();
                status = attributes.value("status").toString();
            }
            if (reader->name() == "duration")
            {
                length = reader->readElementText().toInt()/1000;
            }
        }
        reader->readNext();
    }

    emit add_log(LOG_TRACE, QString("API: %1 is %2, took %3")
                 .arg(index)
                 .arg(length)
                 .arg(elapsed/1000.0));
    if (status.startsWith("ok"))
        emit finished(index, true, length, elapsed);
    else
        emit finished(index, false, 0, elapsed);
}
