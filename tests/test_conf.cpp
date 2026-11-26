// SPDX-License-Identifier: GPL-2.0-only
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cstring>
#include <new>

#include "conf.h"
#include "test_environment.h"

namespace {
// A normal allocator can return storage that held arbitrary prior data. Use a
// fixed prior pattern so omitted scalar initializers fail deterministically.
class ConfStorage {
    alignas(Conf) unsigned char storage[sizeof(Conf)];

   public:
    Conf* value;
    ConfStorage() {
        std::memset(storage, 0x13, sizeof(storage));
        value = new (storage) Conf;
    }
    ~ConfStorage() { value->~Conf(); }
};
}  // namespace

class ConfTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase() {
        if (!test_environment::legacyRegistryWritable())
            QSKIP(
                "Legacy settings are in the Windows registry; set QTSCROBBLER_TEST_WINDOWS_REGISTRY=1 to allow "
                "clearing them");
    }

    void cleanupTestCase() { test_environment::clearLegacyRegistry(); }

    void init() {
        QTest::failOnWarning(QRegularExpression(".*"));
        test_environment::clearLegacyRegistry();
        const QString config = qEnvironmentVariable("XDG_CONFIG_HOME");
        QVERIFY(!config.isEmpty());
        QVERIFY(QDir(config).removeRecursively());
        QVERIFY(QDir().mkpath(config));
    }

    void saveBeforeLoadHasDisabledSiteDefaults() {
        ConfStorage storage;
        auto& conf = *storage.value;
        for (const auto& site : conf.sites) {
            QVERIFY(!site.enabled);
            QVERIFY(site.username.isEmpty());
            QVERIFY(site.password_hash.isEmpty());
        }
        QCOMPARE(conf.sites[CONFIG_LASTFM].conf_name, "Last.fm");
        QCOMPARE(conf.sites[CONFIG_LASTFM].handshake_host, LAST_FM_HOST);
        QCOMPARE(conf.sites[CONFIG_LIBREFM].conf_name, "Libre.fm");
        QCOMPARE(conf.sites[CONFIG_LIBREFM].handshake_host, LIBRE_FM_HOST);
        QVERIFY(conf.sites[CONFIG_CUSTOM].conf_name.isEmpty());
        QVERIFY(conf.sites[CONFIG_CUSTOM].handshake_host.isEmpty());
        QVERIFY(conf.save_config());
        Conf reopened;
        QVERIFY(reopened.load_config());
        for (const auto& site : reopened.sites) QVERIFY(!site.enabled);
    }

    void failedMigrationLeavesSafeSiteDefaults() {
        {
            QSettings old("qtscrob");
            old.setValue("application/tz_override", true);
            old.setValue("application/username", "preserved-user");
        }
        QTemporaryDir directory;
        ConfStorage storage;
        auto& conf = *storage.value;
        QSignalSpy logs(&conf, &Conf::add_log);
        conf.set_custom_location(directory.path());
        logs.clear();
        QVERIFY(!conf.load_config());
        QCOMPARE(logs.size(), 2);
        QCOMPARE(logs.first().at(0).value<LOG_LEVEL>(), LOG_INFO);
        QCOMPARE(logs.first().at(1).toString(), "Moving config to new location");
        QCOMPARE(logs.last().at(0).value<LOG_LEVEL>(), LOG_ERROR);
        QCOMPARE(logs.last().at(1).toString(), "Conf: Access Error");
        for (const auto& site : conf.sites) QVERIFY(!site.enabled);
        QVERIFY(!conf.save_config());
        QSettings preserved("qtscrob");
        QCOMPARE(preserved.value("application/username").toString(), "preserved-user");
    }

    void systemFallbacksCannotEnableAccountsOrTriggerLegacyMigration() {
        {
            QSettings current(test_environment::currentFormat, QSettings::SystemScope, "qtscrob", "qtscrob");
            current.setValue("application/utc_offset", 7200);
            current.setValue("Last.fm/enabled", true);
            current.setValue("Last.fm/username", "system-account");
            current.setValue("Last.fm/password_hash", "system-account-hash");
            current.sync();
            QCOMPARE(current.status(), QSettings::NoError);
            QSettings legacy(QSettings::NativeFormat, QSettings::SystemScope, "qtscrob");
            legacy.setValue("application/tz_override", true);
            legacy.setValue("application/utc_offset", 3600);
            legacy.setValue("Last.fm/enabled", true);
            legacy.sync();
            QCOMPARE(legacy.status(), QSettings::NoError);
        }
        // Show the isolated system fixtures really are visible through ordinary fallbacks.
        QSettings ordinary(test_environment::currentFormat, QSettings::UserScope, "qtscrob", "qtscrob");
        QCOMPARE(ordinary.value("Last.fm/username").toString(), "system-account");
        Conf conf;
        QSignalSpy logs(&conf, &Conf::add_log);
        QVERIFY(conf.load_config());
        QCOMPARE(conf.utc_offset, 0);
        QVERIFY(!conf.tz_override);
        QVERIFY(!conf.sites[CONFIG_LASTFM].enabled);
        QVERIFY(conf.sites[CONFIG_LASTFM].username.isEmpty());
        QVERIFY(conf.sites[CONFIG_LASTFM].password_hash.isEmpty());
        for (const auto& log : logs) QVERIFY(!log[1].toString().contains("Moving config"));
        QVERIFY(conf.save_config());
        QSettings written(test_environment::currentFormat, QSettings::UserScope, "qtscrob", "qtscrob");
        written.setFallbacksEnabled(false);
        QCOMPARE(written.value("application/utc_offset").toInt(), 0);
        QVERIFY(!written.value("Last.fm/enabled").toBool());
    }

    void defaults() {
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
        for (const auto& site : conf.sites) {
            QVERIFY(!site.enabled);
            QVERIFY(site.username.isEmpty());
            QVERIFY(site.password_hash.isEmpty());
        }
    }

    void roundTrip() {
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
            for (int i = 0; i < CONFIG_NUM_SITES; ++i) {
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
        for (int i = 0; i < CONFIG_NUM_SITES; ++i) {
            QCOMPARE(loaded.sites[i].enabled, i != CONFIG_LIBREFM);
            QCOMPARE(loaded.sites[i].username, QString("user-%1").arg(i));
            QCOMPARE(loaded.sites[i].password_hash, QString("hash-%1").arg(i));
        }
        QCOMPARE(loaded.sites[CONFIG_CUSTOM].conf_name, "Offline test service");
        QCOMPARE(loaded.sites[CONFIG_CUSTOM].handshake_host, "scrobble.invalid");
    }

    void newCustomFile() {
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

    void existingCustomFile() {
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

    void invalidCustomLocation_data() {
        QTest::addColumn<QString>("location");
        QTest::addColumn<QString>("message");
        QTest::newRow("blank") << QString() << "Conf: Asked to use a blank custom config location";
        QTest::newRow("missing-parent") << qEnvironmentVariable("XDG_CONFIG_HOME") + "/absent/file.ini"
                                        << "Conf: Custom config parent folder does not exist";
    }

    void invalidCustomLocation() {
        QFETCH(QString, location);
        QFETCH(QString, message);
        Conf conf;
        QVERIFY(conf.load_config());
        QSignalSpy logs(&conf, &Conf::add_log);
        conf.set_custom_location(location);
        QCOMPARE(logs.size(), 2);
        QCOMPARE(logs.first().at(0).value<LOG_LEVEL>(), LOG_DEBUG);
        QCOMPARE(logs.first().at(1).toString(), "Conf: setting location to " + location);
        QCOMPARE(logs.last().at(0).value<LOG_LEVEL>(), LOG_ERROR);
        QCOMPARE(logs.last().at(1).toString(), message);
        conf.utc_offset = 99;
        QVERIFY(conf.save_config());
        Conf loaded;
        QVERIFY(loaded.load_config());
        QCOMPARE(loaded.utc_offset, 99);
    }

    void customSection() {
        const QSettings destination(test_environment::currentFormat, QSettings::UserScope, "qtscrob", "qtscrob");
        QFile persisted(destination.fileName());
        Conf conf;
        const QHash<QString, QVariant> values{{"size", QSize(600, 450)}, {"maximized", true}, {"nested/key", "value"}};
        conf.save_custom("window", values);
        QVERIFY(persisted.open(QIODevice::ReadOnly));
        QVERIFY(persisted.readAll().contains("maximized=true"));
        persisted.close();
        QCOMPARE(conf.load_custom("window"), values);
        QVERIFY(conf.load_custom("missing").isEmpty());
        conf.save_custom("window", {{"maximized", false}});
        QVERIFY(persisted.open(QIODevice::ReadOnly));
        QVERIFY(persisted.readAll().contains("maximized=false"));
        QCOMPARE(conf.load_custom("window").value("size").toSize(), QSize(600, 450));
        QCOMPARE(conf.load_custom("window").value("maximized").toBool(), false);
    }

    void invalidFormat() {
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

    void saveFailure() {
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
        QCOMPARE(logs.size(), 1);
        QCOMPARE(logs.last().at(0).value<LOG_LEVEL>(), LOG_ERROR);
        QCOMPARE(logs.last().at(1).toString(), "Conf: Access Error");
    }

    void customLocationSwitchingPreservesBothFiles() {
        QTemporaryDir directory;
        const QString first = directory.filePath("first.ini");
        const QString second = directory.filePath("second.ini");
        Conf conf;
        conf.set_custom_location(first);
        conf.utc_offset = 3600;
        QVERIFY(conf.save_config());
        conf.save_custom("window", {{"name", "First Window"}});
        conf.set_custom_location(second);
        QCOMPARE(conf.utc_offset, 0);
        QVERIFY(conf.load_custom("window").isEmpty());
        conf.utc_offset = -7200;
        QVERIFY(conf.save_config());
        conf.save_custom("window", {{"name", "Second Window"}});
        conf.set_custom_location(first);
        QCOMPARE(conf.utc_offset, 3600);
        QCOMPARE(conf.load_custom("window").value("name").toString(), "First Window");
        conf.set_custom_location(QString());
        conf.utc_offset = 5400;
        QVERIFY(conf.save_config());
        QSettings unchanged(second, QSettings::IniFormat);
        QCOMPARE(unchanged.value("application/utc_offset").toInt(), -7200);
        QCOMPARE(unchanged.value("window/name").toString(), "Second Window");
        conf.set_custom_location(first);
        QCOMPARE(conf.utc_offset, 5400);
    }

    void savingPreservesUnknownSettingsAndDoesNotLogCredentials() {
        QTemporaryDir directory;
        const QString file = directory.filePath("settings.ini");
        {
            QSettings initial(file, QSettings::IniFormat);
            initial.setValue("unrelated/key", "keep me");
            initial.setValue("application/customFlag", true);
        }
        Conf conf;
        QSignalSpy logs(&conf, &Conf::add_log);
        conf.set_custom_location(file);
        conf.proxy_pass = "sensitive-proxy-secret";
        conf.sites[CONFIG_CUSTOM].username = "sensitive-user";
        conf.sites[CONFIG_CUSTOM].password_hash = "sensitive-password-hash";
        const int logsBeforeSave = logs.size();
        QVERIFY(conf.save_config());
        QCOMPARE(logs.size(), logsBeforeSave + 1);
        QCOMPARE(logs.last().at(0).value<LOG_LEVEL>(), LOG_DEBUG);
        QCOMPARE(logs.last().at(1).toString(), "Conf: Success on read/write");
        QVERIFY(conf.load_config());
        QSettings actual(file, QSettings::IniFormat);
        QCOMPARE(actual.value("unrelated/key").toString(), "keep me");
        QVERIFY(actual.value("application/customFlag").toBool());
        for (const auto& log : logs) {
            const auto text = log.at(1).toString();
            QVERIFY(!text.contains("sensitive-proxy-secret"));
            QVERIFY(!text.contains("sensitive-user"));
            QVERIFY(!text.contains("sensitive-password-hash"));
        }
        QCOMPARE(logs.last().at(0).value<LOG_LEVEL>(), LOG_DEBUG);
        QCOMPARE(logs.last().at(1).toString(), "Conf: Success on read/write");
    }

    void migrationFromVersion011PreservesSections() {
        QSettings source("qtscrob");
        source.setFallbacksEnabled(false);
        source.setValue("application/tz_override", true);
        source.setValue("application/utc_offset", 2700);
        source.setValue("application/mru", QStringList{"/fictional/device"});
        source.setValue("window/size", QSize(700, 500));
        source.setValue("Last.fm/enabled", true);
        source.setValue("Last.fm/username", "old-user");
        source.setValue("Last.fm/password_hash", "old-hash");
        source.sync();
        QCOMPARE(source.status(), QSettings::NoError);
        const QSettings destination(test_environment::currentFormat, QSettings::UserScope, "qtscrob", "qtscrob");
        Conf conf;
        QSignalSpy logs(&conf, &Conf::add_log);
        QVERIFY(conf.load_config());
        QCOMPARE(logs.size(), 2);
        QCOMPARE(logs.first().at(0).value<LOG_LEVEL>(), LOG_INFO);
        QCOMPARE(logs.first().at(1).toString(), "Moving config to new location");
        QCOMPARE(logs.last().at(0).value<LOG_LEVEL>(), LOG_DEBUG);
        QCOMPARE(logs.last().at(1).toString(), "Conf: Success on read/write");
        QFile persisted(destination.fileName());
        QVERIFY(persisted.open(QIODevice::ReadOnly));
        const auto bytes = persisted.readAll();
        QVERIFY(bytes.contains("utc_offset=2700"));
        QVERIFY(bytes.contains("username=old-user"));
        QVERIFY(!QFile::exists(source.fileName()));
        QVERIFY(source.allKeys().isEmpty());
        QCOMPARE(conf.utc_offset, 2700);
        QVERIFY(conf.tz_override);
        QCOMPARE(conf.mru, QStringList{"/fictional/device"});
        QVERIFY(conf.sites[CONFIG_LASTFM].enabled);
        QCOMPARE(conf.sites[CONFIG_LASTFM].username, "old-user");
        QCOMPARE(conf.sites[CONFIG_LASTFM].password_hash, "old-hash");
        QCOMPARE(conf.load_custom("window").value("size").toSize(), QSize(700, 500));
        QVERIFY(conf.load_config());
        QCOMPARE(conf.utc_offset, 2700);
    }

    void migrationDoesNotDeleteSourceOnFailedSave() {
        {
            QSettings old("qtscrob");
            old.setValue("application/tz_override", true);
            old.setValue("application/lastDirectory", "/fictional/player");
            old.setValue("application/username", "preserved-user");
        }
        QTemporaryDir directory;
        Conf conf;
        // A directory is a deterministic inaccessible INI target, including as root.
        conf.set_custom_location(directory.path());
        QVERIFY(!conf.load_config());
        QSettings old("qtscrob");
        QCOMPARE(old.value("application/username").toString(), "preserved-user");
        QCOMPARE(old.value("application/lastDirectory").toString(), "/fictional/player");
    }

    void version010MigrationPropagatesWriteFailure() {
        QTemporaryDir directory;
        const QString parent = directory.filePath("parent");
        const QString backup = directory.filePath("backup");
        QVERIFY(QDir().mkpath(parent));
        const QString file = parent + "/config.ini";
        Conf conf;
        conf.set_custom_location(file);
        {
            QSettings legacy(file, QSettings::IniFormat);
            legacy.setValue("application/lastDirectory", "/fictional/player");
            legacy.setValue("application/username", "preserved-user");
        }
        bool blocked = false;
        connect(&conf, &Conf::add_log, this, [&](LOG_LEVEL, const QString& message) {
            if (message == "Conf: Upgrading from 0.10 or before") {
                blocked = QDir().rename(parent, backup);
                QFile blocker(parent);
                blocked = blocker.open(QIODevice::WriteOnly) && blocked;
            }
        });
        QSignalSpy logs(&conf, &Conf::add_log);
        QVERIFY(!conf.load_config());
        QVERIFY(blocked);
        QCOMPARE(logs.last().at(0).value<LOG_LEVEL>(), LOG_ERROR);
        QCOMPARE(logs.last().at(1).toString(), "Conf: Access Error");
        QSettings preserved(backup + "/config.ini", QSettings::IniFormat);
        QCOMPARE(preserved.value("application/username").toString(), "preserved-user");
    }

    void legacyMigration_data() {
        QTest::addColumn<bool>("enabledFlags");
        QTest::newRow("legacy-enabled-flags") << true;
        QTest::newRow("legacy-disabled-flags") << false;
    }

    void legacyMigration() {
        QFETCH(bool, enabledFlags);
        {
            QSettings old("qtscrob");
            old.setValue("application/lastDirectory", "/example/player");
            old.setValue("application/username", "legacy-user");
            old.setValue("application/password", "legacy-hash");
            old.setValue("application/utc_offset", -3600);
            old.setValue("application/tz_override", enabledFlags);
            old.setValue("application/checkAutoOpen", false);
            old.setValue("application/checkDelPlayList", enabledFlags);
            old.setValue("proxy/proxy_host", "legacy.invalid");
            old.setValue("proxy/proxy_port", 8080);
            old.setValue("proxy/proxy_user", "old-user");
            old.setValue("proxy/proxy_pass", "old-password");
        }
        Conf conf;
        QSignalSpy logs(&conf, &Conf::add_log);
        QVERIFY(conf.load_config());
        QCOMPARE(logs.size(), 4);
        QCOMPARE(logs.at(0).at(0).value<LOG_LEVEL>(), LOG_INFO);
        QCOMPARE(logs.at(0).at(1).toString(), "Moving config to new location");
        QCOMPARE(logs.at(1).at(0).value<LOG_LEVEL>(), LOG_INFO);
        QCOMPARE(logs.at(1).at(1).toString(), "Conf: Upgrading from 0.10 or before");
        for (const int index : {2, 3}) {
            QCOMPARE(logs.at(index).at(0).value<LOG_LEVEL>(), LOG_DEBUG);
            QCOMPARE(logs.at(index).at(1).toString(), "Conf: Success on read/write");
        }
        QCOMPARE(conf.mru, QStringList({"/example/player"}));
        QCOMPARE(conf.utc_offset, -3600);
        QCOMPARE(conf.tz_override, enabledFlags);
        QVERIFY(!conf.auto_open);
        QCOMPARE(conf.del_apple_playlist, enabledFlags);
        QVERIFY(conf.sites[CONFIG_LASTFM].enabled);
        QCOMPARE(conf.sites[CONFIG_LASTFM].username, "legacy-user");
        QCOMPARE(conf.sites[CONFIG_LASTFM].password_hash, "legacy-hash");
        QCOMPARE(conf.proxy_host, "legacy.invalid");
        QCOMPARE(conf.proxy_port, 8080);
        QCOMPARE(conf.proxy_user, "old-user");
        QCOMPARE(conf.proxy_pass, "old-password");
        QSettings old("qtscrob");
        QVERIFY(old.allKeys().isEmpty());
        QSettings destination(test_environment::currentFormat, QSettings::UserScope, "qtscrob", "qtscrob");
        destination.setFallbacksEnabled(false);
        QCOMPARE(destination.value("application/version").toString(), CLIENT_VERSION);
        QCOMPARE(destination.value("application/mru").toStringList(), QStringList{"/example/player"});
        QCOMPARE(destination.value("application/tz_override").toBool(), enabledFlags);
        QCOMPARE(destination.value("application/del_apple_playlist").toBool(), enabledFlags);
        QVERIFY(!destination.contains("application/lastDirectory"));
        QVERIFY(!destination.contains("application/checkDelPlayList"));
        QVERIFY(!destination.contains("application/username"));
        QVERIFY(!destination.contains("application/password"));
        QCOMPARE(destination.value("Last.fm/username").toString(), "legacy-user");
        QCOMPARE(destination.value("Last.fm/password_hash").toString(), "legacy-hash");
        QCOMPARE(destination.value("proxy/proxy_host").toString(), "legacy.invalid");
        logs.clear();
        QVERIFY(conf.load_config());
        QCOMPARE(conf.mru.size(), 1);
        QCOMPARE(logs.size(), 1);
        QCOMPARE(logs.first().at(1).toString(), "Conf: Success on read/write");
    }
};

int main(int argc, char** argv) {
    QTemporaryDir home;
    if (!home.isValid()) return 1;
    test_environment::isolate(home.path());
    QCoreApplication application(argc, argv);
    qRegisterMetaType<LOG_LEVEL>();
    ConfTest tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_conf.moc"
