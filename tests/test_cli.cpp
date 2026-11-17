#include "common.h"

#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

class CliTest : public QObject
{
    Q_OBJECT

private slots:
    void help()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("HOME", directory.path());
        environment.insert("XDG_CONFIG_HOME", directory.path());
        environment.insert("XDG_CACHE_HOME", directory.path());
        environment.insert("LC_ALL", "C.UTF-8");
        process.setProcessEnvironment(environment);
        process.start(CLI_BINARY, {"--help"});
        QVERIFY(process.waitForStarted(2000));
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
        const QByteArray output = process.readAllStandardOutput();
        QVERIFY(output.startsWith(QByteArray("scrobbler (v") + CLIENT_VERSION + "): a Last.fm uploader\n"));
        QVERIFY(output.contains("-c, --config"));
        QVERIFY(output.contains("-f, --file"));
        QVERIFY(output.contains("-d, --database"));
        QCOMPARE(process.readAllStandardError(), QByteArray());
    }
};

QTEST_GUILESS_MAIN(CliTest)
#include "test_cli.moc"
