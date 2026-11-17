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

#include <QTimer>
#include <limits>

GetTrackInfo::GetTrackInfo(QObject *parent) :
    GetTrackInfo(nullptr, QUrl(API_URL), parent)
{
}

GetTrackInfo::GetTrackInfo(QNetworkAccessManager *network_manager, const QUrl &endpoint, QObject *parent) :
    QThread(parent), manager(network_manager ? network_manager : new QNetworkAccessManager(this))
{
    request.setUrl(endpoint);
    request.setRawHeader("User-Agent", CLIENT_USERAGENT);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
}

GetTrackInfo::~GetTrackInfo()
{
    destroying = true;
    cancel_pending();
    quit();
    wait();
}

void GetTrackInfo::get(int requested_index, scrob_entry info, int wait_ms)
{
    // The controller computes the remaining pause needed for the service's
    // five-requests-per-second limit. A timer keeps this pause off the GUI stack.
    if (wait_ms > 0)
    {
        const quint64 scheduled_generation = generation;
        QTimer::singleShot(wait_ms, this, [this, requested_index, info, scheduled_generation]() {
            if (generation == scheduled_generation)
                get(requested_index, info, 0);
        });
        return;
    }
    if (!manager)
    {
        emit add_log(LOG_ERROR, tr("API: Network manager is unavailable"));
        emit finished(requested_index, false, 0, 0);
        return;
    }
    emit add_log(LOG_TRACE, QString("API: Fetching time for %1 (%2 by %3)")
        .arg(requested_index).arg(info.title, info.artist));
    QByteArray data = "method=track.getinfo&api_key=" + QByteArray(API_KEY)
        + "&artist=" + QUrl::toPercentEncoding(info.artist)
        + "&track=" + QUrl::toPercentEncoding(info.title) + "&autocorrect=1";
    RequestState state;
    state.index = requested_index;
    state.elapsed.start();
    QNetworkReply *reply = manager->post(request, data);
    pending.insert(reply, state);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { get_finished(reply); });
    connect(reply, &QObject::destroyed, this, [this, reply]() {
        const auto found = pending.find(reply);
        if (found != pending.end())
        {
            const int request_index = found->index;
            const int elapsed = qMin<qint64>(found->elapsed.elapsed(), std::numeric_limits<int>::max());
            pending.erase(found);
            if (!destroying)
            {
                emit add_log(LOG_ERROR, tr("API: Track request was destroyed before completion"));
                emit finished(request_index, false, 0, elapsed);
            }
        }
    });
}

void GetTrackInfo::cancel_pending()
{
    ++generation;
    QList<QPointer<QNetworkReply>> replies;
    for (QNetworkReply *reply : pending.keys())
        replies.append(reply);
    // Remove identities before abort() can emit finished() synchronously.
    pending.clear();
    for (const auto &reply : replies)
    {
        if (reply)
        {
            reply->abort();
            reply->deleteLater();
        }
    }
}

void GetTrackInfo::run()
{
    exec();
}

void GetTrackInfo::get_finished(QNetworkReply *reply)
{
    const auto found = pending.find(reply);
    if (found == pending.end())
        return;
    const RequestState state = found.value();
    pending.erase(found);
    const int elapsed = qMin<qint64>(state.elapsed.elapsed(), std::numeric_limits<int>::max());
    // The reply belongs to this request even when the manager is supplied by a
    // caller. Dispose of it on every outcome, without taking ownership of that manager.
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
    {
        emit add_log(LOG_ERROR, tr("API: Failed to download track length: %1").arg(reply->errorString()));
        emit finished(state.index, false, 0, elapsed);
        return;
    }
    const QByteArray xml = reply->readAll();
    if (xml.trimmed().isEmpty())
    {
        emit add_log(LOG_ERROR, tr("API: Empty XML"));
        emit finished(state.index, false, 0, elapsed);
        return;
    }

    QXmlStreamReader reader(xml);
    bool valid = reader.readNextStartElement() && reader.name() == QLatin1String("lfm")
        && reader.attributes().value("status") == QLatin1String("ok");
    bool have_track = false;
    bool have_duration = false;
    qint64 milliseconds = 0;
    if (valid)
    {
        while (reader.readNextStartElement())
        {
            if (reader.name() != QLatin1String("track"))
            {
                reader.skipCurrentElement();
                continue;
            }
            if (have_track)
                valid = false;
            have_track = true;
            while (reader.readNextStartElement())
            {
                if (reader.name() == QLatin1String("duration"))
                {
                    bool numeric = false;
                    const auto value = reader.readElementText().trimmed().toLongLong(&numeric);
                    if (have_duration || !numeric)
                        valid = false;
                    have_duration = true;
                    milliseconds = value;
                }
                else
                    reader.skipCurrentElement();
            }
        }
    }
    // Consume the complete document so a valid prefix followed by malformed XML
    // cannot be accepted. A fresh reader prevents data leaking between requests.
    while (!reader.atEnd())
        reader.readNext();
    valid = valid && !reader.hasError() && have_track && have_duration && milliseconds >= 1000
        && milliseconds / 1000 <= std::numeric_limits<int>::max();
    if (!valid)
    {
        emit add_log(LOG_ERROR, tr("API: Invalid track duration XML"));
        emit finished(state.index, false, 0, elapsed);
        return;
    }
    const int length = milliseconds / 1000;
    emit add_log(LOG_TRACE, QString("API: %1 is %2, took %3")
        .arg(state.index).arg(length).arg(elapsed / 1000.0));
    emit finished(state.index, true, length, elapsed);
}
