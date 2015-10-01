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

#ifdef _MSC_VER
#include "xgetopt.h"
#else
#include <getopt.h>
#endif

app::app(int& argc, char** argv) : QCoreApplication(argc, argv)
{
    scrob = new Scrobble();
    method = SCROBBLE_NONE;

    connect(scrob, SIGNAL(parsing_opened(bool)), this, SLOT(parsed(bool)));
    connect(scrob, SIGNAL(submission_finished(bool)), this, SLOT(scrobbled(bool)));
}

bool app::parse_cmd(int argc, char** argv)
{
    QTextStream out(stdout);

    do_time = false;
    do_tzoverride = false;
    do_db = 0;
    do_file = 0;
#ifdef HAVE_MTP
    do_mtp = 0;
#endif
    do_now = 0;
    do_help = 0;
    new_time = -1;
    tz = 0;
    verbosity = LOG_DEFAULT;

    // parse command line
    int c;


    while (1)
    {
        static struct option long_options[] =
        {
            {"help",            no_argument,       &do_help, 1},
            {"database",        no_argument,       &do_db,   1},
            {"file",            no_argument,       &do_file, 1},
#ifdef HAVE_MTP
            {"mtp",             no_argument,       &do_mtp,  1},
#endif
            {"now",             no_argument,       &do_now,  1},
            {"config",          required_argument, 0,        'c'},
            {"timezone",        required_argument, 0,        't'},
            {"location",        required_argument, 0,        'l'},
            {"verbose",         required_argument, 0,        'v'},
            {"recalc",          required_argument, 0,        'r'},
            {0, 0, 0, 0}
        };

        int option_index = 0;

        c = getopt_long(argc, argv, "c:dfhl:mnr:t:v:",
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
            case 'd':
                do_db = 1;
                break;
            case 'f':
                do_file = 1;
                break;
            case 'h':
                do_help = 1;
                break;
            case 'l':
                path = optarg;
                break;
#ifdef HAVE_MTP
            case 'm':
                do_mtp = 1;
                break;
#endif
            case 'n':
                do_now = 1;
                break;
            case 'r':
                do_time = true;
                new_time = strtol(optarg, NULL, 10);
                break;
            case 't':
                do_tzoverride = true;
                tz = atof(optarg);
                break;
            case 'v':
                verbosity = (LOG_LEVEL)strtol(optarg, NULL, 10);
                break;
            case '?':
                break;
            default:
                out << "Unknown option \"" << QChar(c) << "\"" << endl;
                return false;
        }
    }

    if (optind < argc)
    {
        out << tr("Unknown option argument:");
        while (optind < argc)
            out << " " << argv[optind++];
        out << endl;
        return false;
    }
    return true;
}

void app::print_usage()
{
    QTextStream out(stdout);
    QString dst;
    if (scrob->get_dst())
        dst = "Yes";
    else
        dst = "No";

    out << "scrobbler (v" << CLIENT_VERSION << "): " << tr("a Last.fm uploader") << endl;
    out << endl;
    out << tr("Mandatory arguments to long options are mandatory for short options too.") << endl;
    out << endl;
    out << tr("Required:") << endl;
    out << "  -c, --config             " << tr("Location to the config file") << endl;
    out << "                           " << tr("The config file holds the site configurations") << endl;
    out << "                           " << tr("(usernames, passwords), proxy info, timezone offsets etc") << endl;
    out << endl;
    out << tr("Methods:") << endl;
    out << "  -d, --database           " << tr("Submit ipod database") << endl;
    out << "  -f, --file               " << tr("Submit .scrobbler.log file") << endl;
#ifdef HAVE_MTP
    out << "  -m, --mtp                " << tr("Submit MTP device") << endl;
#endif
    out << endl;
    out << "Options:" << endl;
    out << "  -l, --location           " << tr("Mount point of the player") << endl;
    out << "  -t, --timestamp=WHEN     " << tr("UNIX timestamp to recalculate from") << endl;
    out << "  -n, --now                " << tr("Re-calculate the play time to now") << endl;
    out << "  -r, --recalc=TIME        " << tr("Re-calculate the time when tracks were played") << endl;
    out << "  -v, --verbose=LEVEL      " << tr("Verbosity level") << endl;
    out << endl;
    out << tr("Auto-detected timezone info:") << endl;
    out << tr("Timezone: ") << scrob->get_zonename() << endl;
    out << tr("Offset: ") << scrob->offset_str() << endl;
    out << tr("Daylight saving: ") << dst << endl;
    out << endl;
}

void app::run()
{
    QTextStream out(stdout);
    if (!config_path.isEmpty())
        scrob->conf->set_custom_location(config_path);

    scrob->set_log_level(verbosity);

    if (do_time && do_now)
    {
        out << tr("You can only use one of -n and -r") << endl;
        delete scrob;
        this->quit();
        return;
    }

    if (do_help)
    {
        print_usage();
        delete scrob;
        this->quit();
        return;
    }

    // check one method exists
    int num_method = 0;

    if (do_db)
    {
        method = SCROBBLE_IPOD;
        num_method++;
    }
    else if (do_file)
    {
        method = SCROBBLE_LOG;
        num_method++;
    }
#ifdef HAVE_MTP
    else if (do_mtp)
    {
        method = SCROBBLE_MTP;
        num_method++;
    }
#endif

    if (num_method != 1)
    {
        out << tr("Error - no (single) method specified") << endl;
        print_usage();
        delete scrob;
        this->quit();
        return;
    }

    scrob->parse(method, path);
}

void app::parsed(bool success)
{
    QTextStream out(stdout);
    if (!success)
    {
        out << tr("Error parsing data: ") << scrob->get_error_str() << endl;
        this->quit();
        return;
    }

    if (do_time)
        scrob->recalc_dt(new_time);

    if (do_now)
        scrob->recalc_now();

    scrob->submit();
}

void app::scrobbled(bool success)
{
    QTextStream out(stdout);
    if (!success)
    {
        out << tr("Submission failed: ") << scrob->get_error_str() << endl;
    } else {
        out << tr("Submission complete") << endl;
    }
    delete scrob;
    this->quit();
}
