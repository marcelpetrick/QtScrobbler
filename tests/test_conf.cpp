// SPDX-License-Identifier: GPL-2.0-only
#include "conf.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class ConfTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        const QString config = qEnvironmentVariable("XDG_CONFIG_HOME");
        QVERIFY(!config.isEmpty());
        QVERIFY(QDir(config).removeRecursively());
        QVERIFY(QDir().mkpath(config));
    }

    void defaults()
    {
        Conf conf;
        QVERIFY(conf.load_config());
        QCOMPARE(conf.utc_offset, 0);
        QVERIFY(!conf.tz_override);
        QVERIFY(conf.auto_open);
        QVERIFY(!conf.del_apple_playlist);
        QVERIFY(!conf.display_utc);
        QVERIFY(conf.mru.isEmpty());
        QVERIFY(conf.proxy_host.isEmpty());
        QCOMPARE(conf.proxy_port, 0);
        QVERIFY(conf.proxy_user.isEmpty());
        QVERIFY(conf.proxy_pass.isEmpty());
        QCOMPARE(conf.sites[CONFIG_LASTFM].conf_name, "Last.fm");
        QCOMPARE(conf.sites[CONFIG_LASTFM].handshake_host, LAST_FM_HOST);
        QCOMPARE(conf.sites[CONFIG_LIBREFM].conf_name, "Libre.fm");
        QCOMPARE(conf.sites[CONFIG_LIBREFM].handshake_host, LIBRE_FM_HOST);
        for (const auto &site : conf.sites)
        {
            QVERIFY(!site.enabled);
            QVERIFY(site.username.isEmpty());
            QVERIFY(site.password_hash.isEmpty());
        }
    }

    void roundTrip()
    {
        {
            Conf conf;
            QVERIFY(conf.load_config());
            conf.utc_offset = -19800;
            conf.tz_override = true;
            conf.auto_open = false;
            conf.del_apple_playlist = true;
            conf.display_utc = true;
            conf.mru = {"/example/one", "/example/two"};
            conf.proxy_host = "proxy.invalid";
            conf.proxy_port = 3128;
            conf.proxy_user = "fictional-user";
            conf.proxy_pass = "fictional-password";
            for (int i = 0; i < CONFIG_NUM_SITES; ++i)
            {
                conf.sites[i].enabled = (i != CONFIG_LIBREFM);
                conf.sites[i].username = QString("user-%1").arg(i);
                conf.sites[i].password_hash = QString("hash-%1").arg(i);
            }
            conf.sites[CONFIG_CUSTOM].conf_name = "Offline test service";
            conf.sites[CONFIG_CUSTOM].handshake_host = "scrobble.invalid";
            QVERIFY(conf.save_config());
        }
        Conf loaded;
        QVERIFY(loaded.load_config());
        QCOMPARE(loaded.utc_offset, -19800);
        QVERIFY(loaded.tz_override);
        QVERIFY(!loaded.auto_open);
        QVERIFY(loaded.del_apple_playlist);
        QVERIFY(loaded.display_utc);
        QCOMPARE(loaded.mru, QStringList({"/example/one", "/example/two"}));
        QCOMPARE(loaded.proxy_host, "proxy.invalid");
        QCOMPARE(loaded.proxy_port, 3128);
        QCOMPARE(loaded.proxy_user, "fictional-user");
        QCOMPARE(loaded.proxy_pass, "fictional-password");
        for (int i = 0; i < CONFIG_NUM_SITES; ++i)
        {
            QCOMPARE(loaded.sites[i].enabled, i != CONFIG_LIBREFM);
            QCOMPARE(loaded.sites[i].username, QString("user-%1").arg(i));
            QCOMPARE(loaded.sites[i].password_hash, QString("hash-%1").arg(i));
        }
        QCOMPARE(loaded.sites[CONFIG_CUSTOM].conf_name, "Offline test service");
        QCOMPARE(loaded.sites[CONFIG_CUSTOM].handshake_host, "scrobble.invalid");
    }

    void newCustomFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("custom.ini");
        Conf conf;
        conf.set_custom_location(path);
        conf.utc_offset = 5400;
        QVERIFY(conf.save_config());
        QVERIFY(QFile::exists(path));
        QSettings actual(path, QSettings::IniFormat);
        QCOMPARE(actual.value("application/utc_offset").toInt(), 5400);
        QCOMPARE(actual.value("application/version").toString(), CLIENT_VERSION);
    }

    void existingCustomFile()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("custom.ini");
        {
            QSettings existing(path, QSettings::IniFormat);
            existing.setValue("application/utc_offset", 7200);
            existing.setValue("application/display_utc", true);
        }
        Conf conf;
        conf.set_custom_location(path);
        QCOMPARE(conf.utc_offset, 7200);
        QVERIFY(conf.display_utc);
    }

    void invalidCustomLocation_data()
    {
        QTest::addColumn<QString>("location");
        QTest::addColumn<QString>("message");
        QTest::newRow("blank") << QString() << "Conf: Asked to use a blank custom config location";
        QTest::newRow("missing-parent") << qEnvironmentVariable("XDG_CONFIG_HOME") + "/absent/file.ini"
                                       << "Conf: Custom config parent folder does not exist";
    }

    void invalidCustomLocation()
    {
        QFETCH(QString, location);
        QFETCH(QString, message);
        Conf conf;
        QVERIFY(conf.load_config());
        QSignalSpy logs(&conf, &Conf::add_log);
        conf.set_custom_location(location);
        QVERIFY(!logs.isEmpty());
        QCOMPARE(logs.last().at(0).value<LOG_LEVEL>(), LOG_ERROR);
        QCOMPARE(logs.last().at(1).toString(), message);
        conf.utc_offset = 99;
        QVERIFY(conf.save_config());
        Conf loaded;
        QVERIFY(loaded.load_config());
        QCOMPARE(loaded.utc_offset, 99);
    }

    void customSection()
    {
        Conf conf;
        const QHash<QString, QVariant> values{{"size", QSize(600, 450)}, {"maximized", true},
                                              {"nested/key", "value"}};
        conf.save_custom("window", values);
        QCOMPARE(conf.load_custom("window"), values);
        QVERIFY(conf.load_custom("missing").isEmpty());
        conf.save_custom("window", {{"maximized", false}});
        QCOMPARE(conf.load_custom("window").value("size").toSize(), QSize(600, 450));
        QCOMPARE(conf.load_custom("window").value("maximized").toBool(), false);
    }

    void invalidFormat()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("broken.ini");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("[unfinished\nkey=value\n"), qint64(22));
        file.close();
        Conf conf;
        QSignalSpy logs(&conf, &Conf::add_log);
        conf.set_custom_location(path);
        QVERIFY(!conf.load_config());
        QCOMPARE(logs.last().at(0).value<LOG_LEVEL>(), LOG_ERROR);
        QCOMPARE(logs.last().at(1).toString(), "Conf: Format Error");
    }

    void saveFailure()
    {
        QTemporaryDir directory;
        const QString parent = directory.filePath("parent");
        QVERIFY(QDir().mkpath(parent));
        Conf conf;
        conf.set_custom_location(parent + "/config.ini");
        QVERIFY(QDir(parent).removeRecursively());
        QFile blocker(parent);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        QSignalSpy logs(&conf, &Conf::add_log);
        QVERIFY(!conf.save_config());
        QCOMPARE(logs.last().at(0).value<LOG_LEVEL>(), LOG_ERROR);
        QCOMPARE(logs.last().at(1).toString(), "Conf: Access Error");
    }

    void legacyMigration()
    {
        {
            QSettings old("qtscrob");
            old.setValue("application/lastDirectory", "/example/player");
            old.setValue("application/username", "legacy-user");
            old.setValue("application/password", "legacy-hash");
            old.setValue("application/utc_offset", -3600);
            old.setValue("application/tz_override", true);
            old.setValue("application/checkAutoOpen", false);
            old.setValue("application/checkDelPlayList", true);
            old.setValue("proxy/proxy_host", "legacy.invalid");
            old.setValue("proxy/proxy_port", 8080);
            old.setValue("proxy/proxy_user", "old-user");
            old.setValue("proxy/proxy_pass", "old-password");
        }
        Conf conf;
        QVERIFY(conf.load_config());
        QCOMPARE(conf.mru, QStringList({"/example/player"}));
        QCOMPARE(conf.utc_offset, -3600);
        QVERIFY(conf.tz_override);
        QVERIFY(!conf.auto_open);
        QVERIFY(conf.del_apple_playlist);
        QVERIFY(conf.sites[CONFIG_LASTFM].enabled);
        QCOMPARE(conf.sites[CONFIG_LASTFM].username, "legacy-user");
        QCOMPARE(conf.sites[CONFIG_LASTFM].password_hash, "legacy-hash");
        QCOMPARE(conf.proxy_host, "legacy.invalid");
        QCOMPARE(conf.proxy_port, 8080);
        QCOMPARE(conf.proxy_user, "old-user");
        QCOMPARE(conf.proxy_pass, "old-password");
        QSettings old("qtscrob");
        QVERIFY(old.allKeys().isEmpty());
        QVERIFY(conf.load_config());
        QCOMPARE(conf.mru.size(), 1);
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    qputenv("HOME", home.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", home.path().toUtf8());
    QCoreApplication application(argc, argv);
    qRegisterMetaType<LOG_LEVEL>();
    ConfTest tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_conf.moc"
