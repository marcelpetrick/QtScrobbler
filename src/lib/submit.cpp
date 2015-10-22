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

#include "submit.h"
#include <QCryptographicHash>

Submit::Submit(int submit_index)
{
    index = submit_index;
    nam_handshake = new QNetworkAccessManager();
    nam_submit = new QNetworkAccessManager();
    nr_handshake = nr_submit = NULL;

    connect(nam_handshake, SIGNAL(finished(QNetworkReply*)),
         this, SLOT(handshake_finished(QNetworkReply*)));
    connect(nam_submit, SIGNAL(finished(QNetworkReply*)),
         this, SLOT(senddata_finished(QNetworkReply*)));

    scrob_init = false;
    submission_ok = false;

    need_handshake = true;

    batch_current = batch_total = 0;
}

Submit::~Submit()
{
}

bool Submit::init(submit_context conn)
{
    context = conn;

    scrob_init = true;

    return true;
}

void Submit::do_submit()
{
    batch_total = context.entries->size();
    if (context.entries->size() == 0)
        return;

    if (!scrob_init)
    {
        emit add_log(LOG_ERROR,
                     tr("%1: init function hasn't been run!")
                     .arg(SITE_NAME[index]));
        return;
    }

    if (need_handshake)
        handshake();
    else
        senddata();
}

void Submit::senddata()
{
    //emit add_log(LOG_INFO, "Submit::senddata");
    entry_index.clear();

    QString data = "s=" + sessionid;

    // lock
    context.mutex->lock();

    for ( int i = 0; i < context.entries->size(); i++ )
    {
        scrob_entry tmp = context.entries->at(i);

        if (SENT_UNSENT == tmp.sent[index])
        {
            entry_index.append(i);
            tmp.sent[index] = SENT_FAILED;
            context.entries->replace(i, tmp);

            // check length of musicbrainz id
            QString mbid = "";
            if (tmp.mb_track_id.length() == 36)
                mbid = tmp.mb_track_id;

            QString tracknum = "";
            if (tmp.tracknum != 0)
                tracknum = QString::number(tmp.tracknum);

            QString str_count = QString::number(entry_index.size() - 1);

            data += "&a[" + str_count + "]=" + QUrl::toPercentEncoding(tmp.artist) +
                    "&t[" + str_count + "]=" + QUrl::toPercentEncoding(tmp.title) +
                    "&b[" + str_count + "]=" + QUrl::toPercentEncoding(tmp.album) +
                    "&m[" + str_count + "]=" + mbid +
                    "&l[" + str_count + "]=" + QString::number(tmp.length) +
                    "&i[" + str_count + "]=" + QString::number(tmp.when) +
                    "&o[" + str_count + "]=P" +
                    "&n[" + str_count + "]=" + tracknum +
                    "&r[" + str_count + "]=";
        }
        // do we have enough tracks?
        if (entry_index.size() == 50)
            break;
    }
    context.mutex->unlock();

    emit add_log(LOG_INFO, QString("entry_index: %1").arg(entry_index.size()));
    if (0 == entry_index.size())
    {
        // should never be true...
        nr_submit->disconnect(SIGNAL(uploadProgress(qint64, qint64)));
        nr_submit->deleteLater();
        emit finished(false, tr("%1: Error, attempted to send an empty submission!")
                      .arg(SITE_NAME[index]));
        return;
    }

    data += "&portable=1";  // Magic from the official last.fm client source

    batch_current += entry_index.size();
    emit progress(context.site_index, batch_current, batch_total);

    emit add_log(LOG_TRACE, QString("%1: Data: %2")
                 .arg(SITE_NAME[index], data));
    emit add_log(LOG_INFO, QString("%1: Submitting %2 entries")
                 .arg(SITE_NAME[index]).arg(entry_index.size()));

    QNetworkRequest submit_handshake;
    submit_handshake.setUrl(submit_url);
    submit_handshake.setRawHeader("User-Agent", CLIENT_USERAGENT);
    // MUST have a content type, otherwise both last.fm and libre.fm will
    // reject the post.
    submit_handshake.setHeader(QNetworkRequest::ContentTypeHeader,
                               "application/x-www-form-urlencoded");

    QByteArray submit_data = QByteArray(data.toLocal8Bit());
    nr_submit = nam_submit->post(submit_handshake, submit_data);
}

void Submit::senddata_finished(QNetworkReply *reply)
{
    //emit add_log(LOG_INFO, "Submit::senddata_finished");
    if ( reply->error() != QNetworkReply::NoError ) {
        nr_submit->disconnect(SIGNAL(uploadProgress(qint64, qint64)));
        nr_submit->deleteLater();
        emit finished(false, tr("SUBMIT %1: Request failed, %2")
                      .arg(SITE_NAME[index], reply->errorString()));
        return;
    }

    emit add_log(LOG_INFO, QString("SUBMIT: %1 Request succeeded")
                 .arg(SITE_NAME[index]));

    QString result(reply->readAll());
    emit add_log(LOG_INFO, QString("%1: Server response: %2")
                 .arg(SITE_NAME[index], result));

    if (result != "")
    {
        if (result.contains("no POST parameters"))
        {
            // Server problem - http://www.last.fm/forum/21716/_/201367
            // "FAILED Plugin bug: Not all request variables are set - no POST parameters"
            // This if statement can be removed if/when fixed
            need_handshake = true;
            reset_tracks();
            do_submit();
            return;
        }
        else if (result.contains("BADSESSION"))
        {
            need_handshake = true;
            reset_tracks();
            do_submit();
            return;
        }
        else if (result.contains("FAILED"))
        {
            emit add_log(LOG_INFO, QString("%1: Submission FAILED").arg(SITE_NAME[index]));
            nr_submit->disconnect(SIGNAL(uploadProgress(qint64, qint64)));
            nr_submit->deleteLater();
            emit finished(false,
                          tr("%1: Server returned an error after sending data")
                          .arg(SITE_NAME[index]));
            return;
        }
        else if (result.contains("OK"))
        {
            context.mutex->lock();
            int i;
            int count = 0;
            for ( i = 0; i < entry_index.size(); i++ )
            {
                int entry_num = entry_index.value(i);
                scrob_entry tmp = context.entries->at(entry_num);
                tmp.sent[index] = SENT_SUCCESS;
                context.entries->replace(entry_num, tmp);
            }
            for (i = 0; i < context.entries->size(); i++)
            {
                if (SENT_UNSENT == context.entries->at(i).sent[index])
                    count++;
            }
            context.mutex->unlock();

            if (count > 0)
                do_submit();
            else
            {
                emit add_log(LOG_DEBUG, QString("%1: Submission complete")
                             .arg(SITE_NAME[index]));
                nr_submit->disconnect(SIGNAL(uploadProgress(qint64, qint64)));
                nr_submit->deleteLater();
                nr_handshake->deleteLater();
                submission_ok = true;
                emit finished(true, "");
            }
        }
    }
    else
    {
        nr_submit->disconnect(SIGNAL(uploadProgress(qint64, qint64)));
        nr_submit->deleteLater();
        emit finished(false, tr("%1: Empty result from server")
                      .arg(SITE_NAME[index]));
        return;
    }
}

void Submit::handshake()
{
    //emit add_log(LOG_INFO, "Submit::handshake");
    QString time_str = QString::number(QDateTime::currentDateTime().toTime_t());

    QCryptographicHash auth_hash(QCryptographicHash::Md5);
    auth_hash.addData(QString(context.password_hash + time_str).toLocal8Bit());
    QString auth = QString(auth_hash.result().toHex());

    QUrl url_handshake = QString( "http://%1/?hs=true&p=%2&c=%3&v=%4&u=%5&t=%6&a=%7" )
                .arg(context.host, PROTOCOL_VERSION, CLIENT_ID, CLIENT_VERSION )
                .arg( QString(QUrl::toPercentEncoding(context.username) ))
                .arg( time_str, auth );

    emit add_log(LOG_DEBUG, QString("%1: Time: %2")
                 .arg(SITE_NAME[index], time_str));
    emit add_log(LOG_DEBUG, QString("%1: Auth: %2")
                 .arg(SITE_NAME[index], auth));
    emit add_log(LOG_DEBUG, QString("%1: URL: %2")
                 .arg(SITE_NAME[index], url_handshake.toString(QUrl::None)));

    QNetworkRequest request_handshake;
    request_handshake.setUrl(url_handshake);
    request_handshake.setRawHeader("User-Agent", CLIENT_USERAGENT);

    nr_handshake = nam_handshake->get(request_handshake);
}

void Submit::handshake_finished(QNetworkReply* reply)
{
    if ( reply->error() != QNetworkReply::NoError )
    {
        QString const errorString(tr("%1: HANDSHAKE: Request failed: %2")
                                  .arg(SITE_NAME[index], reply->errorString()));
        emit add_log(LOG_ERROR, errorString);
        emit signalHandshakeFailure(errorString);
        return;
    }

    QString result(reply->readAll());
    emit add_log(LOG_TRACE, QString("Handshake Reply: %1").arg(result));
    if (result != "")
    {
        if (result.startsWith("BANNED"))
        {
            QString const errorString(tr("%1: Handshake Failed - client software banned.  Check http://qtscrob.sourceforge.net for updates.")
                                      .arg(SITE_NAME[index]));
            emit add_log(LOG_ERROR, errorString);
            emit signalHandshakeFailure(errorString);
            return;
        }

        if (result.startsWith("BADAUTH"))
        {
            QString const errorString(tr("%1: Handshake Failed - authentication problem.  Check your username and/or password.")
                                      .arg(SITE_NAME[index]));
            emit add_log(LOG_ERROR, errorString);
            emit signalHandshakeFailure(errorString); // is emitted also the second time!
            return;
        }

        if (result.startsWith("BADTIME"))
        {
            QString const errorString(tr("%1: Handshake Failed - clock is incorrect. Check your computer clock and timezone settings.")
                                      .arg(SITE_NAME[index]));
            emit add_log(LOG_ERROR, errorString);
            emit signalHandshakeFailure(errorString);
            return;
        }

        if (result.startsWith("FAILED"))
        {
            QString const errorString(tr("%1: Handshake Failed")
                                      .arg(SITE_NAME[index]));
            emit add_log(LOG_ERROR, errorString);
            emit signalHandshakeFailure(errorString);
            return;
        }

        if (result.startsWith("OK"))
        {
            emit add_log(LOG_INFO, "HANDSHAKE: Request succeeded");
            QStringList const results = result.split( '\n' );
            need_handshake = false;
            sessionid = results[1];
            nowplay_url = results[2];
            submit_url = results[3];

            emit add_log(LOG_DEBUG, QString("%1: SessionID: %2")
                         .arg(SITE_NAME[index], sessionid));
            emit add_log(LOG_DEBUG, QString("%1: Nowplay URL: %2")
                         .arg(SITE_NAME[index], nowplay_url));
            emit add_log(LOG_DEBUG, QString("%1: Submit URL: %2")
                         .arg(SITE_NAME[index], submit_url));

            if (sessionid == "" || submit_url == "")
            {
                QString const errorString(tr("%1: Bad handshake response")
                                          .arg(SITE_NAME[index]));
                emit add_log(LOG_ERROR, errorString);
                emit signalHandshakeFailure(errorString);
                return;
            }
        }
        else
        {
            QString const errorString(tr("%1: Unknown handshake response")
                                      .arg(SITE_NAME[index]));
            emit add_log(LOG_ERROR, errorString);
            emit signalHandshakeFailure(errorString);
            return;
        }
    }

    senddata();
}

void Submit::cancel_submission()
{
    context.mutex->lock();
    // Should be calling abort on QNetworkReplys instead of deleteLater
    // but abort can cause a seg fault - QT bug
    if (nr_handshake != NULL)
        nr_handshake->deleteLater();
    if (nr_submit != NULL)
        nr_submit->deleteLater();

    context.mutex->unlock();
}

void Submit::reset_tracks()
{
    context.mutex->lock();
    for ( int i = 0; i < context.entries->size(); i++ )
    {
        scrob_entry tmp = context.entries->at(i);

        if (tmp.sent[index] != SENT_SUCCESS)
        {
            tmp.sent[index] = SENT_UNSENT;
            context.entries->replace(i, tmp);
        }
    }
    context.mutex->unlock();
}
