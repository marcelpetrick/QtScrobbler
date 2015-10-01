/***************************************************************************
 *   Copyright (C) 2009-2010 by Robert Keevil                              *
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

#ifndef CONF_H
#define CONF_H

#include <QObject>
#include <QSettings>
#include "common.h"

#define LAST_FM_HOST "post.audioscrobbler.com"
#define LIBRE_FM_HOST "turtle.libre.fm"

struct site_conf {
    QString conf_name;
    bool enabled;
    QString username;
    QString password_hash;
    QString handshake_host;
};

class Conf : public QObject
{
    Q_OBJECT
public:
    Conf();
    ~Conf();
    bool load_config();
    bool save_config();
    void set_custom_location(QString);
    QHash<QString, QVariant> load_custom(QString);
    void save_custom(QString, QHash<QString, QVariant>);

    site_conf sites[CONFIG_NUM_SITES];

    int utc_offset;
    bool auto_open;
    bool del_apple_playlist;
    bool tz_override;
    bool display_utc;
    QStringList mru;

    QString proxy_host;
    int proxy_port;
    QString proxy_user;
    QString proxy_pass;
private:
    QSettings *settings;
    bool upgrade_from_0_10();
    void upgrade_from_old_location();
    void status();
signals:
    void add_log(LOG_LEVEL, QString);
};

#endif // CONF_H
