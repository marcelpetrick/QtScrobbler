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
    out << "QTScrob (v" << CLIENT_VERSION << ")" << endl;
    out << endl;
    out << "Options:" << endl;
    out << "  -c, --config=PATH    Path to the configuration file" << endl;
    out << "  -h, --help           This message" << endl;
    out << "  -v, --verbose=LEVEL  Verbosity level" << endl;
    out << endl;
}

int main(int argc, char **argv)
{
    int c;
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
    if (QFile::exists(":/language.qm")) {
        translator.load(":/language");
        app.installTranslator(&translator);
    }

    QTextStream out(stdout);

    while (1)
    {
        static struct option long_options[] =
        {
            {"help",    no_argument,       &do_help,   1},
            {"config",  required_argument, 0,        'c'},
            {"verbose", required_argument, 0,        'v'},
            {0, 0, 0, 0}
        };

        int option_index = 0;

        c = getopt_long(argc, argv, "c:hv:",
                        long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {
            case 0: //long option
                break;
            case 'c':
                config_path = optarg;
                break;
            case 'h':
                do_help = 1;
                break;
            case 'v':
                verbosity = (LOG_LEVEL)QString(optarg).toInt();
                break;
            case '?':
                break;
            default:
                abort ();
        }
    }

    if (optind < argc)
    {
        out << "Unknown option argument:";
        while (optind < argc)
            out << " " << argv[optind++];
        out << endl;
        exit(1);
    }

    if (do_help)
    {
        printusage();
        exit(0);
    }

    QTScrob mainWin;
    if (!config_path.isEmpty())
        mainWin.scrob->conf->set_custom_location(config_path);

    mainWin.scrob->set_log_level(verbosity);

    mainWin.setupWindow();
    mainWin.show();

    return app.exec();
}
