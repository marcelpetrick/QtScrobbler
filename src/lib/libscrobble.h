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

#ifndef LIBSCROBBLE_H
#define LIBSCROBBLE_H

#include <QtCore>
#include <QString>

#include "submit.h"
#include "common.h"
#include "parse.h"
#include "conf.h"
#include "gettrackinfo.h"
#include "dbcache.h"

#include <ctime>

#define HAVE_ITUNESDB  // to enable ipod support
#define HAVE_LOG       // to enable portable player log support

class Submit;

class Scrobble : public QObject
{
    Q_OBJECT
private:
    int num_submissions;
    Parse *parser;
    QMutex *mutex;

    QString error_str;
    bool have_mb;
    SCROBBLE_METHOD scrobble_method;

    QList<scrob_entry> entries;
    QList<log_entry> log_messages;

    LOG_LEVEL log_print_level;

    void sort_tracks();
    void cleanup_tracks(void);
    void check_timestamps();
    void check_track_lengths();
    GetTrackInfo *api_info;
    QList<int> zero_length_tracks;
    void remove_sent();
    void clear_submission_state(bool);

    int is_dst;
    QString zonename;
    int gmt_offset;

    void set_proxy();
    bool proxy_set;

    DBCache *cache;
    bool db_ready;

private slots:
    void submit_finished(bool, QString);
    void add_log(LOG_LEVEL, QString);
    void add_entry(scrob_entry);
    void parser_open_finished(bool, QString);
    void parser_clear_finished(bool, QString);
    void updated_track_length(int index, bool success, int length, int elapsed);

signals:
    void submission_finished(bool success);
    void parsing_opened(bool success);
    void parsing_cleared(bool success);
    void missing_times_start(int total);
    void missing_times_progress(int remaining);
    void missing_times_finished();
    void missing_times_get(int index, scrob_entry info, int wait);

public:
    Scrobble();
    ~Scrobble();

    QHash<int, Submit *> submissions;
    Conf *conf;
    bool parse(SCROBBLE_METHOD method, QString path);
    void parse_open(QString, int);
    void parser_clear();
    void recalc_dt(int);
    void recalc_dt(int, int);
    void recalc_now();
    int get_num_tracks();
    scrob_entry get_track(int);
    void remove_track(int);
    void update_track(scrob_entry, int);
    bool submit();
    QString get_error_str() { return error_str; }
    //bool mb_present() { return have_mb; };
    //int get_last_method() { return scrobble_method; };
    bool get_parser_busy() { return (parser==NULL)?false:true; }
    void clear_method();
    int get_num_logs();
    log_entry get_log(int);
    void clear_log();
    void set_log_level(LOG_LEVEL);
    bool check_age();

    // required for submission
    uint get_gmt();

    void cancel_submission();

    int get_dst() { return is_dst; };
    int get_gmt_offset() { return gmt_offset; };
    int get_custom_offset();
    QString get_zonename() { return zonename; };
    QString offset_str();
};

#endif
