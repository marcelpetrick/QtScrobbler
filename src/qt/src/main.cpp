/***************************************************************************
 *   Copyright (C) 2006-2007 by Tomasz Moń                                 *
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

#include <QApplication>
#include "qtscrob.h"

#ifdef _MSC_VER
#include "xgetopt.h"
#else
#include <getopt.h>
#endif

void printusage()
{
    QTextStream out(stdout);
    out << "QTScrob (v" << CLIENT_VERSION << ")" << Qt::endl;
    out << Qt::endl;
    out << "Options:" << Qt::endl;
    out << "  -c, --config=PATH    Path to the configuration file" << Qt::endl;
    out << "  -h, --help           This message" << Qt::endl;
    out << "  -v, --verbose=LEVEL  Verbosity level" << Qt::endl;
    out << Qt::endl;
}

int main(int argc, char **argv)
{
    int do_help = 0;
    LOG_LEVEL verbosity = LOG_DEFAULT;
    QString config_path = "";

    QApplication app(argc, argv);
#ifndef Q_OS_MACX
    /* Don't override the higher quality OS X .icns file */
    app.setWindowIcon(QIcon(":/resources/icons/128x128/qtscrob.png"));
#endif
    app.setApplicationName("QTScrobbler");

    QTranslator translator;
    if (translator.load(":/language"))
        app.installTranslator(&translator);

    QTextStream error(stderr);
    opterr = 0;

    while (1)
    {
        const struct option long_options[] =
        {
            {"help",    no_argument,       nullptr,    'h'},
            {"config",  required_argument, 0,        'c'},
            {"verbose", required_argument, 0,        'v'},
            {0, 0, 0, 0}
        };

        int option_index = 0;

        int c = getopt_long(argc, argv, "c:hv:",
                        long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {
            case 'c':
                config_path = QString::fromLocal8Bit(optarg);
                if (config_path.isEmpty()) {
                    error << "Configuration path must not be empty" << Qt::endl;
                    return 2;
                }
                break;
            case 'h':
                do_help = 1;
                break;
            case 'v':
            {
                bool valid = false;
                const int level = QString::fromLocal8Bit(optarg).toInt(&valid);
                if (!valid || level < LOG_ERROR || level >= LOG_NUM_LEVELS) {
                    error << "Verbosity must be an integer between 0 and 3" << Qt::endl;
                    return 2;
                }
                verbosity = static_cast<LOG_LEVEL>(level);
                break;
            }
            default:
                error << "Invalid option or missing argument: " << QString::fromLocal8Bit(argv[optind - 1])
                      << Qt::endl;
                return 2;
        }
    }

    if (optind < argc)
    {
        error << "Unknown option argument:";
        while (optind < argc)
            error << " " << QString::fromLocal8Bit(argv[optind++]);
        error << Qt::endl;
        return 2;
    }

    if (do_help)
    {
        printusage();
        return 0;
    }

    QTScrob mainWin;
    if (!config_path.isEmpty())
        mainWin.scrob->conf->set_custom_location(config_path);

    mainWin.scrob->set_log_level(verbosity);

    mainWin.setupWindow();
    mainWin.show();

    return app.exec();
}
