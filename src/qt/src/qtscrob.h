/***************************************************************************
 *   Copyright (C) 2006-2008 by Tomasz Moń                                 *
 *   Copyright (C) 2009 by Robert Keevil                                   *
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

#ifndef QTSCROB_H
#define QTSCROB_H

#include <QtGui>
#include <QApplication>
#include <QMainWindow>
#include <QMessageBox>
#include <QByteArray>
#include <QtDebug>
#include <QSettings>
#include <QDateTimeEdit>
#include <QLabel>
#include "libscrobble.h"
#include "common.h"
#include "missingtimeprogress.h"

#include <QMenuBar>
#include <QStatusBar>

class Settings;
class About;
class Help;
class Progress;
class Console;
class QApplication;
class QTableWidget;
class QTableWidgetItem;
class QMenu;
class QAction;
class QWidget;
class QStringList;
class QSettings;
class QTranslator;
class QLabel;
class QDateTimeEdit;

class QTScrob : public QMainWindow {
		Q_OBJECT
	protected:
		void closeEvent(QCloseEvent *event);
        bool eventFilter(QObject *obj, QEvent *event);

	private:
		Settings *winSettings;
		About *winAbout;
		Help *winHelp;
		Progress *winProgress;
		Console *winConsole;
		QAction *actOpen;
		QAction *actOpeniTunes;
		QAction *actExit;
		QAction *actHelp;
		QAction *actAbout;
		QAction *actAboutQt;
		QAction *actSettings;
		QAction *actConsole;
		QWidget *centralWidget;
		QTableWidget *logTable;
		QPushButton *btnOpen;
		QPushButton *btnOpeniTunes;
		QPushButton *btnDelete;
		QPushButton *btnSubmit;
		QMenuBar *menuBar;
		QMenu *menuHelp;
		QMenu *menuGlobal;
		QStatusBar *statusbar;
		QLabel *lblStatus;
		QLabel *lblDTformat;
		QDateTimeEdit *DTedit;
		QPushButton *btnRecalcDT;
#ifdef HAVE_MTP
		QAction *actOpenMTP;
		QPushButton *btnOpenMTP;
#endif

		QStringList tableLabels;
		int	items;
		time_t recalc_timestamp;
		QDateTime lastDate;
		QString last_mountpoint;

		void setupWidgets();
		void DT_labels();
		void connectSlots();
		void set_open_buttons();

		//Play Counts
		quint32 entrylen, playcount, dateplayed, bookmark;
		qint32 songs;
		bool passchanged;
		bool display_utc;
		QString dt_format(time_t);

        MissingTimeProgress *progress_missing;

    signals:
        void update_missing_times_progress(int remaining);

	public slots:
		void setupWindow();
		void readWindow();
		void saveWindow();
		void help();
		void about();
		void aboutQt();
		void open(SCROBBLE_METHOD);
		void open_log();
		void open_ipod();
		void settings_close();
#ifdef HAVE_MTP
		void open_mtp();
#endif
		void parse(SCROBBLE_METHOD, QString);
		void parsed(bool);
		void console();
		void deleteRow();
		void changeRow(int, int);
		void scrobble();
		void settings();
		void recalcDT();
		void updaterecalcDT(QDateTime);
		void loadtable();
		void scrobbled(bool);

        void missing_times_start(int total);
        void missing_times_progress(int remaining);
        void missing_times_finished();

	public:
		Scrobble *scrob;
		QFile file;

		/**
		 * Default Constructor
		 */
		QTScrob(QWidget *parent = 0);

		/**
		 * Default Destructor
		 */
		~QTScrob();
};

#endif
