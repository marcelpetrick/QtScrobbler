// SPDX-License-Identifier: GPL-2.0-only
#include "qtscrob.h"

#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

class GuiTest : public QObject
{
    Q_OBJECT

private slots:
    void importEditAndClose()
    {
        QCOMPARE(QString::fromLatin1(qVersion()), QString::fromLatin1(QT_VERSION_STR));
        QVERIFY(QSqlDatabase::isDriverAvailable("QSQLITE"));
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QFile::copy(EXAMPLE_LOG, directory.filePath(".scrobbler.log")));
        QTScrob window;
        window.scrob->conf->display_utc = true;
        window.setupWindow();
        window.show();
        auto *table = window.findChild<QTableWidget *>();
        QVERIFY(table);
        QCOMPARE(table->rowCount(), 0);
        QCOMPARE(table->columnCount(), 7);
        QCOMPARE(table->horizontalHeaderItem(0)->text(), "Artist");
        QCOMPARE(table->horizontalHeaderItem(6)->text(), "Date (UTC)");
        QSignalSpy parsed(window.scrob, &Scrobble::parsing_opened);

        window.parse(SCROBBLE_LOG, directory.path());

        QTRY_COMPARE_WITH_TIMEOUT(parsed.count(), 1, 2000);
        QCOMPARE(parsed.first().at(0).toBool(), true);
        QCOMPARE(table->rowCount(), 3);
        QCOMPARE(window.scrob->get_num_tracks(), 3);
        QCOMPARE(table->item(0, 0)->text(), "Example Artist");
        QCOMPARE(table->item(0, 2)->text(), "Morning Signal");
        QCOMPARE(table->item(0, 3)->text(), "1");
        QCOMPARE(table->item(0, 4)->text(), "210");
        QCOMPARE(table->item(0, 5)->text(), "L");
        QCOMPARE(table->item(0, 6)->text(), "2026-11-17 08:00:00");
        if (qEnvironmentVariableIsSet("QTSCROBBLER_SCREENSHOT_PATH")) {
            QApplication::processEvents();
            QVERIFY(window.grab().save(qEnvironmentVariable("QTSCROBBLER_SCREENSHOT_PATH")));
        }
        table->item(0, 0)->setText("Edited Artist");
        QCOMPARE(window.scrob->get_track(0).artist, "Edited Artist");
        table->item(0, 2)->setText("Edited Title");
        QCOMPARE(window.scrob->get_track(0).title, "Edited Title");
        window.loadtable();
        table->item(0, 1)->setText("Edited Album");
        QCOMPARE(window.scrob->get_track(0).album, "Edited Album");

        window.settings();
        auto *settings = window.findChild<QDialog *>();
        QVERIFY(settings);
        QVERIFY(settings->isVisible());
        window.settings_close();
        QVERIFY(window.findChild<QDialog *>() == nullptr);
        window.settings();
        QVERIFY(window.findChild<QDialog *>() != nullptr);
        window.settings_close();

        QSignalSpy cleared(window.scrob, &Scrobble::parsing_cleared);
        window.scrob->parser_clear();
        QCOMPARE(cleared.count(), 1);
        QVERIFY(cleared.first().at(0).toBool());
        QVERIFY(!window.scrob->get_parser_busy());
        QVERIFY(!QFile::exists(directory.filePath(".scrobbler.log")));
        QVERIFY(QFile::exists(EXAMPLE_LOG));
        QVERIFY(window.close());
        QVERIFY(!window.isVisible());
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    qputenv("HOME", home.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", home.path().toUtf8());
    qputenv("XDG_CACHE_HOME", home.path().toUtf8());
    QApplication application(argc, argv);
    GuiTest tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_gui.moc"
