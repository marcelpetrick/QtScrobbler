/***************************************************************************
 *   Copyright (C) 2006-2013 by Robert Keevil                              *
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

#include <cmath> // abs
#include "libscrobble.h"

#include "parse-log.h"
#include "parse-ipod.h"
#ifdef HAVE_MTP
#include "parse-mtp.h"
#endif

#include "dbcache.h"

#ifdef _MSC_VER
// disable "'function': was declared deprecated"
#pragma warning (disable: 4996)
#endif


void Scrobble::clear_method()
{
    scrobble_method = SCROBBLE_NONE;
}

uint Scrobble::get_gmt()
{
    return QDateTime::currentDateTime().toTime_t();
}

Scrobble::Scrobble()
{
    parser = NULL;
    mutex = new QMutex();
    conf = new Conf();
    api_info = new GetTrackInfo();
    cache = new DBCache();
    connect(conf, SIGNAL(add_log(LOG_LEVEL,QString)),
            this, SLOT(add_log(LOG_LEVEL,QString)));
    connect(api_info, SIGNAL(finished(int,bool,int,int)),
            this, SLOT(updated_track_length(int,bool,int,int)));
    connect(api_info, SIGNAL(add_log(LOG_LEVEL,QString)),
            this, SLOT(add_log(LOG_LEVEL,QString)));
    connect(this, SIGNAL(missing_times_get(int,scrob_entry,int)),
            api_info, SLOT(get(int,scrob_entry,int)));
    connect(cache, SIGNAL(add_log(LOG_LEVEL,QString)),
            this, SLOT(add_log(LOG_LEVEL,QString)));
    api_info->start();
    conf->load_config();
    error_str = "";
    scrobble_method = SCROBBLE_NONE;
    log_print_level = LOG_INFO;
    proxy_set = false;
    db_ready = cache->init();

    // belt and braces
    QDateTime a, b;
    a.setTimeSpec(Qt::UTC);
    b.setTimeSpec(Qt::LocalTime);
    a = QDateTime::currentDateTime().toUTC();
    b = QDateTime::currentDateTime().toLocalTime();

    gmt_offset = dt_to_time_t(b) - dt_to_time_t(a);

    /* initialise TZ variables */
    tzset();

    // our own copy - returned via get_dst
    is_dst = daylight;
    int tzindex = (is_dst)?1:0;

    QTextCodec *codec = QTextCodec::codecForLocale();
    zonename = codec->toUnicode(tzname[tzindex]);

    if (is_dst < 0)
        add_log(LOG_ERROR, "is_dst < 0");

    add_log(LOG_DEBUG, "Detected Timezone: " + zonename);
    add_log(LOG_DEBUG, QString("Detected DST: %1").arg(is_dst));
    add_log(LOG_DEBUG, "Detected Offset: " + offset_str());
    add_log(LOG_DEBUG, QString("Detected GMT: %1").arg(get_gmt()));
}

Scrobble::~Scrobble()
{
    api_info->quit();
    api_info->wait();
    delete api_info;
    delete conf;
    delete mutex;
    delete cache;
}

/* mktime() code taken from lynx-2.8.5 source, written
 by Philippe De Muyter <phdm@macqel.be> */
// if only QT had its own time functions...
time_t Scrobble::dt_to_time_t(QDateTime dt)
{
    short month, year;
    time_t result;
    static int m_to_d[12] =
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    month = dt.date().month() - 1;
    year = dt.date().year() + month / 12;
    month %= 12;
    if (month < 0)
    {
        year -= 1;
        month += 12;
    }
    result = (year - 1970) * 365 + (year - 1969) / 4 + m_to_d[month];
    result = (year - 1970) * 365 + m_to_d[month];
    if (month <= 1)
        year -= 1;
    result += (year - 1968) / 4;
    result -= (year - 1900) / 100;
    result += (year - 1600) / 400;
    result += dt.date().day();
    result -= 1;
    result *= 24;
    result += dt.time().hour();
    result *= 60;
    result += dt.time().minute();
    result *= 60;
    result += dt.time().second();
    return (result);
}

/*!
  *
  */
int Scrobble::get_custom_offset()
{
    return (conf->tz_override?conf->utc_offset:gmt_offset);
}

// convenience overloaded function.  Will recalc ALL tracks.
void Scrobble::recalc_dt(int base_dt)
{
    recalc_dt(base_dt, entries.size() - 1);
}

// recalcs datetime backwards from entry_offset
// assumes that the entries have been sorted by datetime.
void Scrobble::recalc_dt(int base_dt, int entry_offset)
{
    for (int i = entry_offset; i >= 0; i--) {
        scrob_entry temp = entries.at(i);
        base_dt -= temp.length;
        temp.when = base_dt;

        update_track(temp, i);
    }
}

void Scrobble::recalc_now()
{
    recalc_dt(get_gmt());
}

int Scrobble::get_num_tracks()
{
    return entries.size();
}

scrob_entry Scrobble::get_track(int track_index)
{
    return entries.at(track_index);
}

void Scrobble::remove_track(int track_index)
{
    entries.erase(entries.begin()+track_index);
}

void Scrobble::update_track(scrob_entry track, int track_index)
{
    entries.replace(track_index, track);
}

void Scrobble::sort_tracks()
{
    // sort by played Date/Time
    qSort(entries.begin(), entries.end(), entry_less_than);
}

// called before submission to ensure data sent is valid
void Scrobble::cleanup_tracks(void)
{
    sort_tracks();

    int size = entries.size();

    //delete tracks that were skipped or too short
    for (int i = 0; i < size; i++) {
        scrob_entry tmp = entries.at(i);

        // reset all previously failed tracks to unsent
        for (int j = 0; j < CONFIG_NUM_SITES; j++)
        {
            if (tmp.sent[j] != SENT_SUCCESS)
                tmp.sent[j] = SENT_UNSENT;
        }
        entries.replace(i, tmp);
        if ( tmp.played != 'L' || tmp.length < 30 )
        {
            //entries.erase(entries.begin() + i);
            entries.removeAt(i);
            size--;
            i--;
        }
    }
}

// called after parsing to ensure all tracks have timestamps
void Scrobble::check_timestamps()
{
    const int size = entries.size();
    int i;

    if (0 == size)
        return;

    sort_tracks();

    // do we have at least one track without date/time info?
    if (0 == entries.at(0).when)
    {
        // do we have ANY tracks with dt info?
        if (0 == entries.at(size - 1).when)
        {
            // no? then recalc them all
            recalc_dt(get_gmt());
        }
        else
        {
            // find where the good date info ends
            for (i = 1; i < size; i++)
            {
                if (0 != entries.at(i).when)
                    break;
            }
            // recalc the bad ones
            recalc_dt(get_gmt(), i);
        }
    }
}

void Scrobble::check_track_lengths()
{
    zero_length_tracks.clear();
    for (int i = 0; i < entries.length(); i++)
    {
        const int len = entries.at(i).length;
        if (0 == len)
        {
            zero_length_tracks.append(i);
        }
        else if (len > 0 && len < 30)
        {
            // set tracks which are too short as skipped so they do not submit
            mutex->lock();
            entries[i].played = 'S';
            mutex->unlock();
        }
    }

    if (db_ready)
    {
        for (int i = zero_length_tracks.length() - 1; i >= 0 ; i--)
        {
            int index = zero_length_tracks[i];
            int length = cache->get_track_length(entries[index].artist,
                                                 entries[index].title);
            if (length != -1)
            {
                mutex->lock();
                entries[index].length = length;
                mutex->unlock();
                zero_length_tracks.removeAt(i);
            }
        }
    }

    if (!zero_length_tracks.isEmpty())
    {
        add_log(LOG_INFO, tr("Fetching missing track lengths from last.fm..."));
        emit missing_times_start(zero_length_tracks.length());
        int index = zero_length_tracks.takeFirst();
        emit missing_times_get(index, entries.at(index), 0);
    }
}

void Scrobble::updated_track_length(int index, bool success, int length, int elapsed)
{

    const int minimum_time = 200;
    if (success)
    {
        mutex->lock();
        entries[index].length = length;
        cache->set_track_length(entries[index].artist, entries[index].title, length);
        mutex->unlock();
    }
    if (!zero_length_tracks.isEmpty())
    {
        emit missing_times_progress(zero_length_tracks.length());
        int target = zero_length_tracks.takeFirst();
        emit missing_times_get(target, entries.at(target),
                               (elapsed < minimum_time)?(minimum_time-elapsed):0);
    }
    else
        emit missing_times_finished();
}

QString Scrobble::offset_str(void)
{
    QChar sign = (gmt_offset < 0)?'-':'+';
    int hour = abs(gmt_offset) / 3600;
    int min = (abs(gmt_offset) % 3600) / 60;
    QString hour_str = QString::number(hour).rightJustified(2, '0');
    QString min_str = QString::number(min).rightJustified(2, '0');

    return QString("UTC %1%2:%3")
            .arg(sign)
            .arg(hour_str, min_str);
}

void Scrobble::add_log(LOG_LEVEL level, QString msg)
{
    log_entry tmp;
    tmp.level = level;
    tmp.msg = msg;
    log_messages.push_back(tmp);

    if (level <= log_print_level)
        QTextStream(stdout) << LOG_LEVEL_NAMES[level] << ": " << msg << endl;

    if (LOG_ERROR == level)
        error_str = msg;
}

int Scrobble::get_num_logs()
{
    return log_messages.size();
}

log_entry Scrobble::get_log(int index)
{
    return log_messages.at(index);
}

void Scrobble::clear_log()
{
    log_messages.clear();
}

void Scrobble::set_log_level(LOG_LEVEL level)
{
    log_print_level = level;
}

bool Scrobble::check_age()
{
    bool too_old = false;
    // Now - 30 days
    time_t max_age = get_gmt() - 2592000;
    for ( int i = 0; i < entries.size(); i++) {
        scrob_entry tmp = entries.at(i);
        if ( tmp.when < max_age )
        {
            add_log(LOG_INFO, "Track too old: " + tmp.artist + " " + tmp.title);
            too_old = true;
        }
    }
    return too_old;
}

bool Scrobble::submit()
{
    if (entries.empty())
    {
        add_log(LOG_ERROR, tr("Nothing to submit!"));
        emit submission_finished(false);
        return false;
    }

    // May already have been called, but better to be safe.
    cleanup_tracks();

    if (check_age())
    {
        add_log(LOG_ERROR,
                tr("One or more tracks are too old.  Correct this and try again."));
        emit submission_finished(false);
        return false;
    }

    set_proxy();

    Submit::submit_context conn;

    conn.entries = &entries;
    conn.have_mb = have_mb;

    submissions.clear();
    num_submissions = 0;
    for (int i = 0; i < CONFIG_NUM_SITES; i++)
    {
        if (conf->sites[i].enabled
            && !conf->sites[i].username.isEmpty()
            && !conf->sites[i].password_hash.isEmpty()
            && !conf->sites[i].handshake_host.isEmpty()
            && !conf->sites[i].conf_name.isEmpty())
        {
            num_submissions++;
            conn.username = conf->sites[i].username;
            conn.password_hash = conf->sites[i].password_hash;
            conn.host = conf->sites[i].handshake_host;
            conn.site_index = i;
            conn.mutex = mutex;

            Submit *sub = new Submit(i);
            submissions.insert(i, sub);

            submissions[i]->init(conn);

            connect(submissions[i],
                    SIGNAL(finished(bool, QString)),
                    this, SLOT(submit_finished(bool, QString)));
            connect(submissions[i],
                    SIGNAL(add_log(LOG_LEVEL,QString)),
                    this, SLOT(add_log(LOG_LEVEL, QString)));

            submissions[i]->do_submit();
        }
    }
    if (0 == submissions.size())
    {
        add_log(LOG_ERROR,
                tr("No sites were configured and/or enabled. Unable to submit"));
        emit submission_finished(false);
        return false;
    }

    add_log(LOG_INFO, "Scrobble::submit() finished");

    return true;
}

void Scrobble::submit_finished(bool success, QString msg)
{
    add_log(LOG_INFO, "Submit() complete");
    if (!msg.isEmpty())
        add_log(LOG_ERROR, msg);

    if (!success)
    {
        parser->disconnect();
        delete parser;
        parser = NULL;
    }

    num_submissions--;

    if (0 == num_submissions)
    {
        clear_submission_state(success);
    }
}

void Scrobble::remove_sent()
{
    for (int i = entries.size() - 1; i >= 0; i--)
    {
        bool submitted = true;

        QHashIterator<int, Submit *> j(submissions);
        while (j.hasNext())
        {
            j.next();
            if (SENT_SUCCESS != entries[i].sent[j.value()->index])
                submitted = false;
        }
        if (submitted)
            entries.removeAt(i);
    }
}

bool Scrobble::parse(SCROBBLE_METHOD method, QString path)
{
    int offset = get_custom_offset();
    if (parser != NULL)
    {
        add_log(LOG_ERROR, tr("Asked to create a duplicate parser"));
        return false;
    }

    scrobble_method = SCROBBLE_NONE;
    entries.clear();
    switch (method)
    {
        case SCROBBLE_LOG:
            parser = new Parse_Log();
            break;
        case SCROBBLE_IPOD:
            parser = new Parse_Ipod();
            break;
#ifdef HAVE_MTP
        case SCROBBLE_MTP:
            parser = new Parse_MTP();
            break;
#endif
        case SCROBBLE_NONE:
        default:
            add_log(LOG_ERROR, tr("Error - asked to use an unknown Parser!"));
            return false;
            break;
    }
    scrobble_method = method;

    connect(parser, SIGNAL(add_log(LOG_LEVEL, QString)),
            this, SLOT(add_log(LOG_LEVEL, QString)));
    connect(parser, SIGNAL(entry(scrob_entry)),
            this, SLOT(add_entry(scrob_entry)));
    connect(parser, SIGNAL(open_finished(bool, QString)),
            this, SLOT(parser_open_finished(bool, QString)));
    connect(parser, SIGNAL(clear_finished(bool, QString)),
            this, SLOT(parser_clear_finished(bool, QString)));

    parser->open(path, offset);

    return true;
}

void Scrobble::parse_open(QString path, int offset)
{
    if (parser != NULL)
        parser->open(path, offset);
}

void Scrobble::add_entry(scrob_entry new_entry)
{
    entries.push_back(new_entry);
}

void Scrobble::parser_open_finished(bool success, QString msg)
{
    if (success)
    {
        add_log(LOG_INFO, msg);
        check_track_lengths();
        check_timestamps();
    }
    else
    {
        add_log(LOG_ERROR, msg);
        error_str = msg;
        parser->disconnect();
        delete parser;
        parser = NULL;
    }
    // hot potato, hot potato, pass it on, pass it on
    emit parsing_opened(success);
}

void Scrobble::parser_clear()
{
    if (parser != NULL)
        parser->clear();
    else
        add_log(LOG_ERROR, tr("Parser: asked to clear without a live object"));
}

void Scrobble::parser_clear_finished(bool success, QString msg)
{
    if (success)
    {
        add_log(LOG_INFO, msg);
    }
    else
    {
        add_log(LOG_ERROR, msg);
        error_str = msg;
    }

    // all done, next usage, if any, can create its own shiney new one.
    parser->disconnect();
    delete parser;
    parser = NULL;

    // pass it on up the chain
    emit parsing_cleared(success);
}

void Scrobble::set_proxy()
{
    if (!conf->proxy_host.isEmpty())
    {
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::HttpProxy);
        proxy.setHostName(conf->proxy_host);
        proxy.setPort(conf->proxy_port);
        proxy.setUser(conf->proxy_user);
        proxy.setPassword(conf->proxy_pass);
        QNetworkProxy::setApplicationProxy(proxy);
        proxy_set = true;
    }
    else if (proxy_set)
    {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    }
}

void Scrobble::cancel_submission()
{
    QHashIterator<int, Submit *> i(submissions);
    while (i.hasNext())
    {
        i.next();
        i.value()->cancel_submission();
    }
    clear_submission_state(false);
}

void Scrobble::clear_submission_state(bool success)
{
    bool overall_success = true;

    // parser will disconnect & delete itself after clearing
    remove_sent();

    // Remove sumbission classes now we are finished with them
    QHashIterator<int, Submit *> i(submissions);
    while (i.hasNext())
    {
        i.next();
        if (!i.value()->submission_successful())
            overall_success = false;
        // remove all signals/slots
        i.value()->disconnect();
        i.value()->deleteLater();
    }

    // If nothing was sent (checks failed - tracks too old?) do not clear device
    if (0 == submissions.size())
        overall_success = false;

    // parser will disconnect & delete itself after clearing
    if (overall_success)
        parser_clear();
    else
    {
        parser->disconnect();
        delete parser;
        parser = NULL;
    }

    emit submission_finished(success);
}
