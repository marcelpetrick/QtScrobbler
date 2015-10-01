/***************************************************************************
 *   Copyright (C) 2006-2013 by Robert Keevil                              *
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

#ifndef SUBMIT_H
#define SUBMIT_H

#include <QtCore>
//#include <QHttp>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>

#include "libscrobble.h"
#include "common.h"

class Scrobble;

class Submit : public QObject
{
    Q_OBJECT
public:
    struct submit_context {
        QString friendlyname;
        QString username;
        QString password_hash;
        QString host;
        QList<scrob_entry> *entries;
        bool have_mb;
        int site_index;
        QMutex *mutex;  // lock used for the entries list
    };

    Submit(int);
    ~Submit();
    int index;
    bool init(submit_context);
    void do_submit();

    void cancel_submission();

    bool submission_successful() { return submission_ok; }

private:
    QList<int> entry_index;
    submit_context context;

    QNetworkProxy *proxy;

    // handshaking stuff
    QNetworkAccessManager *nam_handshake;
    QNetworkReply *nr_handshake;
    void handshake();
    bool need_handshake;
    QString sessionid;
    QString nowplay_url;
    QString submit_url;

    QNetworkAccessManager *nam_submit;
    QNetworkReply *nr_submit;
    void senddata();
    void reset_tracks();

    bool scrob_init;
    bool submission_ok;

    int batch_current, batch_total;

private slots:
    void handshake_finished(QNetworkReply*);
    void senddata_finished(QNetworkReply*);

signals:
    void finished(bool success, QString error_msg);
    void progress(int, int, int);
    void add_log(LOG_LEVEL, QString);
};

#endif
