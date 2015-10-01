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

#include "conf.h"

Conf::Conf()
{
    settings = new QSettings(
#ifdef Q_OS_WIN32
                             QSettings::IniFormat,
#else
                             QSettings::NativeFormat,
#endif
                             QSettings::UserScope,
#ifdef Q_OS_MAC
                             "qtscrob.sourceforge.net",
#else
                             "qtscrob",
#endif
                             "qtscrob");

    settings->setFallbacksEnabled(false);

    sites[CONFIG_LASTFM].conf_name = "Last.fm";
    sites[CONFIG_LASTFM].handshake_host = LAST_FM_HOST;

    sites[CONFIG_LIBREFM].conf_name = "Libre.fm";
    sites[CONFIG_LIBREFM].handshake_host = LIBRE_FM_HOST;
}

Conf::~Conf()
{
    settings->sync();
    delete settings;
}

void Conf::set_custom_location(QString location)
{
    emit add_log(LOG_DEBUG, QString("Conf: setting location to %1")
            .arg(location));
    // the file may not exist yet, or could be readonly (eg CLI client)
    // only do basic sanity checks

    if (location.isEmpty())
    {
        emit add_log(LOG_ERROR, tr("Conf: Asked to use a blank custom config location"));
        return;
    }

    QDir tmp_folder(QFileInfo(location).filePath());
    if (!tmp_folder.exists())
    {
        emit add_log(LOG_ERROR, tr("Conf: Custom config parent folder does not exist"));
        return;
    }

    delete settings;
    settings = new QSettings(location, QSettings::IniFormat);

    load_config();
}

bool Conf::load_config()
{
    upgrade_from_old_location();
    if (settings->contains("application/lastDirectory"))
    {
        emit add_log(LOG_INFO, "Conf: Upgrading from 0.10 or before");
        upgrade_from_0_10();
        return true;
    }

    settings->beginGroup("application");
    utc_offset = settings->value("utc_offset", "").toInt();
    tz_override = settings->value("tz_override", false).toBool();
    auto_open = settings->value("auto_open", true).toBool();
    del_apple_playlist = settings->value("del_apple_playlist", false).toBool();
    display_utc = settings->value("display_utc", false).toBool();
    mru = settings->value("mru").toStringList();
    settings->endGroup();
    settings->beginGroup("proxy");
    proxy_host = settings->value("proxy_host", "").toString();
    proxy_port = settings->value("proxy_port", "").toInt();
    proxy_user = settings->value("proxy_user", "").toString();
    proxy_pass = settings->value("proxy_pass", "").toString();
    settings->endGroup();

    for (int i = 0; i < CONFIG_NUM_SITES; i++)
    {
        settings->beginGroup(SITE_NAME[i]);
        sites[i].enabled = settings->value("enabled", false).toBool();
        sites[i].username = settings->value("username", "").toString();
        sites[i].password_hash = settings->value("password_hash", "").toString();
        if (i >= CONFIG_CUSTOM)
        {
            sites[i].conf_name = settings->value("conf_name", "").toString();
            sites[i].handshake_host =
                    settings->value("handshake_host", "").toString();
        }
        settings->endGroup();
    }
    status();


    return true;
}

bool Conf::save_config()
{
    settings->beginGroup("application");
    settings->setValue("version", CLIENT_VERSION);
    settings->setValue("utc_offset", utc_offset);
    settings->setValue("tz_override", tz_override);
    settings->setValue("auto_open", auto_open);
    settings->setValue("del_apple_playlist", del_apple_playlist);
    settings->setValue("display_utc", display_utc);
    settings->setValue("mru", mru);
    settings->endGroup();
    settings->beginGroup("proxy");
    settings->setValue("proxy_host", proxy_host);
    settings->setValue("proxy_port", proxy_port);
    settings->setValue("proxy_user", proxy_user);
    settings->setValue("proxy_pass", proxy_pass);
    settings->endGroup();

    for (int i = 0; i < CONFIG_NUM_SITES; i++)
    {
        settings->beginGroup(SITE_NAME[i]);
        settings->setValue("enabled", sites[i].enabled);
        settings->setValue("username", sites[i].username);
        settings->setValue("password_hash", sites[i].password_hash);
        if (i >= CONFIG_CUSTOM)
        {
            settings->setValue("conf_name", sites[i].conf_name);
            settings->setValue("handshake_host", sites[i].handshake_host);
        }
        settings->endGroup();
    }
    settings->sync();
    status();

    return true;
}

bool Conf::upgrade_from_0_10()
{
    settings->beginGroup("application");
    sites[CONFIG_LASTFM].username = settings->value("username","").toString();
    sites[CONFIG_LASTFM].password_hash = settings->value("password","").toString();
    // turn lastdirectory into mru entry
    mru.append(settings->value("lastDirectory","").toString());
    utc_offset = settings->value("utc_offset","").toInt();
    tz_override = settings->value("tz_override", "false").toBool();
    auto_open = settings->value("checkAutoOpen","true").toBool();
    del_apple_playlist = settings->value("checkDelPlayList","false").toBool();
    settings->endGroup();
    settings->beginGroup("proxy");
    proxy_host = settings->value("proxy_host", "").toString();
    proxy_port = settings->value("proxy_port", "").toInt();
    proxy_user = settings->value("proxy_user", "").toString();
    proxy_pass = settings->value("proxy_pass", "").toString();
    settings->endGroup();

    // Now we're back at the lowest level of the config tree,
    // clear all old entries.
    settings->clear();
    settings->sync();

    sites[CONFIG_LASTFM].enabled = true;
    // Since we've just wiped out the old settings, save back the new version
    // straight away.
    save_config();
    // Load them back in so we get the defaults for uninitialised values
    load_config();

    return true;
}

void Conf::upgrade_from_old_location()
{
    QSettings old("qtscrob");
    old.setFallbacksEnabled(false);
    QStringList keys = old.allKeys();

    // does old contain a valid key (either 0.10 and 0.11)
    if (keys.contains("application/tz_override"))
    {
        emit add_log(LOG_INFO, "Moving config to new location");
        // copy all old key/value pairs from old to current
        for (int i = 0; i < keys.size(); i++)
        {
            settings->setValue(keys.at(i), old.value(keys.at(i)));
        }
        settings->sync();

        // delete all old keys
        old.clear();
        old.sync();

#ifndef Q_OS_WIN32 // if not registry
        QFile(old.fileName()).remove();
#endif
    }
}

QHash<QString, QVariant> Conf::load_custom(QString section)
{
    QHash<QString, QVariant> values;

    settings->beginGroup(section);
    QStringList keys = settings->allKeys();
    for (int i = 0; i < keys.size(); i++)
    {
        values.insert(keys.at(i), settings->value(keys.at(i), ""));
    }
    settings->endGroup();

    return values;
}

void Conf::save_custom(QString section, QHash<QString, QVariant> values)
{
    settings->beginGroup(section);
    QHash<QString, QVariant>::const_iterator i = values.constBegin();
    while (i != values.constEnd()) {
        settings->setValue(i.key(), i.value());
        ++i;
    }
    settings->endGroup();
    settings->sync();
}

void Conf::status()
{
    switch (settings->status())
    {
        case QSettings::NoError:
            emit add_log(LOG_DEBUG, "Conf: Success on read/write");
            break;
        case QSettings::AccessError:
            emit add_log(LOG_ERROR, tr("Conf: Access Error"));
            break;
        case QSettings::FormatError:
            emit add_log(LOG_ERROR, tr("Conf: Format Error"));
            break;
        default:
            break;
    }
}
