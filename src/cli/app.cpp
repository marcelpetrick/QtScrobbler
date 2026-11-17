/***************************************************************************
 *   Copyright (C) 2006-2010 by Robert Keevil                              *
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

#include "app.h"
#include <cmath>

#ifdef _MSC_VER
#include "xgetopt.h"
#else
#include <getopt.h>
#endif

app::app(int& argc, char** argv) : QCoreApplication(argc, argv)
{
    scrob = new Scrobble();
    method = SCROBBLE_NONE;
    connect(scrob, &Scrobble::parsing_opened, this, &app::parsed);
    connect(scrob, &Scrobble::submission_finished, this, &app::scrobbled);
    connect(scrob, &Scrobble::missing_times_start, this, [this](int) { metadata_pending = true; });
    connect(scrob, &Scrobble::missing_times_finished, this, [this] {
        metadata_pending = false;
        parsed(true);
    });
}

app::~app()
{
    delete scrob;
}

bool app::parse_cmd(int argc, char** argv)
{
    QTextStream error(stderr);
    do_time = false;
    do_tzoverride = false;
    do_db = do_file = do_now = do_help = 0;
#ifdef HAVE_MTP
    do_mtp = 0;
#endif
    config_path.clear();
    path.clear();
    new_time = -1;
    tz = 0;
    verbosity = LOG_DEFAULT;
    optind = 1;
    opterr = 0;
    const struct option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"database", no_argument, nullptr, 'd'},
        {"file", no_argument, nullptr, 'f'},
#ifdef HAVE_MTP
        {"mtp", no_argument, nullptr, 'm'},
#endif
        {"now", no_argument, nullptr, 'n'},
        {"config", required_argument, nullptr, 'c'},
        {"timezone", required_argument, nullptr, 't'},
        {"location", required_argument, nullptr, 'l'},
        {"verbose", required_argument, nullptr, 'v'},
        {"recalc", required_argument, nullptr, 'r'},
        {nullptr, 0, nullptr, 0}
    };
#ifdef HAVE_MTP
    const char *short_options = "c:dfhl:mnr:t:v:";
#else
    const char *short_options = "c:dfhl:nr:t:v:";
#endif
    int option;
    while ((option = getopt_long(argc, argv, short_options, long_options, nullptr)) != -1)
    {
        bool valid = false;
        switch (option)
        {
            case 'c':
                config_path = QString::fromLocal8Bit(optarg);
                if (config_path.isEmpty())
                {
                    error << tr("Configuration path must not be empty") << Qt::endl;
                    return false;
                }
                break;
            case 'd': do_db = 1; break;
            case 'f': do_file = 1; break;
            case 'h': do_help = 1; break;
            case 'l': path = QString::fromLocal8Bit(optarg); break;
#ifdef HAVE_MTP
            case 'm': do_mtp = 1; break;
#endif
            case 'n': do_now = 1; break;
            case 'r':
                new_time = QString::fromLocal8Bit(optarg).toInt(&valid, 10);
                if (!valid || new_time < 0)
                {
                    error << tr("Recalculation time must be an integer between 0 and 2147483647") << Qt::endl;
                    return false;
                }
                do_time = true;
                break;
            case 't':
                tz = QString::fromLocal8Bit(optarg).toDouble(&valid);
                if (!valid || !std::isfinite(tz) || tz < -24 || tz > 24)
                {
                    error << tr("Timezone must be a finite number between -24 and 24 hours") << Qt::endl;
                    return false;
                }
                do_tzoverride = true;
                break;
            case 'v':
            {
                const int level = QString::fromLocal8Bit(optarg).toInt(&valid, 10);
                if (!valid || level < LOG_ERROR || level >= LOG_NUM_LEVELS)
                {
                    error << tr("Verbosity must be an integer between 0 and 3") << Qt::endl;
                    return false;
                }
                verbosity = static_cast<LOG_LEVEL>(level);
                break;
            }
            default:
                error << tr("Invalid option or missing argument: ")
                      << QString::fromLocal8Bit(argv[optind - 1]) << Qt::endl;
                return false;
        }
    }
    if (optind < argc)
    {
        error << tr("Unknown option argument:");
        while (optind < argc)
            error << ' ' << QString::fromLocal8Bit(argv[optind++]);
        error << Qt::endl;
        return false;
    }
    return true;
}

void app::print_usage()
{
    QTextStream out(stdout);
    out << "scrobbler (v" << CLIENT_VERSION << "): " << tr("a Last.fm uploader") << Qt::endl
        << Qt::endl << tr("Mandatory arguments to long options are mandatory for short options too.") << Qt::endl
        << Qt::endl << tr("Configuration:") << Qt::endl
        << "  -c, --config=PATH        " << tr("Location of an existing configuration file (optional)") << Qt::endl
        << "                           " << tr("The config file holds site credentials, proxy and timezone settings") << Qt::endl
        << Qt::endl << tr("Methods (choose exactly one):") << Qt::endl
        << "  -d, --database           " << tr("Submit iPod database") << Qt::endl
        << "  -f, --file               " << tr("Submit .scrobbler.log from the directory given by --location") << Qt::endl;
#ifdef HAVE_MTP
    out << "  -m, --mtp                " << tr("Submit MTP device") << Qt::endl;
#endif
    out << Qt::endl << tr("Options:") << Qt::endl
        << "  -h, --help               " << tr("Show this help") << Qt::endl
        << "  -l, --location=DIR       " << tr("Mount point or directory containing the player files") << Qt::endl
        << "  -t, --timezone=HOURS     " << tr("UTC offset in hours (-24 to 24; fractional values allowed)") << Qt::endl
        << "  -n, --now                " << tr("Re-calculate the play time to now") << Qt::endl
        << "  -r, --recalc=TIME        " << tr("Unix timestamp to recalculate backwards from (0 to 2147483647)") << Qt::endl
        << "  -v, --verbose=LEVEL      " << tr("Verbosity level (0=error, 1=info, 2=debug, 3=trace)") << Qt::endl
        << Qt::endl << tr("Auto-detected timezone info:") << Qt::endl
        << tr("Timezone: ") << scrob->get_zonename() << Qt::endl
        << tr("Offset: ") << scrob->offset_str() << Qt::endl
        << tr("Daylight saving: ") << (scrob->get_dst() ? "Yes" : "No") << Qt::endl << Qt::endl;
}

void app::run()
{
    QTextStream error(stderr);
    if (do_time && do_now)
    {
        error << tr("You can only use one of -n and -r") << Qt::endl;
        exit(2);
        return;
    }
    if (do_help)
    {
        print_usage();
        exit(0);
        return;
    }
    int num_method = do_db + do_file;
#ifdef HAVE_MTP
    num_method += do_mtp;
#endif
    if (num_method != 1)
    {
        error << tr("Error - no (single) method specified") << Qt::endl;
        print_usage();
        exit(2);
        return;
    }
    if (do_db)
        method = SCROBBLE_IPOD;
    else if (do_file)
        method = SCROBBLE_LOG;
#ifdef HAVE_MTP
    else
        method = SCROBBLE_MTP;
#endif
    if (!config_path.isEmpty())
    {
        QFile file(config_path);
        if (!QFileInfo(file).isFile() || !file.open(QIODevice::ReadOnly))
        {
            error << tr("Unable to read configuration file: ") << config_path << Qt::endl;
            exit(1);
            return;
        }
        file.close();
        QSettings check(config_path, QSettings::IniFormat);
        check.allKeys();
        if (check.status() != QSettings::NoError)
        {
            error << tr("Invalid configuration file: ") << config_path << Qt::endl;
            exit(1);
            return;
        }
        scrob->conf->set_custom_location(config_path);
    }
    scrob->set_log_level(verbosity);
    if (do_tzoverride)
    {
        scrob->conf->tz_override = true;
        scrob->conf->utc_offset = qRound(tz * 3600);
    }
    scrob->parse(method, path);
}

void app::parsed(bool success)
{
    if (!success)
    {
        QTextStream(stderr) << tr("Error parsing data: ") << scrob->get_error_str() << Qt::endl;
        exit(1);
        return;
    }
    if (metadata_pending)
        return;
    if (do_time)
        scrob->recalc_dt(new_time);
    if (do_now)
        scrob->recalc_now();
    scrob->submit();
}

void app::scrobbled(bool success)
{
    if (success)
        QTextStream(stdout) << tr("Submission complete") << Qt::endl;
    else
        QTextStream(stderr) << tr("Submission failed: ") << scrob->get_error_str() << Qt::endl;
    // Exit after this signal stack unwinds; the application's destructor owns Scrobble.
    exit(success ? 0 : 1);
}
