// SPDX-License-Identifier: GPL-2.0-only
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QIcon>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QtTest>

#include "about.h"
#include "console.h"
#include "help.h"
#include "http_fixture.h"
#include "progress.h"
#include "qtscrob.h"
#include "settings.h"
#include "test_environment.h"

namespace {
QStringList guiWarnings;
QtMessageHandler previousGuiHandler = nullptr;

void captureGuiMessages(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    // The offscreen platform cannot negotiate native window size hints. This exact
    // platform diagnostic is the only warning allowance in the headless suite.
    const bool platformAllowance = QGuiApplication::platformName() == "offscreen" &&
                                   message == "This plugin does not support propagateSizeHints()";
    if ((type == QtWarningMsg || type == QtCriticalMsg) && !platformAllowance) guiWarnings.append(message);
    if (previousGuiHandler) previousGuiHandler(type, context, message);
}
}  // namespace

class GuiTest : public QObject {
    Q_OBJECT
    static QPushButton* button(QWidget& widget, const QString& text) {
        for (auto* candidate : widget.findChildren<QPushButton*>())
            if (candidate->text() == text) return candidate;
        return nullptr;
    }

    static QAction* menuAction(QWidget& window, const QString& text) {
        for (auto* candidate : window.findChildren<QAction*>(Qt::FindDirectChildrenOnly))
            if (candidate->text() == text) return candidate;
        return nullptr;
    }

    static bool statusBarShows(QTScrob& window, const QString& text) {
        for (auto* label : static_cast<QMainWindow&>(window).statusBar()->findChildren<QLabel*>())
            if (label->isVisibleTo(&window) && label->text() == text) return true;
        return false;
    }

    static void answer(QMessageBox::StandardButton answer) {
        QTimer::singleShot(0, qApp, [answer] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (box) box->button(answer)->click();
        });
    }

    static QList<Site_UI*> siteWidgets(Settings& settings) {
        QList<Site_UI*> sites;
        auto* tabs = settings.findChild<QTabWidget*>("tab_widget_sites");
        for (int i = 0; i < tabs->count(); ++i) sites.append(qobject_cast<Site_UI*>(tabs->widget(i)));
        return sites;
    }

    static bool fixture(QTemporaryDir& directory) {
        return directory.isValid() && QFile::copy(EXAMPLE_LOG, directory.filePath(".scrobbler.log"));
    }

   private slots:
    void init() {
        guiWarnings.clear();
        previousGuiHandler = qInstallMessageHandler(captureGuiMessages);
    }

    void cleanup() {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        qInstallMessageHandler(previousGuiHandler);
        QVERIFY2(guiWarnings.isEmpty(), qPrintable(guiWarnings.join('\n')));
        QVERIFY(QSqlDatabase::connectionNames().isEmpty());
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    }

    void processOptions_data() {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<int>("expectedExit");
        QTest::addColumn<QByteArray>("expectedError");
        QTest::newRow("help") << QStringList{"--help"} << 0 << QByteArray();
        QTest::newRow("short-help") << QStringList{"-h"} << 0 << QByteArray();
        QTest::newRow("config-help") << QStringList{"-c", "/tmp/fictional-config.ini", "-h"} << 0 << QByteArray();
        QTest::newRow("verbosity-minimum") << QStringList{"-v", "0", "--help"} << 0 << QByteArray();
        QTest::newRow("verbosity-maximum") << QStringList{"--verbose=3", "--help"} << 0 << QByteArray();
        for (const QString value : {"invalid", "-1", "4", "1.5", "2147483648"})
            QTest::newRow(qPrintable("invalid-verbosity-" + value))
                << QStringList{"--verbose=" + value, "--help"} << 2
                << QByteArray("Verbosity must be an integer between 0 and 3\n");
        for (const QString option : {"--unknown", "--config", "--verbose"})
            QTest::newRow(qPrintable(option))
                << QStringList{option} << 2
                << QByteArray("Invalid option or missing argument: ") + option.toUtf8() + '\n';
        QTest::newRow("empty-config") << QStringList{"--config=", "--help"} << 2
                                      << QByteArray("Configuration path must not be empty\n");
        QTest::newRow("positional") << QStringList{"extra", "words"} << 2
                                    << QByteArray("Unknown option argument: extra words\n");
    }

    void processOptions() {
        QFETCH(QStringList, arguments);
        QFETCH(int, expectedExit);
        QFETCH(QByteArray, expectedError);
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QProcess child;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("HOME", home.path());
        environment.insert("XDG_CONFIG_HOME", home.path());
        environment.insert("XDG_CACHE_HOME", home.path());
        environment.insert("XDG_RUNTIME_DIR", home.path());
        environment.insert("QT_QPA_PLATFORM", "offscreen");
        environment.insert("LC_ALL", "C.UTF-8");
        environment.remove("POSIXLY_CORRECT");
        child.setProcessEnvironment(environment);
        child.start(GUI_BINARY, arguments);
        QVERIFY(child.waitForStarted(1000));
        if (!child.waitForFinished(3000)) {
            child.kill();
            child.waitForFinished(1000);
            QFAIL("GUI option process exceeded its timeout");
        }
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), expectedExit);
        QCOMPARE(test_environment::lines(child.readAllStandardError()), expectedError);
        const auto output = test_environment::lines(child.readAllStandardOutput());
        if (expectedExit == 0) {
            QVERIFY(output.startsWith(QByteArray("QTScrob (v") + CLIENT_VERSION + ")\n"));
            QVERIFY(output.contains("--config=PATH"));
        }
    }

    void chooserCancelAcceptAndErrors() {
        QTemporaryDir directory;
        QVERIFY(fixture(directory));
        QTScrob window;
        window.setupWindow();
        window.scrob->conf->auto_open = false;
        window.scrob->conf->mru.clear();
        bool sawChooser = false;
        bool selectsDirectories = false;
        QTimer::singleShot(0, &window, [&] {
            auto* chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
            sawChooser = chooser != nullptr;
            if (chooser) {
                selectsDirectories =
                    chooser->fileMode() == QFileDialog::Directory && chooser->testOption(QFileDialog::ShowDirsOnly);
                chooser->reject();
            }
        });
        window.open_log();
        QVERIFY(sawChooser);
        QVERIFY(selectsDirectories);
        QCOMPARE(window.scrob->get_num_tracks(), 0);
        QVERIFY(window.findChildren<QFileDialog*>().isEmpty());
        QTemporaryDir previous;
        QVERIFY(previous.isValid());
        window.scrob->conf->mru = {directory.path(), previous.path()};
        bool remembersPastLocation = false;
        QTimer::singleShot(0, &window, [&] {
            auto* chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
            if (chooser) {
                remembersPastLocation = chooser->history().contains(previous.path());
                chooser->setDirectory(directory.path());
                static_cast<QDialog*>(chooser)->accept();
            }
        });
        auto* openAction = menuAction(window, "Open");
        QVERIFY(openAction);
        openAction->trigger();
        QVERIFY(remembersPastLocation);
        QCOMPARE(window.scrob->get_num_tracks(), 3);
        QVERIFY(window.findChildren<QFileDialog*>().isEmpty());
        window.scrob->cancel_submission();
        answer(QMessageBox::Ok);
        QCoreApplication::processEvents();
        QTemporaryDir missing;
        QString errorText;
        QTimer::singleShot(0, &window, [&] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (box) {
                errorText = box->text();
                box->button(QMessageBox::Ok)->click();
            }
        });
        window.parse(SCROBBLE_LOG, missing.path());
        QVERIFY(errorText.startsWith("Error parsing:"));
        QVERIFY(!window.scrob->get_parser_busy());
        window.scrob->conf->mru = {missing.path()};
        window.scrob->conf->auto_open = false;
        sawChooser = false;
        QTimer::singleShot(0, &window, [&] {
            auto* chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
            sawChooser = chooser != nullptr;
            if (chooser) chooser->reject();
        });
        window.open_ipod();
        QVERIFY(sawChooser);
        QVERIFY(window.findChildren<QFileDialog*>().isEmpty());
        // Automatic reopen falls back to the chooser when a remembered player
        // was removed, or when there is no remembered directory yet.
        window.scrob->conf->auto_open = true;
        for (const QStringList& history : {QStringList{}, QStringList{missing.filePath("removed-player")}}) {
            window.scrob->conf->mru = history;
            sawChooser = false;
            QTimer::singleShot(0, &window, [&] {
                auto* chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
                sawChooser = chooser != nullptr;
                if (chooser) chooser->reject();
            });
            window.open_log();
            QVERIFY(sawChooser);
            QVERIFY(window.findChildren<QFileDialog*>().isEmpty());
            QCOMPARE(window.scrob->get_num_tracks(), 0);
        }
    }

    void emptySubmissionAboutQtAndWindowState() {
        QTScrob window;
        window.setupWindow();
        answer(QMessageBox::Ok);
        window.scrobble();
        QVERIFY(window.scrob->submissions.isEmpty());
        bool showedAboutQt = false;
        QTimer::singleShot(0, &window, [&] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (box) {
                showedAboutQt = true;
                box->accept();
            }
        });
        auto* aboutQt = menuAction(window, "About Qt");
        QVERIFY(aboutQt);
        aboutQt->trigger();
        QCoreApplication::processEvents();
        QVERIFY(showedAboutQt);
        window.resize(680, 460);
        window.move(30, 40);
        window.saveWindow();
        QTScrob restored;
        restored.setupWindow();
        QCOMPARE(restored.size(), QSize(680, 460));
        QCOMPARE(restored.pos(), window.pos());
        window.setWindowState(Qt::WindowMaximized);
        window.saveWindow();
        QTScrob maximized;
        maximized.setupWindow();
        QVERIFY(maximized.isMaximized());
        window.scrob->conf->save_custom("window", {{"maximized", false}});
        window.settings();  // Destructor closes an open settings dialog safely.
    }

    void closingWindowPersistsLocationAndSettings() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString configPath = directory.filePath("window.ini");
        QSize size;
        QPoint position;
        {
            QTScrob window;
            window.scrob->conf->set_custom_location(configPath);
            window.setupWindow();
            window.resize(720, 480);
            window.move(40, 60);
            window.show();
            size = window.size();
            position = window.pos();
            window.scrob->conf->auto_open = false;
            auto* exit = menuAction(window, "Exit");
            QVERIFY(exit);
            exit->trigger();
            QVERIFY(!window.isVisible());
        }

        QTScrob reopened;
        reopened.scrob->conf->set_custom_location(configPath);
        reopened.setupWindow();
        QCOMPARE(reopened.size(), size);
        QCOMPARE(reopened.pos(), position);
        QVERIFY(!reopened.scrob->conf->auto_open);
    }

    void settingsPositiveTimezoneAndRecalculation() {
        QTemporaryDir directory;
        QVERIFY(fixture(directory));
        QTScrob window;
        window.scrob->conf->display_utc = true;
        window.setupWindow();
        auto* edit = window.findChild<QDateTimeEdit*>();
        QVERIFY(edit);
        edit->setDateTime(QDateTime(QDate(2026, 11, 30), QTime(23, 30, 45)));
        QVERIFY(statusBarShows(window, "Adjust from (UTC):"));
        window.parse(SCROBBLE_LOG, directory.path());
        window.settings();
        auto* settings = window.findChild<Settings*>();
        QVERIFY(settings);
        settings->findChild<QCheckBox*>("chkDisplayUTC")->setChecked(false);
        settings->findChild<QGroupBox*>("grpManual")->setChecked(true);
        settings->findChild<QComboBox*>("cmb_tz_hour")->setCurrentIndex(17);
        settings->findChild<QComboBox*>("cmb_tz_min")->setCurrentIndex(3);
        settings->findChild<QPushButton*>("btnOK")->click();
        QCOMPARE(window.scrob->conf->utc_offset, 20700);
        QCOMPARE(edit->date(), QDate(2026, 12, 1));
        QCOMPARE(edit->time(), QTime(5, 15, 45));
        QVERIFY(statusBarShows(window, "Adjust from (Local):"));
        auto* table = window.findChild<QTableWidget*>();
        QCOMPARE(table->horizontalHeaderItem(6)->text(), "Date (Local)");
        QCOMPARE(table->item(0, 6)->text(), "2026-11-17 13:45:00");
        edit->setDateTime(QDateTime(QDate(2026, 11, 17), QTime(12, 0)));
        auto* recalc = button(window, "Recalc Date/Time");
        QVERIFY(recalc);
        QTest::mouseClick(recalc, Qt::LeftButton);
        const auto last = window.scrob->get_track(2);
        QCOMPARE(last.when + last.length, 1794916800 - 20700);
        QCOMPARE(table->item(2, 6)->text(), "2026-11-17 11:56:00");
        QTest::keyClick(table, Qt::Key_Right);
        QCOMPARE(window.scrob->get_num_tracks(), 3);
    }

    void isolatedClipboardAndProgressBoundaries() {
        if (QGuiApplication::platformName() != "offscreen")
            QSKIP("Clipboard mutation runs only on the isolated offscreen clipboard");
        QTScrob window;
        window.setupWindow();
        window.console();
        auto* console = window.findChild<Console*>();
        QVERIFY(console);
        auto* copy = button(*console, "Copy to clipboard");
        auto* text = console->findChild<QTextEdit*>();
        QVERIFY(copy && text);
        console->findChild<QComboBox*>()->setCurrentIndex(LOG_TRACE);
        copy->click();
        QCOMPARE(QApplication::clipboard()->text(), text->toPlainText());
        const QString copied = QApplication::clipboard()->text();
        window.scrob->clear_log();
        QTRY_VERIFY_WITH_TIMEOUT(text->toPlainText().isEmpty(), 1000);
        copy->click();
        QCOMPARE(QApplication::clipboard()->text(), copied);
        Progress empty(&window);
        QSignalSpy finished(window.scrob, &Scrobble::submission_finished);
        empty.show();
        QVERIFY(empty.isVisible());
        empty.cancel();
        QVERIFY(!empty.isVisible());
        QCOMPARE(finished.count(), 0);
        empty.update_progress(-1, 1, 2);  // Unknown site does not dereference a missing widget.
        QVERIFY(empty.findChildren<QProgressBar*>().isEmpty());
    }

    void consoleDisplaysOnlyNewLogsAtTheSelectedLevel() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString logPath = directory.filePath(".scrobbler.log");
        QFile log(logPath);
        QVERIFY(log.open(QIODevice::WriteOnly));
        const QByteArray malformed = "#AUDIOSCROBBLER/1.1\n#TZ/UTC\ninvalid client header\nignored\n";
        QCOMPARE(log.write(malformed), malformed.size());
        log.close();

        QTScrob window;
        window.scrob->clear_log();
        window.scrob->parser_clear();
        Console console(&window);
        QCOMPARE(console.size(), QSize(800, 600));

        auto* output = console.findChild<QTextEdit*>();
        auto* level = console.findChild<QComboBox*>();
        auto* label = console.findChild<QLabel*>();
        auto* copy = button(console, "Copy to clipboard");
        QVERIFY(output && level && label && copy);
        auto* layout = console.layout();
        QVERIFY(layout);
        QCOMPARE(layout->contentsMargins(), QMargins());
        QCOMPARE(layout->count(), 2);
        QCOMPARE(layout->itemAt(0)->widget(), output);
        auto* controls = layout->itemAt(1)->layout();
        QVERIFY(controls);
        QCOMPARE(controls->count(), 4);
        QCOMPARE(controls->itemAt(0)->widget(), label);
        QCOMPARE(label->text(), "Log Level:");
        QCOMPARE(controls->itemAt(1)->widget(), level);
        QVERIFY(controls->itemAt(2)->spacerItem());
        QCOMPARE(controls->itemAt(3)->widget(), copy);

        const QString initial = "ERROR : Parser: asked to clear without a live object";
        QCOMPARE(output->toPlainText(), initial);
        window.scrob->conf->tz_override = true;
        window.scrob->conf->utc_offset = 0;
        QVERIFY(window.scrob->parse(SCROBBLE_LOG, directory.path()));
        const QString opened = "\nINFO  : Opening log file: " + logPath + "\nINFO  : UTC Offset: 0";
        const QString rejected =
            "\nINFO  : Overriding detected timezone\nERROR : Parsing failed - missing #CLIENT header";
        const QString updated = initial + opened + rejected;
        QTRY_COMPARE_WITH_TIMEOUT(output->toPlainText(), updated, 1000);
        QSignalSpy changes(output, &QTextEdit::textChanged);
        // A full 250 ms polling interval with no new logs must leave the existing text alone.
        QVERIFY(!changes.wait(400));
        QCOMPARE(output->toPlainText(), updated);

        level->setCurrentIndex(LOG_DEBUG);
        QCOMPARE(output->toPlainText(),
                 initial + opened + "\nDEBUG : Log version: 1.1\nDEBUG : Log TZ: UTC" + rejected);
        window.scrob->clear_log();
        QTRY_VERIFY_WITH_TIMEOUT(output->toPlainText().isEmpty(), 1000);
        QVERIFY(!window.scrob->parse(SCROBBLE_NONE, directory.path()));
        QTRY_COMPARE_WITH_TIMEOUT(output->toPlainText(), QString("ERROR : Error - asked to use an unknown Parser!"),
                                  1000);
    }

    void settingsDaylightTimezoneProbe() {
        QTemporaryDir home;
        QProcess child;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("HOME", home.path());
        environment.insert("XDG_CONFIG_HOME", home.path());
        environment.insert("XDG_CACHE_HOME", home.path());
        environment.insert("XDG_RUNTIME_DIR", home.path());
        environment.insert("TZ", "Australia/Sydney");
        child.setProcessEnvironment(environment);
        child.start(QCoreApplication::applicationFilePath(), {"--settings-timezone-probe"});
        QVERIFY(child.waitForStarted(1000));
        QVERIFY(child.waitForFinished(3000));
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QVERIFY2(child.exitCode() == 0, child.readAllStandardOutput().constData());
    }

    void constructionBeforeSetupIsSafe() {
        QTScrob window;
        QCOMPARE(window.scrob->get_num_tracks(), 0);
    }

    void mainControlsRemainAccessible() {
        QTScrob window;
        window.setupWindow();
        window.show();
        QCOMPARE(window.windowTitle(), "QTScrobbler");
        auto* table = window.findChild<QTableWidget*>();
        QVERIFY(table);
        QVERIFY(table->alternatingRowColors());
        QCOMPARE(table->selectionBehavior(), QAbstractItemView::SelectRows);
        for (int column = 0; column < table->columnCount(); ++column)
            QCOMPARE(table->horizontalHeader()->sectionResizeMode(column), QHeaderView::Stretch);
        // QTScrob's private centralWidget member hides QMainWindow::centralWidget().
        QWidget* central = static_cast<QMainWindow&>(window).centralWidget();
        QVERIFY(central);
        QVERIFY(central->layout());
        QVERIFY(central->layout()->indexOf(table) >= 0);
        for (const QString text : {"Open .scrobbler.log", "Open iTunesDB", "Delete selected item", "Submit"}) {
            auto* control = button(window, text);
            QVERIFY(control);
            QVERIFY(control->isVisible());
            QVERIFY(!control->icon().isNull());
            QVERIFY(central->layout()->indexOf(control) >= 0);
        }
        QVERIFY(statusBarShows(window, "Ready"));
        auto* status = static_cast<QMainWindow&>(window).statusBar();
        auto* date = status->findChild<QDateTimeEdit*>();
        auto* recalc = button(*status, "Recalc Date/Time");
        QVERIFY(date && date->isVisible());
        QVERIFY(recalc && recalc->isVisible() && !recalc->icon().isNull());
#ifdef HAVE_MTP
        auto* mtp = button(window, "Open MTP Device");
        QVERIFY(mtp && mtp->isVisible() && !mtp->icon().isNull());
#endif
        // Theme icons can mask a broken bundle, so check the bundled fallbacks directly.
        for (const QString name :
             {"16x16/qtscrob.png", "128x128/qtscrob.png", "appointment-new.png", "document-open.png", "edit-clear.png",
              "help-browser.png", "mail-send-receive.png", "multimedia-player-iriver-s10_16.png",
              "preferences-system.png", "system-log-out.png"}) {
            const QString path = ":/resources/icons/" + name;
            QVERIFY2(QFile::exists(path), qPrintable(path));
            QVERIFY2(!QIcon(path).isNull(), qPrintable(path));
        }
        QStringList menus;
        for (auto* action : static_cast<QMainWindow&>(window).menuBar()->actions()) menus.append(action->text());
        QCOMPARE(menus, QStringList({"Global", "Help"}));
#ifdef HAVE_MTP
        const QStringList globalActions = {"Open", "Open iTunesDB", "Open MTP Device", "Settings", "", "Exit"};
#else
        const QStringList globalActions = {"Open", "Open iTunesDB", "Settings", "", "Exit"};
#endif
        const QList<QStringList> expectedActions = {globalActions, {"Help", "Console", "About", "About Qt"}};
        for (int index = 0; index < expectedActions.size(); ++index) {
            auto* menu = static_cast<QMainWindow&>(window).menuBar()->actions()[index]->menu();
            QVERIFY(menu);
            QStringList actual;
            for (auto* action : menu->actions()) actual.append(action->text());
            QCOMPARE(actual, expectedActions[index]);
        }
        for (const QString text : {"Open", "Open iTunesDB", "Settings", "Exit", "Help"}) {
            auto* action = menuAction(window, text);
            QVERIFY(action && !action->icon().isNull());
        }
#ifdef HAVE_MTP
        auto* openMtp = menuAction(window, "Open MTP Device");
        QVERIFY(openMtp && !openMtp->icon().isNull());
#endif
        QCOMPARE(menuAction(window, "Help")->shortcut(), QKeySequence(QKeySequence::HelpContents));
    }

    void editAllFieldsAndRejectInvalidValues() {
        QTemporaryDir directory;
        QVERIFY(fixture(directory));
        QTScrob window;
        window.scrob->conf->display_utc = true;
        window.setupWindow();
        window.parse(SCROBBLE_LOG, directory.path());
        auto* table = window.findChild<QTableWidget*>();
        QVERIFY(table);
        table->item(0, 0)->setText("Élan");
        QCOMPARE(window.scrob->get_track(0).artist, "Élan");
        QVERIFY(window.scrob->get_track(0).mb_track_id.isEmpty());
        table->item(0, 0)->setText("");
        QCOMPARE(table->item(0, 0)->text(), "Élan");
        table->item(0, 1)->setText("");
        QCOMPARE(window.scrob->get_track(0).album, QString());
        table->item(0, 2)->setText("");
        QCOMPARE(window.scrob->get_track(0).title, "Morning Signal");
        table->item(0, 3)->setText("12");
        QCOMPARE(window.scrob->get_track(0).tracknum, 12);
        for (const QString value : {"wrong", "-1", "2147483648"}) {
            table->item(0, 3)->setText(value);
            QCOMPARE(window.scrob->get_track(0).tracknum, 12);
        }
        table->item(0, 3)->setText("0");
        QCOMPARE(window.scrob->get_track(0).tracknum, 0);
        table->item(0, 3)->setText("12");
        table->item(0, 3)->setText("");
        QCOMPARE(window.scrob->get_track(0).tracknum, 0);
        table->item(0, 4)->setText("240");
        QCOMPARE(window.scrob->get_track(0).length, 240);
        for (const QString value : {"", "invalid", "-1", "2147483648"}) {
            table->item(0, 4)->setText(value);
            QCOMPARE(window.scrob->get_track(0).length, 240);
        }
        table->item(0, 4)->setText("0");
        QCOMPARE(window.scrob->get_track(0).length, 0);
        table->item(0, 4)->setText("240");
        table->item(0, 5)->setText("S");
        QCOMPARE(window.scrob->get_track(0).played, QChar('S'));
        table->item(0, 5)->setText("bad");
        QCOMPARE(window.scrob->get_track(0).played, QChar('S'));
        table->item(0, 5)->setText("");
        QCOMPARE(table->item(0, 5)->text(), "S");
        QCOMPARE(window.scrob->get_track(0).played, QChar('S'));
        table->item(0, 5)->setText("L");
        QCOMPARE(window.scrob->get_track(0).played, QChar('L'));
        table->item(0, 6)->setText("2026-11-17 10:00:00");
        QCOMPARE(window.scrob->get_track(0).when, 1794909600);
        table->item(0, 6)->setText("invalid");
        QCOMPARE(window.scrob->get_track(0).when, 1794909600);
        window.scrob->conf->display_utc = false;
        window.scrob->conf->tz_override = true;
        window.scrob->conf->utc_offset = 3600;
        window.loadtable();
        QCOMPARE(table->item(0, 6)->text(), "2026-11-17 11:00:00");
        table->item(0, 6)->setText("2026-11-17 12:00:00");
        QCOMPARE(window.scrob->get_track(0).when, 1794913200);
    }

    void timestampEditsRespectStorageBounds_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("displayUtc");
        QTest::addColumn<int>("offset");
        QTest::addColumn<int>("expectedTimestamp");
        QTest::newRow("epoch") << "1970-01-01 00:00:00" << true << 0 << 0;
        QTest::newRow("maximum") << "2038-01-19 03:14:07" << true << 0 << 2147483647;
        QTest::newRow("before-epoch") << "1969-12-31 23:59:59" << true << 0 << -1;
        QTest::newRow("after-maximum") << "2038-01-19 03:14:08" << true << 0 << -1;
        QTest::newRow("local-epoch") << "1970-01-01 01:00:00" << false << 3600 << 0;
        QTest::newRow("local-before-epoch") << "1970-01-01 00:59:59" << false << 3600 << -1;
        QTest::newRow("local-maximum") << "2038-01-19 02:14:07" << false << -3600 << 2147483647;
        QTest::newRow("local-after-maximum") << "2038-01-19 04:14:08" << false << 3600 << -1;
        QTest::newRow("invalid-calendar") << "2026-02-30 10:00:00" << true << 0 << -1;
        QTest::newRow("empty") << "" << true << 0 << -1;
    }

    void timestampEditsRespectStorageBounds() {
        QFETCH(QString, text);
        QFETCH(bool, displayUtc);
        QFETCH(int, offset);
        QFETCH(int, expectedTimestamp);
        QTemporaryDir directory;
        QVERIFY(fixture(directory));
        QTScrob window;
        window.scrob->conf->display_utc = displayUtc;
        window.scrob->conf->tz_override = true;
        window.scrob->conf->utc_offset = offset;
        window.setupWindow();
        window.parse(SCROBBLE_LOG, directory.path());
        auto* table = window.findChild<QTableWidget*>();
        QVERIFY(table);
        const auto before = window.scrob->get_track(0);
        const QString displayedBefore = table->item(0, 6)->text();
        table->item(0, 6)->setText(text);
        const auto after = window.scrob->get_track(0);
        QCOMPARE(after.when, expectedTimestamp < 0 ? before.when : expectedTimestamp);
        if (expectedTimestamp < 0) QCOMPARE(table->item(0, 6)->text(), displayedBefore);
        QCOMPARE(after.artist, before.artist);
        QCOMPARE(after.title, before.title);
        QCOMPARE(after.length, before.length);
        QCOMPARE(after.mb_track_id, before.mb_track_id);
    }

    void rejectedSubmissionLeavesSourceAvailable() {
        QTemporaryDir directory;
        QVERIFY(fixture(directory));
        QTScrob window;
        window.setupWindow();
        for (auto& site : window.scrob->conf->sites) site.enabled = false;
        window.parse(SCROBBLE_LOG, directory.path());
        window.scrob->recalc_now();
        auto* submit = button(window, "Submit");
        QVERIFY(submit);
        QString diagnostic;
        connect(window.scrob, &Scrobble::submission_finished, &window, [&] {
            QTimer::singleShot(0, &window, [&] {
                auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if (box) {
                    diagnostic = box->text();
                    box->button(QMessageBox::Ok)->click();
                }
            });
        });
        QSignalSpy finished(window.scrob, &Scrobble::submission_finished);
        submit->click();
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first()[0].toBool(), false);
        QTRY_VERIFY_WITH_TIMEOUT(submit->isEnabled(), 1000);
        QVERIFY(diagnostic.contains("No sites were configured and/or enabled"));
        QVERIFY(!window.findChild<Progress*>());
        QCOMPARE(window.scrob->get_num_tracks(), 3);
        QVERIFY(QFile::exists(directory.filePath(".scrobbler.log")));
        QVERIFY(window.scrob->submissions.isEmpty());
    }

    void deleteSelectionAndReimport() {
        QTemporaryDir directory;
        QVERIFY(fixture(directory));
        QTScrob window;
        window.scrob->conf->auto_open = true;
        window.setupWindow();
        window.show();
        auto* table = window.findChild<QTableWidget*>();
        auto* open = button(window, "Open .scrobbler.log");
        QVERIFY(open);
        QList<QAbstractButton*> openButtons{open, button(window, "Open iTunesDB")};
        QList<QAction*> openActions{menuAction(window, "Open"), menuAction(window, "Open iTunesDB")};
#ifdef HAVE_MTP
        openButtons.append(button(window, "Open MTP Device"));
        openActions.append(menuAction(window, "Open MTP Device"));
#endif
        for (auto* control : openButtons) QVERIFY(control && control->isEnabled());
        for (auto* action : openActions) QVERIFY(action && action->isEnabled());
        window.parse(SCROBBLE_LOG, directory.path());
        for (auto* control : openButtons) QVERIFY(!control->isEnabled());
        for (auto* action : openActions) QVERIFY(!action->isEnabled());
        QCOMPARE(window.scrob->conf->mru.first(), directory.path());
        table->selectRow(1);
        answer(QMessageBox::No);
        window.deleteRow();
        QCOMPARE(table->rowCount(), 3);
        table->selectRow(1);
        answer(QMessageBox::Yes);
        QTest::keyClick(table, Qt::Key_Delete);
        QCOMPARE(table->rowCount(), 2);
        QCOMPARE(window.scrob->get_track(1).title, "Quiet Harbour");
        table->selectAll();
        answer(QMessageBox::Yes);
        window.deleteRow();
        QCOMPARE(table->rowCount(), 0);
        QVERIFY(!window.scrob->get_parser_busy());
        for (auto* control : openButtons) QVERIFY(control->isEnabled());
        for (auto* action : openActions) QVERIFY(action->isEnabled());
        QTest::mouseClick(open, Qt::LeftButton);
        QCOMPARE(table->rowCount(), 3);
        QCOMPARE(window.scrob->conf->mru.count(directory.path()), 1);
        table->clearSelection();
        window.deleteRow();
        QCOMPARE(table->rowCount(), 3);
        table->setRangeSelected(QTableWidgetSelectionRange(1, 0, 2, 6), true);
        answer(QMessageBox::Yes);
        window.deleteRow();
        QCOMPARE(table->rowCount(), 1);
        table->setCurrentCell(-1, -1);
        table->clearSelection();
        window.deleteRow();
        QCOMPARE(table->rowCount(), 1);
        // Selection and the current index are independent in Qt. A selection
        // without a current row must not open a destructive confirmation.
        table->selectionModel()->select(table->model()->index(0, 0),
                                        QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QVERIFY(!table->selectedItems().isEmpty());
        QCOMPARE(table->currentRow(), -1);
        bool requestedConfirmation = false;
        QTimer::singleShot(0, &window, [&] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (box) {
                requestedConfirmation = true;
                box->button(QMessageBox::No)->click();
            }
        });
        window.deleteRow();
        QCoreApplication::processEvents();
        QVERIFY(!requestedConfirmation);
        QCOMPARE(table->rowCount(), 1);
        table->selectRow(0);
        answer(QMessageBox::No);
        window.deleteRow();
        QCOMPARE(table->rowCount(), 1);
        table->selectRow(0);
        answer(QMessageBox::Yes);
        window.deleteRow();
        QCOMPARE(table->rowCount(), 0);
        QVERIFY(open->isEnabled());
    }

    void recalculateFromDisplayedTime() {
        QTemporaryDir directory;
        QVERIFY(fixture(directory));
        QTScrob window;
        window.scrob->conf->display_utc = true;
        window.setupWindow();
        window.parse(SCROBBLE_LOG, directory.path());
        auto* edit = window.findChild<QDateTimeEdit*>();
        QVERIFY(edit);
        const QDateTime displayed(QDate(2027, 2, 3), QTime(12, 34, 56));
        edit->setDateTime(displayed);
        window.settings();
        window.settings_close();
        QCOMPARE(edit->dateTime(), displayed);
        auto* recalc = button(window, "Recalc Date/Time");
        QVERIFY(recalc);
        recalc->click();
        const auto last = window.scrob->get_track(2);
        QCOMPARE(last.when + last.length,
                 QDateTime(displayed.date(), displayed.time(), QTimeZone::utc()).toSecsSinceEpoch());
        QCOMPARE(window.scrob->get_track(1).when + window.scrob->get_track(1).length, last.when);
    }

    void settingsSaveCancelAndReopen() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString configPath = directory.filePath("settings.ini");
        QTScrob window;
        window.scrob->conf->set_custom_location(configPath);
        window.scrob->conf->auto_open = true;
        window.scrob->conf->del_apple_playlist = true;
        window.scrob->conf->display_utc = true;
        window.scrob->conf->utc_offset = -19800;
        window.scrob->conf->tz_override = true;
        window.scrob->conf->proxy_host = "saved-proxy.invalid";
        window.scrob->conf->proxy_port = 4321;
        window.scrob->conf->proxy_user = "saved-proxy-user";
        window.scrob->conf->proxy_pass = "fictional-proxy-password";
        auto& lastfm = window.scrob->conf->sites[CONFIG_LASTFM];
        lastfm.enabled = true;
        lastfm.username = "existing-user";
        lastfm.password_hash = "fixture-password-hash";
        window.setupWindow();
        window.settings();
        auto* settings = window.findChild<Settings*>();
        QVERIFY(settings);
        auto sites = siteWidgets(*settings);
        QCOMPARE(sites.size(), CONFIG_NUM_SITES);
        QVERIFY(settings->findChild<QCheckBox*>("chkAutoOpen")->isChecked());
        QVERIFY(settings->findChild<QCheckBox*>("chkDelPlayList")->isChecked());
        QVERIFY(settings->findChild<QCheckBox*>("chkDisplayUTC")->isChecked());
        QVERIFY(settings->findChild<QGroupBox*>("grpManual")->isChecked());
        QCOMPARE(settings->findChild<QComboBox*>("cmb_tz_hour")->currentIndex(), 7);
        QCOMPARE(settings->findChild<QComboBox*>("cmb_tz_min")->currentIndex(), 2);
        QCOMPARE(settings->findChild<QLineEdit*>("edtProxyHost")->text(), "saved-proxy.invalid");
        QCOMPARE(settings->findChild<QLineEdit*>("edtProxyPort")->text(), "4321");
        QCOMPARE(settings->findChild<QLineEdit*>("edtProxyUser")->text(), "saved-proxy-user");
        QCOMPARE(settings->findChild<QLineEdit*>("edtProxyPass")->text(), "fictional-proxy-password");
        for (int index = 0; index < sites.size(); ++index) {
            auto* site = sites[index];
            auto* form = qobject_cast<QFormLayout*>(static_cast<QWidget*>(site)->layout());
            QVERIFY(form);
            const QList<QPair<QWidget*, QString>> fields = {{site->enabled, "Enabled"},
                                                            {site->name, "Site Name"},
                                                            {site->host, "Host"},
                                                            {site->username, "Username"},
                                                            {site->password, "Password"}};
            for (const auto& field : fields) {
                auto* label = qobject_cast<QLabel*>(form->labelForField(field.first));
                QVERIFY(label);
                QCOMPARE(label->text(), field.second);
            }
            QCOMPARE(site->name->text(), window.scrob->conf->sites[index].conf_name);
            QCOMPARE(site->host->text(), window.scrob->conf->sites[index].handshake_host);
            QCOMPARE(site->name->isEnabled(), index == CONFIG_CUSTOM);
        }
        QVERIFY(!sites[CONFIG_LASTFM]->host->isEnabled());
        QVERIFY(sites[CONFIG_CUSTOM]->host->isEnabled());
        QVERIFY(sites[CONFIG_LASTFM]->enabled->isChecked());
        QVERIFY(!sites[CONFIG_LIBREFM]->enabled->isChecked());
        QVERIFY(!sites[CONFIG_CUSTOM]->enabled->isChecked());
        QCOMPARE(sites[CONFIG_LASTFM]->username->text(), "existing-user");
        QCOMPARE(sites[CONFIG_LASTFM]->password->text(), "********");
        QCOMPARE(sites[CONFIG_LIBREFM]->password->text(), QString());
        QCOMPARE(sites[CONFIG_CUSTOM]->password->text(), QString());
        sites[CONFIG_LASTFM]->enabled->setChecked(false);
        sites[CONFIG_LIBREFM]->enabled->setChecked(true);
        sites[CONFIG_LIBREFM]->username->setText("new-librefm-user");
        sites[CONFIG_CUSTOM]->enabled->setChecked(true);
        sites[CONFIG_CUSTOM]->host->setText("127.0.0.1:12345");
        sites[CONFIG_CUSTOM]->name->setText("Local fixture");
        sites[CONFIG_CUSTOM]->username->setText("fictional-user");
        sites[CONFIG_CUSTOM]->password->setText("fictional-secret");
        sites[CONFIG_CUSTOM]->password->setModified(true);
        settings->findChild<QCheckBox*>("chkAutoOpen")->setChecked(false);
        settings->findChild<QCheckBox*>("chkDelPlayList")->setChecked(false);
        settings->findChild<QCheckBox*>("chkDisplayUTC")->setChecked(true);
        settings->findChild<QLineEdit*>("edtProxyHost")->setText("127.0.0.1");
        settings->findChild<QLineEdit*>("edtProxyPort")->setText("3128");
        settings->findChild<QGroupBox*>("grpManual")->setChecked(true);
        settings->findChild<QComboBox*>("cmb_tz_hour")->setCurrentIndex(7);
        settings->findChild<QComboBox*>("cmb_tz_min")->setCurrentIndex(2);
        settings->findChild<QPushButton*>("btnOK")->click();
        QVERIFY(!window.findChild<Settings*>());
        const auto& site = window.scrob->conf->sites[CONFIG_CUSTOM];
        QCOMPARE(site.handshake_host, "127.0.0.1:12345");
        QCOMPARE(site.conf_name, "Local fixture");
        QVERIFY(site.enabled);
        QVERIFY(!lastfm.enabled);
        QCOMPARE(lastfm.password_hash, "fixture-password-hash");
        QVERIFY(window.scrob->conf->sites[CONFIG_LIBREFM].enabled);
        QCOMPARE(window.scrob->conf->sites[CONFIG_LIBREFM].username, "new-librefm-user");
        QCOMPARE(site.password_hash,
                 QString::fromLatin1(QCryptographicHash::hash("fictional-secret", QCryptographicHash::Md5).toHex()));
        QCOMPARE(window.scrob->conf->utc_offset, -19800);
        QCOMPARE(window.scrob->conf->proxy_port, 3128);
        QVERIFY(!window.scrob->conf->auto_open);
        QVERIFY(!window.scrob->conf->del_apple_playlist);
        QVERIFY(window.scrob->conf->display_utc);
        QVERIFY(window.scrob->conf->tz_override);
        window.settings();
        settings = window.findChild<Settings*>();
        QVERIFY(settings);
        sites = siteWidgets(*settings);
        QVERIFY(!settings->findChild<QCheckBox*>("chkAutoOpen")->isChecked());
        QVERIFY(!settings->findChild<QCheckBox*>("chkDelPlayList")->isChecked());
        QVERIFY(!sites[CONFIG_LASTFM]->enabled->isChecked());
        QVERIFY(sites[CONFIG_LIBREFM]->enabled->isChecked());
        QVERIFY(sites[CONFIG_CUSTOM]->enabled->isChecked());
        QCOMPARE(sites[CONFIG_CUSTOM]->password->text(), "********");
        QCOMPARE(sites[CONFIG_CUSTOM]->password->echoMode(), QLineEdit::Password);
        sites[CONFIG_CUSTOM]->username->setText("discarded-change");
        window.settings();
        QCOMPARE(window.findChild<Settings*>(), settings);
        QCOMPARE(window.findChildren<Settings*>().size(), 1);
        QCOMPARE(siteWidgets(*settings)[CONFIG_CUSTOM]->username->text(), "discarded-change");
        settings->findChild<QPushButton*>("btnCancel")->click();
        QCOMPARE(window.scrob->conf->sites[CONFIG_CUSTOM].username, "fictional-user");
        window.settings();
        settings = window.findChild<Settings*>();
        QVERIFY(settings);
        sites = siteWidgets(*settings);
        QCOMPARE(sites[CONFIG_CUSTOM]->username->text(), "fictional-user");
        sites[CONFIG_LASTFM]->enabled->setChecked(true);
        sites[CONFIG_LIBREFM]->enabled->setChecked(false);
        sites[CONFIG_CUSTOM]->enabled->setChecked(false);
        settings->findChild<QCheckBox*>("chkAutoOpen")->setChecked(true);
        settings->findChild<QCheckBox*>("chkDelPlayList")->setChecked(true);
        settings->findChild<QComboBox*>("cmb_tz_hour")->setCurrentText("0");
        settings->findChild<QComboBox*>("cmb_tz_min")->setCurrentText("15");
        settings->findChild<QPushButton*>("btnOK")->click();
        QCOMPARE(window.scrob->conf->sites[CONFIG_CUSTOM].password_hash,
                 QString::fromLatin1(QCryptographicHash::hash("fictional-secret", QCryptographicHash::Md5).toHex()));
        QCOMPARE(window.scrob->conf->utc_offset, 900);
        QVERIFY(window.scrob->conf->tz_override);
        window.settings();
        settings = window.findChild<Settings*>();
        QVERIFY(settings);
        QCOMPARE(settings->findChild<QComboBox*>("cmb_tz_hour")->currentText(), "0");
        QCOMPARE(settings->findChild<QComboBox*>("cmb_tz_min")->currentText(), "15");
        settings->findChild<QGroupBox*>("grpManual")->setChecked(false);
        settings->findChild<QPushButton*>("btnOK")->click();

        Conf restored;
        restored.set_custom_location(configPath);
        QVERIFY(restored.auto_open);
        QVERIFY(restored.del_apple_playlist);
        QVERIFY(!restored.tz_override);
        QCOMPARE(restored.utc_offset, 900);
        QVERIFY(restored.sites[CONFIG_LASTFM].enabled);
        QVERIFY(!restored.sites[CONFIG_LIBREFM].enabled);
        QVERIFY(!restored.sites[CONFIG_CUSTOM].enabled);
        QCOMPARE(restored.sites[CONFIG_LASTFM].password_hash, "fixture-password-hash");
        window.settings_close();  // Repeated close notifications are harmless.
    }

    void auxiliaryWindowsAndMissingProgress() {
        QTScrob window;
        window.setupWindow();
        auto* helpAction = menuAction(window, "Help");
        auto* aboutAction = menuAction(window, "About");
        auto* consoleAction = menuAction(window, "Console");
        QVERIFY(helpAction && aboutAction && consoleAction);
        helpAction->trigger();
        auto* help = window.findChild<Help*>();
        QVERIFY(help && help->isVisible());
        QCOMPARE(help->windowTitle(), "QTScrobbler Help");
        auto* helpText = help->findChild<QTextEdit*>();
        QVERIFY(helpText && helpText->isReadOnly());
        QVERIFY(!helpText->toPlainText().isEmpty());
        QVERIFY(help->layout()->indexOf(helpText) >= 0);
        help->close();
        helpAction->trigger();
        QCOMPARE(window.findChildren<Help*>().size(), 1);
        QVERIFY(help->isVisible());
        aboutAction->trigger();
        auto* about = window.findChild<About*>();
        QVERIFY(about && about->isVisible());
        QCOMPARE(about->findChild<QLabel*>("lblVersion")->text(), QString("QTScrobbler v%1").arg(CLIENT_VERSION));
        about->close();
        aboutAction->trigger();
        QCOMPARE(window.findChildren<About*>().size(), 1);
        QVERIFY(about->isVisible());
        consoleAction->trigger();
        auto* console = window.findChild<Console*>();
        QVERIFY(console && console->isVisible());
        auto* level = console->findChild<QComboBox*>();
        auto* output = console->findChild<QTextEdit*>();
        QVERIFY(level && output);
        QCOMPARE(console->windowTitle(), "Console");
        QVERIFY(output->isReadOnly());
        QVERIFY(!level->isEditable());
        QCOMPARE(level->count(), int(LOG_NUM_LEVELS));
        QCOMPARE(level->currentIndex(), int(LOG_DEFAULT));
        level->setCurrentIndex(LOG_TRACE);
        QVERIFY(output->toPlainText().contains("Detected Timezone"));
        window.scrob->parser_clear();
        QTRY_VERIFY_WITH_TIMEOUT(output->toPlainText().contains("Parser: asked to clear"), 1000);
        level->setCurrentIndex(LOG_ERROR);
        QVERIFY(!output->toPlainText().contains("Detected Timezone"));
        window.scrob->clear_log();
        QTRY_VERIFY_WITH_TIMEOUT(output->toPlainText().isEmpty(), 1000);
        console->close();
        consoleAction->trigger();
        QCOMPARE(window.findChildren<Console*>().size(), 1);
        QVERIFY(console->isVisible());
        QTemporaryDir directory;
        QVERIFY(fixture(directory));
        window.parse(SCROBBLE_LOG, directory.path());
        auto* table = window.findChild<QTableWidget*>();
        QVERIFY(table && table->item(0, 4));
        QCOMPARE(table->item(0, 4)->text(), "210");
        window.missing_times_start(4);
        auto* missing = window.findChild<MissingTimeProgress*>();
        QVERIFY(missing && missing->isVisible());
        auto* progress = missing->findChild<QProgressBar*>();
        QVERIFY(progress);
        QCOMPARE(missing->windowTitle(), "Please wait");
        QCOMPARE(missing->windowModality(), Qt::WindowModal);
        QCOMPARE(missing->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
        QCOMPARE(missing->sizePolicy().verticalPolicy(), QSizePolicy::Fixed);
        auto* label = missing->findChild<QLabel*>();
        QVERIFY(label);
        QCOMPARE(label->text(), "Fetching missing track lengths");
        QVERIFY(static_cast<QWidget*>(missing)->layout()->indexOf(label) >= 0);
        QVERIFY(static_cast<QWidget*>(missing)->layout()->indexOf(progress) >= 0);
        QCOMPARE(progress->maximum(), 4);
        window.missing_times_progress(2);
        QCOMPARE(progress->value(), 2);
        auto first = window.scrob->get_track(0);
        first.length = 333;
        window.scrob->update_track(first, 0);
        QCOMPARE(table->item(0, 4)->text(), "210");
        window.missing_times_finished();
        QVERIFY(!window.findChild<MissingTimeProgress*>());
        QCOMPARE(table->item(0, 4)->text(), "333");
        window.missing_times_finished();  // A late completion cannot reuse a deleted dialog.
        window.missing_times_start(2);
        window.missing_times_start(2);
        QCOMPARE(window.findChildren<MissingTimeProgress*>().size(), 1);
        window.missing_times_progress(1);
        window.missing_times_finished();
        QVERIFY(!window.findChild<MissingTimeProgress*>());
    }

    void submissionProgress_data() {
        QTest::addColumn<QString>("outcome");
        QTest::newRow("success") << "success";
        QTest::newRow("failure") << "failure";
        QTest::newRow("cancel") << "cancel";
    }

    void submissionProgress() {
        QFETCH(QString, outcome);
        HttpFixture server;
        QVERIFY(server.isListening());
        server.respond = [&](const HttpFixture::Request& request) -> HttpFixture::Response {
            if (outcome == "cancel") return {{}, 200, true};
            return {request.method == "GET" ? server.handshake()
                    : outcome == "failure"  ? QByteArray("FAILED fixture\n")
                                            : QByteArray("OK\n")};
        };
        QTemporaryDir directory;
        QVERIFY(fixture(directory));
        QTScrob window;
        window.setupWindow();
        for (auto& site : window.scrob->conf->sites) site.enabled = false;
        auto& site = window.scrob->conf->sites[CONFIG_CUSTOM];
        site.enabled = true;
        site.conf_name = "Local fixture";
        site.username = "fictional-user";
        site.password_hash = "fictional-hash";
        site.handshake_host = server.host();
        window.scrob->conf->proxy_host.clear();
        window.parse(SCROBBLE_LOG, directory.path());
        auto* submit = button(window, "Submit");
        QVERIFY(submit);
        QSignalSpy finished(window.scrob, &Scrobble::submission_finished);
        QTimer dismissError;
        if (outcome != "success") {
            connect(&dismissError, &QTimer::timeout, &window, [&] {
                auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if (box) {
                    box->button(QMessageBox::Ok)->click();
                    dismissError.stop();
                }
            });
            dismissError.start(1);
        }
        submit->click();
        QVERIFY(statusBarShows(window, "Submitting data to server..."));
        auto* progress = window.findChild<Progress*>();
        QVERIFY(progress);
        QVERIFY(progress->isVisible());
        QCOMPARE(progress->windowTitle(), "Submission Progress");
        auto* cancelControl = button(*progress, "Cancel");
        QVERIFY(cancelControl);
        const auto rows = progress->findChildren<Progress_UI*>();
        QCOMPARE(rows.size(), 1);
        auto* row = rows.first();
        QCOMPARE(row->name->text(), "Custom");
        QCOMPARE(row->progress->minimumWidth(), 200);
        QCOMPARE(row->progress->maximumWidth(), 200);
        QCOMPARE(row->layout->rowCount(), 1);
        QCOMPARE(row->layout->itemAt(0, QFormLayout::LabelRole)->widget(), row->name);
        QCOMPARE(row->layout->itemAt(0, QFormLayout::FieldRole)->widget(), row->progress);
        auto* layout = static_cast<QWidget*>(progress)->layout();
        QCOMPARE(layout->count(), 3);
        QCOMPARE(layout->itemAt(0)->widget(), row);
        QCOMPARE(layout->itemAt(0)->alignment(), Qt::AlignRight);
        QVERIFY(layout->itemAt(1)->spacerItem());
        QCOMPARE(layout->itemAt(1)->sizeHint().height(), 10);
        QCOMPARE(layout->itemAt(2)->widget(), cancelControl);

        progress->update_progress(CONFIG_CUSTOM, 2, 7);
        QCOMPARE(row->progress->maximum(), 7);
        QCOMPARE(row->progress->value(), 2);
        progress->update_progress(-1, 1, 3);
        QCOMPARE(row->progress->maximum(), 7);
        QCOMPARE(row->progress->value(), 2);
        QSignalSpy sentProgress(row->progress, &QProgressBar::valueChanged);
        QVERIFY(!submit->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(!server.requests.isEmpty(), 2000);
        if (outcome == "cancel") {
            auto* cancel = button(*progress, "Cancel");
            QVERIFY(cancel);
            cancel->click();
        }
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(sentProgress.size(), outcome == "cancel" ? 0 : 1);
        if (outcome != "cancel") QCOMPARE(sentProgress.first().first().toInt(), 3);
        QCOMPARE(finished.first().at(0).toBool(), outcome == "success");
        QTRY_VERIFY_WITH_TIMEOUT(!window.findChild<Progress*>(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(
            statusBarShows(window, outcome == "success" ? "Data submitted succesfully" : "Submission failed"), 2000);
        QVERIFY(submit->isEnabled());
        QVERIFY(button(window, "Open .scrobbler.log")->isEnabled());
        QCOMPARE(QFile::exists(directory.filePath(".scrobbler.log")), outcome != "success");
        QCOMPARE(window.scrob->get_num_tracks(), outcome == "success" ? 0 : 3);
        QCOMPARE(window.findChild<QTableWidget*>()->rowCount(), outcome == "success" ? 0 : 3);
        QVERIFY(!window.scrob->get_parser_busy());
        QVERIFY(window.scrob->submissions.isEmpty());
    }

    void importEditAndClose() {
        QCOMPARE(QString::fromLatin1(qVersion()), QString::fromLatin1(QT_VERSION_STR));
        QVERIFY(QSqlDatabase::isDriverAvailable("QSQLITE"));
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QFile::copy(EXAMPLE_LOG, directory.filePath(".scrobbler.log")));
        QTScrob window;
        window.scrob->conf->display_utc = true;
        window.setupWindow();
        window.show();
        auto* table = window.findChild<QTableWidget*>();
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
        QVERIFY(statusBarShows(window, "Loaded 3 entries"));
        const QList<QStringList> expected{
            {"Example Artist", "Example Album", "Morning Signal", "1", "210", "L", "2026-11-17 08:00:00"},
            {"Example Artist", "Example Album", "Evening Echo", "2", "185", "L", "2026-11-17 08:05:00"},
            {"Fictional Ensemble", "Demo Sessions", "Quiet Harbour", "3", "240", "L", "2026-11-17 08:10:00"},
        };
        for (int row = 0; row < expected.size(); ++row) {
            QStringList displayed;
            for (int column = 0; column < table->columnCount(); ++column) {
                auto* item = table->item(row, column);
                QVERIFY(item);
                displayed.append(item->text());
            }
            QCOMPARE(displayed, expected[row]);
        }
        if (qEnvironmentVariableIsSet("QTSCROBBLER_SCREENSHOT_PATH")) {
            window.resize(1120, 320);
            QApplication::processEvents();
            QVERIFY(window.grab().save(qEnvironmentVariable("QTSCROBBLER_SCREENSHOT_PATH")));
        }
        table->item(0, 0)->setText("Edited Artist");
        QCOMPARE(window.scrob->get_track(0).artist, "Edited Artist");
        table->item(0, 2)->setText("Edited Title");
        QCOMPARE(window.scrob->get_track(0).title, "Edited Title");
        QApplication::setOverrideCursor(Qt::CrossCursor);
        window.loadtable();
        const bool restoredCursor =
            QApplication::overrideCursor() && QApplication::overrideCursor()->shape() == Qt::CrossCursor;
        QApplication::restoreOverrideCursor();
        QVERIFY(restoredCursor);
        table->item(0, 1)->setText("Edited Album");
        QCOMPARE(window.scrob->get_track(0).album, "Edited Album");

        window.settings();
        auto* settings = window.findChild<QDialog*>();
        QVERIFY(settings);
        QVERIFY(settings->isVisible());
        window.settings_close();
        QVERIFY(window.findChild<QDialog*>() == nullptr);
        window.settings();
        QVERIFY(window.findChild<QDialog*>() != nullptr);
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

int main(int argc, char** argv) {
    QTemporaryDir home;
    if (!home.isValid()) return 1;
    test_environment::isolate(home.path());
    qputenv("XDG_RUNTIME_DIR", home.path().toUtf8());
    QApplication application(argc, argv);
    if (argc == 2 && QByteArray(argv[1]) == "--settings-timezone-probe") {
        QTScrob window;
        window.setupWindow();
        window.settings();
        auto* settings = window.findChild<Settings*>();
        auto* label = settings->findChild<QLabel*>("lblTZInfo");
        const bool daylight = QDateTime::currentDateTime().isDaylightTime();
        return label && label->text().contains(daylight ? "Yes" : "No") ? 0 : 1;
    }
    GuiTest tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_gui.moc"
