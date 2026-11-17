// SPDX-License-Identifier: GPL-2.0-only
#include "dbcache.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

class DBCacheTest : public QObject
{
    Q_OBJECT

    static bool seedDatabase(const QString &filename, const QStringList &statements)
    {
        const QString name = "seed_" + QUuid::createUuid().toString();
        bool success;
        {
            QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", name);
            database.setDatabaseName(filename);
            success = database.open();
            for (const QString &statement : statements)
            {
                QSqlQuery query(database);
                if (!success || !query.exec(statement))
                {
                    success = false;
                    break;
                }
            }
            database.close();
        }
        QSqlDatabase::removeDatabase(name);
        return success;
    }

private slots:
    void init()
    {
        const QString config = qEnvironmentVariable("XDG_CONFIG_HOME");
        QVERIFY(!config.isEmpty());
        QVERIFY(QDir(config).removeRecursively());
        QVERIFY(QDir().mkpath(config));
    }

    void cleanup()
    {
        QVERIFY2(QSqlDatabase::connectionNames().isEmpty(),
                 qPrintable(QSqlDatabase::connectionNames().join(',')));
    }

    void roundTrip()
    {
        DBCache cache;
        QVERIFY(cache.init());
        QCOMPARE(cache.get_track_length("missing", "track"), -1);
        cache.set_track_length("Artist's \"quoted\" name", QString::fromUtf8("海辺"), 217);
        QCOMPARE(cache.get_track_length("Artist's \"quoted\" name", QString::fromUtf8("海辺")), 217);
        QCOMPARE(cache.get_track_length("Other artist", QString::fromUtf8("海辺")), -1);
        cache.set_track_length("Artist's \"quoted\" name", QString::fromUtf8("海辺"), 301);
        QCOMPARE(cache.get_track_length("Artist's \"quoted\" name", QString::fromUtf8("海辺")), 301);
    }

    void simultaneousCaches()
    {
        DBCache first;
        QVERIFY(first.init());
        first.set_track_length("artist", "title", 180);
        {
            DBCache second;
            QVERIFY(second.init());
            QCOMPARE(second.get_track_length("artist", "title"), 180);
            second.set_track_length("artist", "second", 240);
        }
        QCOMPARE(first.get_track_length("artist", "title"), 180);
        QCOMPARE(first.get_track_length("artist", "second"), 240);
    }

    void releasesConnection()
    {
        const QStringList before = QSqlDatabase::connectionNames();
        {
            DBCache cache;
            QVERIFY(cache.init());
        }
        QCOMPARE(QSqlDatabase::connectionNames(), before);
    }

    void repeatedInitAndReopen()
    {
        QTemporaryDir directory;
        const auto clock = [] { return qint64(5000000000); };
        {
            DBCache first(directory.path(), clock);
            QVERIFY(first.init());
            first.set_track_length("artist", "title", 450);
            QVERIFY(first.init());
            QCOMPARE(first.get_track_length("artist", "title"), 450);
        }
        DBCache reopened(directory.path(), clock);
        QVERIFY(reopened.init());
        QCOMPARE(reopened.get_track_length("artist", "title"), 450);
    }

    void emptyClockUsesSystemTime()
    {
        QTemporaryDir directory;
        DBCache cache(directory.path(), {});
        QVERIFY(cache.init());
        cache.set_track_length("artist", "title", 180);
        QCOMPARE(cache.get_track_length("artist", "title"), 180);
    }

    void expiry_data()
    {
        QTest::addColumn<qint64>("elapsed");
        QTest::addColumn<int>("expected");
        QTest::newRow("new") << qint64(0) << 210;
        QTest::newRow("before-boundary") << qint64(MAX_TRACK_LENGTH_AGE - 1) << 210;
        QTest::newRow("exact-boundary") << qint64(MAX_TRACK_LENGTH_AGE) << -1;
        QTest::newRow("after-boundary") << qint64(MAX_TRACK_LENGTH_AGE + 1) << -1;
    }

    void expiry()
    {
        QFETCH(qint64, elapsed);
        QFETCH(int, expected);
        QTemporaryDir directory;
        qint64 now = 5000000000; // Exercises timestamps beyond a 32-bit epoch.
        DBCache cache(directory.path(), [&now] { return now; });
        QVERIFY(cache.init());
        cache.set_track_length("artist", "title", 210);
        now += elapsed;
        QCOMPARE(cache.get_track_length("artist", "title"), expected);
        cache.set_track_length("artist", "title", 211);
        QCOMPARE(cache.get_track_length("artist", "title"), 211);
    }

    void parentCannotBeCreated()
    {
        QTemporaryDir directory;
        const QString parent = directory.filePath("blocking-file");
        QFile file(parent);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        DBCache cache(parent + "/child", [] { return qint64(1); });
        QSignalSpy logs(&cache, &DBCache::add_log);
        QVERIFY(!cache.init());
        QCOMPARE(logs.last().at(1).toString(), "DB: path could not be created: " + parent + "/child");
    }

    void databaseCannotOpen()
    {
        QTemporaryDir directory;
        QVERIFY(QDir().mkpath(directory.filePath("qtscrob.db")));
        DBCache cache(directory.path(), [] { return qint64(1); });
        QSignalSpy logs(&cache, &DBCache::add_log);
        QVERIFY(!cache.init());
        QVERIFY(logs.last().at(1).toString().startsWith("DB: failed to open:"));
    }

    void incompatibleSchema_data()
    {
        QTest::addColumn<QStringList>("statements");
        QTest::newRow("missing-version")
            << QStringList{"CREATE TABLE schema (name TEXT, value TEXT)",
                           "CREATE TABLE track_length (artist TEXT, title TEXT, length INT, timestamp INT)"};
        QTest::newRow("future-version")
            << QStringList{"CREATE TABLE schema (name TEXT, value TEXT)",
                           "INSERT INTO schema VALUES ('version', '99')",
                           "CREATE TABLE track_length (artist TEXT, title TEXT, length INT, timestamp INT)"};
        QTest::newRow("missing-track-table")
            << QStringList{"CREATE TABLE schema (name TEXT, value TEXT)",
                           "INSERT INTO schema VALUES ('version', '1')"};
        QTest::newRow("invalid-schema-table")
            << QStringList{"CREATE TABLE schema (other TEXT)",
                           "CREATE TABLE track_length (artist TEXT, title TEXT, length INT, timestamp INT)"};
        QTest::newRow("invalid-track-columns")
            << QStringList{"CREATE TABLE schema (name TEXT, value TEXT)",
                           "INSERT INTO schema VALUES ('version', '1')",
                           "CREATE TABLE track_length (other TEXT)"};
    }

    void incompatibleSchema()
    {
        QFETCH(QStringList, statements);
        QTemporaryDir directory;
        QVERIFY(seedDatabase(directory.filePath("qtscrob.db"), statements));
        DBCache cache(directory.path(), [] { return qint64(1); });
        QSignalSpy logs(&cache, &DBCache::add_log);
        QVERIFY(!cache.init());
        QVERIFY(!cache.init()); // An open connection alone does not establish a usable cache.
        QVERIFY(logs.last().at(1).toString().startsWith("DB: incompatible cache schema"));
    }

    void schemaCreationFailure_data()
    {
        QTest::addColumn<QString>("statement");
        QTest::addColumn<QString>("error");
        QTest::newRow("existing-view") << "CREATE VIEW schema AS SELECT 1 AS other"
                                      << "DB: failed to create table schema:";
        QTest::newRow("existing-track-table") << "CREATE TABLE track_length (other TEXT)"
                                             << "DB: failed to create table track_length:";
    }

    void schemaCreationFailure()
    {
        QFETCH(QString, statement);
        QFETCH(QString, error);
        QTemporaryDir directory;
        QVERIFY(seedDatabase(directory.filePath("qtscrob.db"), {statement}));
        DBCache cache(directory.path(), [] { return qint64(1); });
        QSignalSpy logs(&cache, &DBCache::add_log);
        QVERIFY(!cache.init());
        bool found = false;
        for (const auto &log : logs)
            found = found || log.at(1).toString().startsWith(error);
        QVERIFY(found);
        QCOMPARE(logs.last().at(1).toString(), "DB: failed to create a new DB");
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
    DBCacheTest tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_dbcache.moc"
