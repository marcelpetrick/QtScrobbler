// SPDX-License-Identifier: GPL-2.0-only
#include "console.h"
#include "progress.h"
#include "qtscrob.h"
#include "translation_labels.h"

#include <QComboBox>
#include <QDomDocument>
#include <QPushButton>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSqlDatabase>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QtTest>

namespace {
QStringList unexpectedMessages;
QMutex messagesMutex;
QtMessageHandler previousMessageHandler = nullptr;

void captureMessages(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const bool allowed = QGuiApplication::platformName() == "offscreen"
        && message == "This plugin does not support propagateSizeHints()";
    if ((type == QtWarningMsg || type == QtCriticalMsg) && !allowed) {
        QMutexLocker locker(&messagesMutex);
        unexpectedMessages.append(message);
    }
    if (previousMessageHandler)
        previousMessageHandler(type, context, message);
}

QStringList placeholders(const QString &text)
{
    QStringList result;
    auto matches = QRegularExpression("%L?\\d+|%n").globalMatch(text);
    while (matches.hasNext())
        result.append(matches.next().captured());
    result.sort();
    return result;
}

void localeRows()
{
    QTest::addColumn<QString>("language");
    QTest::newRow("German") << "de";
    QTest::newRow("Polish") << "pl";
}
}

class TranslationsTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        unexpectedMessages.clear();
        previousMessageHandler = qInstallMessageHandler(captureMessages);
    }

    void cleanup()
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        qInstallMessageHandler(previousMessageHandler);
        QVERIFY2(unexpectedMessages.isEmpty(), qPrintable(unexpectedMessages.join('\n')));
        QVERIFY(QSqlDatabase::connectionNames().isEmpty());
    }

    void compiledCatalogMatchesEveryActiveEntry_data() { localeRows(); }

    void compiledCatalogMatchesEveryActiveEntry()
    {
        QFETCH(QString, language);
        const QLocale previous = QLocale();
        QLocale::setDefault(QLocale(language));
        auto restore = qScopeGuard([&] { QLocale::setDefault(previous); });
        QTranslator translator;
        QVERIFY2(translator.load(":/language"), qPrintable(language));
        QVERIFY(!translator.isEmpty());
        QFile source(QString(TRANSLATION_SOURCE_DIR) + '/' + language + ".ts");
        QVERIFY(source.open(QIODevice::ReadOnly));
        QDomDocument document;
        QVERIFY(document.setContent(&source));
        int active = 0;
        int historical = 0;
        int sameText = 0;
        const auto contexts = document.documentElement().elementsByTagName("context");
        for (int i = 0; i < contexts.size(); ++i) {
            const auto context = contexts.at(i).toElement();
            const QByteArray contextName = context.firstChildElement("name").text().toUtf8();
            const auto messages = context.elementsByTagName("message");
            for (int j = 0; j < messages.size(); ++j) {
                const auto message = messages.at(j).toElement();
                const auto translation = message.firstChildElement("translation");
                const QString type = translation.attribute("type");
                if (type == "vanished" || type == "obsolete") {
                    ++historical;
                    continue;
                }
                const QString original = message.firstChildElement("source").text();
                const QString expected = translation.text();
                QVERIFY2(type != "unfinished" && !expected.isEmpty(), qPrintable(original));
                const QString actual = translator.translate(contextName.constData(), original.toUtf8().constData());
                // An empty lookup is fallback, even when source and target have identical spelling.
                QVERIFY2(!actual.isEmpty(), qPrintable(QString::fromUtf8(contextName) + ": " + original));
                QCOMPARE(actual, expected);
                QCOMPARE(placeholders(actual), placeholders(original));
                sameText += actual == original;
                ++active;
            }
        }
        QCOMPARE(active, 223);
        QCOMPARE(historical, 46);
        QCOMPARE(sameText, 41);
        QVERIFY(translator.translate("QTScrob", "This key is deliberately absent").isEmpty());
    }

    void translatedWidgetsAndStableConfiguration_data() { localeRows(); }

    void translatedWidgetsAndStableConfiguration()
    {
        QFETCH(QString, language);
        const bool german = language == "de";
        const QLocale previous = QLocale();
        QLocale::setDefault(QLocale(language));
        QTranslator translator;
        QVERIFY(translator.load(":/language"));
        QVERIFY(qApp->installTranslator(&translator));
        auto restore = qScopeGuard([&] {
            qApp->removeTranslator(&translator);
            QLocale::setDefault(previous);
        });
        const QString custom = german ? "Benutzerdefiniert" : "Niestandardowa";
        QCOMPARE(site_display_name(CONFIG_CUSTOM), custom);
        QCOMPARE(site_display_name(CONFIG_LASTFM), "Last.fm");
        QCOMPARE(site_display_name(CONFIG_LIBREFM), "Libre.fm");
        QVERIFY(site_display_name(-1).isEmpty());
        QVERIFY(site_display_name(CONFIG_NUM_SITES).isEmpty());
        const QStringList levels{german ? "FEHLER" : "BŁĄD", "INFO ", "DEBUG", "TRACE"};
        for (int level = 0; level < LOG_NUM_LEVELS; ++level)
            QCOMPARE(log_level_display_name(level), levels.at(level));
        QVERIFY(log_level_display_name(-1).isEmpty());
        QCOMPARE(SITE_NAME[CONFIG_CUSTOM], "Custom");
        QCOMPARE(LOG_LEVEL_NAMES[LOG_ERROR], "ERROR");

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString config = directory.filePath("translated.ini");
        QTScrob window;
        window.scrob->conf->set_custom_location(config);
        window.scrob->conf->sites[CONFIG_CUSTOM].username = "fictional-local-user";
        QVERIFY(window.scrob->conf->save_config());
        window.setupWindow();
        auto *table = window.findChild<QTableWidget *>();
        QVERIFY(table);
        QCOMPARE(table->horizontalHeaderItem(0)->text(), german ? "Interpret" : "Artysta");
        bool translatedSubmit = false;
        for (auto *button : window.findChildren<QPushButton *>())
            translatedSubmit |= button->text() == (german ? "Übermitteln" : "Wyślij");
        QVERIFY(translatedSubmit);
        if (QGuiApplication::platformName() == "offscreen")
            QTest::ignoreMessage(QtWarningMsg, "This plugin does not support propagateSizeHints()");
        window.settings();
        auto *settings = window.findChild<QDialog *>("settingsWin");
        QVERIFY(settings);
        QCOMPARE(settings->windowTitle(), german ? "Einstellungen" : "Ustawienia");
        auto *tabs = settings->findChild<QTabWidget *>("tab_widget_sites");
        QVERIFY(tabs);
        QCOMPARE(tabs->tabText(CONFIG_CUSTOM), custom);
        window.settings_close();

        Console console(&window);
        auto *level = console.findChild<QComboBox *>();
        QVERIFY(level);
        for (int i = 0; i < LOG_NUM_LEVELS; ++i)
            QCOMPARE(level->itemText(i), QString("%1 - %2").arg(i).arg(levels.at(i)));
        window.scrob->parser_clear();
        auto *output = console.findChild<QTextEdit *>();
        QVERIFY(output);
        QTRY_VERIFY_WITH_TIMEOUT(output->toPlainText().contains(levels.first() + " : "), 1000);

        window.scrob->submissions.insert(CONFIG_CUSTOM, new Submit(CONFIG_CUSTOM));
        Progress progress(&window);
        auto *site = progress.findChild<Progress_UI *>();
        QVERIFY(site);
        QCOMPARE(site->name->text(), custom);
        QSettings saved(config, QSettings::IniFormat);
        QCOMPARE(saved.value("Custom/username").toString(), "fictional-local-user");
        QVERIFY(!saved.contains(custom + "/username"));
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
    qputenv("XDG_RUNTIME_DIR", home.path().toUtf8());
    QApplication application(argc, argv);
    TranslationsTest tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_translations.moc"
