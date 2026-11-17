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

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QXmlStreamReader>

#include "common.h"

class GetTrackInfo : public QThread {
    Q_OBJECT
   public:
    explicit GetTrackInfo(QObject* parent = 0);
    // Injected managers retain external ownership and must share this object's affinity.
    GetTrackInfo(QNetworkAccessManager* network_manager, const QUrl& endpoint, QObject* parent = 0);
    ~GetTrackInfo();

   protected:
    void run();

   signals:
    void finished(int index, bool success, int time, int elapsed);
    void add_log(LOG_LEVEL, QString);

   public slots:
    void get(int index, scrob_entry info, int wait);
    // Cancel active and delayed requests without emitting stale completion signals.
    void cancel_pending();

   private slots:
    void get_finished(QNetworkReply* reply);

   private:
    struct RequestState {
        int index = 0;
        QElapsedTimer elapsed;
    };
    QPointer<QNetworkAccessManager> manager;
    QNetworkRequest request;
    QHash<QNetworkReply*, RequestState> pending;
    bool destroying = false;
    quint64 generation = 0;
};

#endif  // GETTRACKINFO_H
