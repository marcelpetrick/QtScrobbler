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

namespace {
constexpr qint64 MAX_RESPONSE_BYTES = 64 * 1024;
constexpr char RESPONSE_TOO_LARGE[] = "submitResponseTooLarge";

void limitProtocolResponse(QNetworkReply* reply) {
    // Reserve one extra byte to detect overflow even when the server streams a reply without Content-Length.
    reply->setReadBufferSize(MAX_RESPONSE_BYTES + 1);
    QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > MAX_RESPONSE_BYTES) {
            reply->setProperty(RESPONSE_TOO_LARGE, true);
            reply->abort();
        }
    });
}

bool responseTooLarge(const QNetworkReply* reply) {
    return reply->property(RESPONSE_TOO_LARGE).toBool() || reply->bytesAvailable() > MAX_RESPONSE_BYTES;
}
}  // namespace

Submit::Submit(int submit_index) {
    index = submit_index;
    nam_handshake = new QNetworkAccessManager(this);
    nam_submit = new QNetworkAccessManager(this);
    nr_handshake = nr_submit = NULL;

    connect(nam_handshake, &QNetworkAccessManager::finished, this, &Submit::handshake_finished);
    connect(nam_submit, &QNetworkAccessManager::finished, this, &Submit::senddata_finished);

    scrob_init = false;
    submission_ok = false;

    need_handshake = true;

    batch_current = batch_total = 0;
}

Submit::~Submit() {
    // A controller may be destroyed by a handler of QNetworkAccessManager::finished.
    // Keep the managers and their replies alive until that Qt callback stack unwinds.
    for (auto* manager : {nam_handshake, nam_submit}) QObject::disconnect(manager, nullptr, this, nullptr);
    cancel_submission();
    for (auto* manager : {nam_handshake, nam_submit}) {
        manager->setParent(QCoreApplication::instance());
        manager->deleteLater();
    }
}

bool Submit::init(submit_context conn) {
    if (index < 0 || index >= CONFIG_NUM_SITES || !conn.entries || !conn.mutex) return false;
    context = conn;
    batch_current = 0;
    retries = 0;
    cancelled = false;
    submission_ok = false;
    scrob_init = true;

    return true;
}

void Submit::do_submit() {
    if (!scrob_init || cancelled) return;
    batch_total = context.entries->size();
    if (batch_total == 0) {
        emit finished(false, tr("Cannot submit an empty track list"));
        return;
    }

    if (need_handshake)
        handshake();
    else
        senddata();
}

void Submit::senddata() {
    // emit add_log(LOG_INFO, "Submit::senddata");
    entry_index.clear();

    QString data = "s=" + QUrl::toPercentEncoding(sessionid);

    // lock
    context.mutex->lock();

    for (int i = 0; i < context.entries->size(); i++) {
        scrob_entry tmp = context.entries->at(i);

        if (SENT_UNSENT == tmp.sent[index]) {
            entry_index.append(i);
            tmp.sent[index] = SENT_FAILED;
            context.entries->replace(i, tmp);

            // check length of musicbrainz id
            QString mbid = "";
            if (tmp.mb_track_id.length() == 36) mbid = tmp.mb_track_id;

            QString tracknum = "";
            if (tmp.tracknum != 0) tracknum = QString::number(tmp.tracknum);

            QString str_count = QString::number(entry_index.size() - 1);

            data += "&a[" + str_count + "]=" + QUrl::toPercentEncoding(tmp.artist) + "&t[" + str_count +
                    "]=" + QUrl::toPercentEncoding(tmp.title) + "&b[" + str_count +
                    "]=" + QUrl::toPercentEncoding(tmp.album) + "&m[" + str_count + "]=" + mbid + "&l[" + str_count +
                    "]=" + QString::number(tmp.length) + "&i[" + str_count + "]=" + QString::number(tmp.when) + "&o[" +
                    str_count + "]=P" + "&n[" + str_count + "]=" + tracknum + "&r[" + str_count + "]=";
        }
        // do we have enough tracks?
        if (entry_index.size() == 50) break;
    }
    context.mutex->unlock();

    emit add_log(LOG_INFO, QString("entry_index: %1").arg(entry_index.size()));
    if (0 == entry_index.size()) {
        // should never be true...
        emit finished(false, tr("%1: Error, attempted to send an empty submission!").arg(SITE_NAME[index]));
        return;
    }

    data += "&portable=1";  // Magic from the official last.fm client source

    emit progress(context.site_index, batch_current + entry_index.size(), batch_total);

    emit add_log(LOG_INFO, QString("%1: Submitting %2 entries").arg(SITE_NAME[index]).arg(entry_index.size()));

    QNetworkRequest submit_handshake;
    submit_handshake.setUrl(submit_url);
    submit_handshake.setRawHeader("User-Agent", CLIENT_USERAGENT);
    // MUST have a content type, otherwise both last.fm and libre.fm will
    // reject the post.
    submit_handshake.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QByteArray submit_data = QByteArray(data.toUtf8());
    nr_submit = nam_submit->post(submit_handshake, submit_data);
    limitProtocolResponse(nr_submit);
}

void Submit::senddata_finished(QNetworkReply* reply) {
    if (reply != nr_submit || cancelled) return;
    nr_submit = nullptr;
    reply->deleteLater();
    if (responseTooLarge(reply)) {
        emit finished(false, tr("%1: Server returned an error after sending data").arg(SITE_NAME[index]));
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit finished(false, tr("SUBMIT %1: Request failed, %2").arg(SITE_NAME[index], reply->errorString()));
        return;
    }
    const QString result = QString::fromUtf8(reply->read(MAX_RESPONSE_BYTES)).trimmed();
    if (result == "BADSESSION" || result.contains("no POST parameters")) {
        if (++retries > 2) {
            emit finished(false, tr("%1: Submission retry limit reached").arg(SITE_NAME[index]));
            return;
        }
        need_handshake = true;
        reset_tracks();
        do_submit();
        return;
    }
    if (result != "OK") {
        emit finished(false, result.isEmpty()
                                 ? tr("%1: Empty result from server").arg(SITE_NAME[index])
                                 : tr("%1: Server returned an error after sending data").arg(SITE_NAME[index]));
        return;
    }
    int remaining = 0;
    {
        QMutexLocker lock(context.mutex);
        for (int entry : std::as_const(entry_index)) (*context.entries)[entry].sent[index] = SENT_SUCCESS;
        for (const auto& entry : std::as_const(*context.entries))
            if (entry.sent[index] == SENT_UNSENT) ++remaining;
    }
    batch_current += entry_index.size();
    retries = 0;
    if (remaining)
        do_submit();
    else {
        submission_ok = true;
        emit finished(true, "");
    }
}

void Submit::handshake() {
    // emit add_log(LOG_INFO, "Submit::handshake");
    QString time_str = QString::number(QDateTime::currentDateTime().toSecsSinceEpoch());

    QCryptographicHash auth_hash(QCryptographicHash::Md5);
    auth_hash.addData(QString(context.password_hash + time_str).toLocal8Bit());
    QString auth = QString(auth_hash.result().toHex());

    QUrl url_handshake = QString("http://%1/?hs=true&p=%2&c=%3&v=%4&u=%5&t=%6&a=%7")
                             .arg(context.host, PROTOCOL_VERSION, CLIENT_ID, CLIENT_VERSION,
                                  QString::fromLatin1(QUrl::toPercentEncoding(context.username)), time_str, auth);

    emit add_log(LOG_DEBUG, QString("%1: Time: %2").arg(SITE_NAME[index], time_str));

    QNetworkRequest request_handshake;
    request_handshake.setUrl(url_handshake);
    request_handshake.setRawHeader("User-Agent", CLIENT_USERAGENT);

    nr_handshake = nam_handshake->get(request_handshake);
    limitProtocolResponse(nr_handshake);
}

void Submit::handshake_finished(QNetworkReply* reply) {
    if (reply != nr_handshake || cancelled) return;
    nr_handshake = nullptr;
    reply->deleteLater();
    QString error;
    if (responseTooLarge(reply))
        error = tr("%1: Bad handshake response").arg(SITE_NAME[index]);
    else if (reply->error() != QNetworkReply::NoError)
        error = tr("%1: HANDSHAKE: Request failed: %2").arg(SITE_NAME[index], reply->errorString());
    else {
        const auto fields = QString::fromUtf8(reply->read(MAX_RESPONSE_BYTES)).split('\n');
        const QString status = fields.first().trimmed();
        if (status == "BANNED")
            error = tr("%1: Handshake Failed - client software banned").arg(SITE_NAME[index]);
        else if (status == "BADAUTH")
            error = tr("%1: Handshake Failed - authentication problem").arg(SITE_NAME[index]);
        else if (status == "BADTIME")
            error = tr("%1: Handshake Failed - clock is incorrect").arg(SITE_NAME[index]);
        else if (status.startsWith("FAILED"))
            error = tr("%1: Handshake Failed").arg(SITE_NAME[index]);
        else if (status.isEmpty() || status == "OK") {
            const QUrl endpoint(fields.value(3).trimmed());
            if (status.isEmpty() || fields.size() < 4 || fields.value(1).trimmed().isEmpty() || !endpoint.isValid() ||
                endpoint.host().isEmpty() || (endpoint.scheme() != "http" && endpoint.scheme() != "https"))
                error = tr("%1: Bad handshake response").arg(SITE_NAME[index]);
            else {
                sessionid = fields[1].trimmed();
                nowplay_url = fields[2].trimmed();
                submit_url = endpoint.toString();
                need_handshake = false;
            }
        } else
            error = tr("%1: Unknown handshake response").arg(SITE_NAME[index]);
    }
    if (!error.isEmpty()) {
        emit add_log(LOG_ERROR, error);
        emit signalHandshakeFailure(error);
        return;
    }
    senddata();
}

void Submit::cancel_submission() {
    cancelled = true;
    for (QNetworkReply* reply : {nr_handshake, nr_submit}) {
        if (reply) {
            reply->disconnect(this);
            reply->abort();
            reply->deleteLater();
        }
    }
    nr_handshake = nr_submit = nullptr;
}

void Submit::reset_tracks() {
    context.mutex->lock();
    for (int i = 0; i < context.entries->size(); i++) {
        scrob_entry tmp = context.entries->at(i);

        if (tmp.sent[index] != SENT_SUCCESS) {
            tmp.sent[index] = SENT_UNSENT;
            context.entries->replace(i, tmp);
        }
    }
    context.mutex->unlock();
}
