// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QTcpServer>
#include <QTcpSocket>
#include <functional>

// A real HTTP peer restricted to loopback. Tests inspect exact bytes received by Qt.
class HttpFixture : public QTcpServer
{
public:
    struct Request {
        QByteArray method;
        QByteArray target;
        QMap<QByteArray, QByteArray> headers;
        QByteArray body;
    };
    struct Response {
        QByteArray body;
        int status = 200;
        bool hold = false;
    };
    QList<Request> requests;
    std::function<Response(const Request &)> respond;

    HttpFixture()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    if (socket->property("handled").toBool())
                        return;
                    QByteArray buffer = socket->property("buffer").toByteArray() + socket->readAll();
                    socket->setProperty("buffer", buffer);
                    const auto end = buffer.indexOf("\r\n\r\n");
                    if (end < 0)
                        return;
                    const auto lines = buffer.left(end).split('\n');
                    const auto first = lines.first().trimmed().split(' ');
                    if (first.size() != 3) {
                        socket->abort();
                        return;
                    }
                    Request request{first[0], first[1], {}, {}};
                    for (const auto &line : lines.mid(1)) {
                        const auto colon = line.indexOf(':');
                        if (colon > 0)
                            request.headers.insert(line.left(colon).trimmed().toLower(),
                                                   line.mid(colon + 1).trimmed());
                    }
                    const int length = request.headers.value("content-length").toInt();
                    if (buffer.size() < end + 4 + length)
                        return;
                    request.body = buffer.mid(end + 4, length);
                    socket->setProperty("handled", true);
                    requests.append(request);
                    const Response response = respond ? respond(request) : Response{};
                    if (response.hold)
                        return;
                    if (response.status == 0) {
                        socket->abort();
                        return;
                    }
                    socket->write("HTTP/1.1 " + QByteArray::number(response.status) +
                                  " Result\r\nContent-Length: " + QByteArray::number(response.body.size()) +
                                  "\r\nConnection: close\r\n\r\n" + response.body);
                    socket->disconnectFromHost();
                });
            }
        });
        listen(QHostAddress::LocalHost, 0);
    }

    QString host() const { return QString("127.0.0.1:%1").arg(serverPort()); }
    QByteArray url(const QByteArray &path) const { return "http://" + host().toLatin1() + path; }
    QByteArray handshake() const { return "OK\nsession\n" + url("/now") + '\n' + url("/submit") + '\n'; }
};
