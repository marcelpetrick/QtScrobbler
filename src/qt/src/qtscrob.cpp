/***************************************************************************
 *   Copyright (C) 2006-2009 by Tomasz Moń                                 *
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

#include "qtscrob.h"
#include "settings.h"
#include "about.h"
#include "help.h"
#include "progress.h"
#include "console.h"

#include <QTime>
#include <QTableWidget>
#include <QCloseEvent>
#include <QFileDialog>

#ifdef _MSC_VER
// disable "'function': was declared deprecated"
#pragma warning (disable: 4996)
#endif

QTScrob::QTScrob(QWidget *parent) : QMainWindow( parent ) {
	scrob = new Scrobble();

	logTable = new QTableWidget();
	tableLabels << tr("Artist") << tr("Album") << tr("Title") << tr("TrackNumber") << tr("Length") << tr("Played") << "";
	logTable->setColumnCount(7);
	logTable->setRowCount(0);
	logTable->setHorizontalHeaderLabels(tableLabels);
	logTable->setAlternatingRowColors(true);
    logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    logTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    logTable->installEventFilter(this);

    recalc_timestamp = scrob->get_gmt();
}

QTScrob::~QTScrob() {
	if (winSettings!=NULL)
		delete winSettings;

	if (winConsole != NULL)
		delete winConsole;

	delete scrob;
}

void QTScrob::closeEvent(QCloseEvent *event)
{
	saveWindow();
	scrob->conf->save_config();
	event->accept();
}

void QTScrob::setupWindow() {
	readWindow();
	setupWidgets();
	connectSlots();
}

void QTScrob::readWindow() {
	QHash <QString, QVariant> values = scrob->conf->load_custom("window");

	resize(values.value("size", QSize(705, 458)).toSize());
	move(values.value("pos", QPoint(100, 100)).toPoint());

	if (values.value("maximized", "false").toBool())
	{
		setWindowState(windowState() | Qt::WindowMaximized);
	}
}

void QTScrob::saveWindow() {
	QHash <QString, QVariant> values;
	values.insert("maximized", isMaximized());
	if (!isMaximized())
	{
		values.insert("size", size());
		values.insert("pos", pos());
	}

	scrob->conf->save_custom("window", values);
}

void QTScrob::setupWidgets() {
	actOpen = new QAction(tr("Open"),this);
	actOpeniTunes = new QAction(tr("Open iTunesDB"),this);
	actExit = new QAction(tr("Exit"),this);
	actHelp = new QAction(tr("Help"),this);
	actAbout = new QAction(tr("About"),this);
    actAboutQt = new QAction(tr("About Qt"),this);
	actSettings = new QAction(tr("Settings"),this);
	actConsole = new QAction(tr("Console"), this);

	centralWidget = new QWidget(this);

	btnOpen = new QPushButton(tr("Open .scrobbler.log"));
	btnOpeniTunes = new QPushButton(tr("Open iTunesDB"));
	btnDelete = new QPushButton(tr("Delete selected item"));
	btnSubmit = new QPushButton(tr("Submit"));

	btnOpen->setIcon(QIcon::fromTheme("document-open",
							QIcon(":/resources/icons/document-open.png")));
	btnDelete->setIcon(QIcon::fromTheme("edit-clear",
							QIcon(":/resources/icons/edit-clear.png")));
	btnSubmit->setIcon(QIcon::fromTheme("mail-send-receive",
							QIcon(":/resources/icons/mail-send-receive.png")));

	btnOpeniTunes->setIcon(QIcon(":/resources/icons/16x16/qtscrob.png"));

#ifdef HAVE_MTP
	actOpenMTP = new QAction(tr("Open MTP Device"), this);
	actOpenMTP->setIcon(QIcon(":/resources/icons/multimedia-player-iriver-s10_16.png"));
	btnOpenMTP = new QPushButton(tr("Open MTP Device"));
	btnOpenMTP->setIcon(QIcon(":/resources/icons/multimedia-player-iriver-s10_16.png"));
#endif

	QGridLayout *layout = new QGridLayout(centralWidget);
	layout->setMargin(0);
	layout->setColumnStretch(0, 1);
	layout->addWidget(btnOpen, 0, 1);
	layout->addWidget(btnOpeniTunes, 0, 2);
#ifdef HAVE_MTP
	layout->addWidget(btnOpenMTP, 0, 3);
#endif
	layout->addWidget(btnDelete, 0, 4);
	layout->addWidget(btnSubmit, 0, 5);
	layout->setColumnStretch(6, 1);
	layout->addWidget(logTable, 1, 0, 1, 7);

	this->setCentralWidget(centralWidget);

	menuBar = new QMenuBar(this);
	menuBar->setGeometry(QRect(0, 0, 705, 26));
	menuHelp = new QMenu(tr("Help"),menuBar);
	menuGlobal = new QMenu(tr("Global"),menuBar);
	this->setMenuBar(menuBar);
	statusbar = new QStatusBar(this);
	this->setStatusBar(statusbar);

	lblStatus = new QLabel(tr("Ready"));
	lblDTformat = new QLabel();
	DTedit = new QDateTimeEdit();
	btnRecalcDT = new QPushButton(tr("Recalc Date/Time"));
	statusbar->addWidget(lblStatus, 1);
	statusbar->addWidget(lblDTformat);
	statusbar->addWidget(DTedit);
	statusbar->addWidget(btnRecalcDT);

	btnRecalcDT->setIcon(QIcon::fromTheme("appointment-new",
							QIcon(":/resources/icons/appointment-new.png")));
	actSettings->setIcon(QIcon::fromTheme("preferences-system",
							QIcon(":/resources/icons/preferences-system.png")));
	actHelp->setIcon(QIcon::fromTheme("help-browser",
							QIcon(":/resources/icons/help-browser.png")));
	actExit->setIcon(QIcon::fromTheme("application-exit",
							QIcon(":/resources/icons/system-log-out.png")));
	actOpen->setIcon(QIcon::fromTheme("document-open",
							QIcon(":/resources/icons/document-open.png")));

	actOpeniTunes->setIcon(QIcon(":/resources/icons/16x16/qtscrob.png"));

	actHelp->setShortcut(QKeySequence::HelpContents);

	menuBar->addAction(menuGlobal->menuAction());
	menuBar->addAction(menuHelp->menuAction());
	menuHelp->addAction(actHelp);
	menuHelp->addAction(actConsole);
	menuHelp->addAction(actAbout);
	menuHelp->addAction(actAboutQt);
	menuGlobal->addAction(actOpen);
	menuGlobal->addAction(actOpeniTunes);
#ifdef HAVE_MTP
	menuGlobal->addAction(actOpenMTP);
#endif
	menuGlobal->addAction(actSettings);
	menuGlobal->addSeparator();
	menuGlobal->addAction(actExit);

	this->setWindowTitle(tr("QTScrobbler"));

	winSettings = NULL;
	winAbout = NULL;
	winHelp = NULL;
	winProgress = NULL;
	winConsole = NULL;
	passchanged = false;

	DT_labels();
}

void QTScrob::DT_labels()
{
	struct tm * tmp;
	time_t tmp_dt;

	if(scrob->conf->display_utc)
	{
		logTable->horizontalHeaderItem(6)->setText(tr("Date (UTC)"));
		lblDTformat->setText(tr("Adjust from (UTC):"));
		tmp_dt = recalc_timestamp;
	}
	else
	{
		logTable->horizontalHeaderItem(6)->setText(tr("Date (Local)"));
		lblDTformat->setText(tr("Adjust from (Local):"));
		tmp_dt = recalc_timestamp + scrob->get_custom_offset();
	}

	tmp = gmtime(&tmp_dt);

	QDate tmp_date(tmp->tm_year + 1900,
			tmp->tm_mon + 1,
			tmp->tm_mday);

	QTime tmp_time(tmp->tm_hour,
			tmp->tm_min,
			tmp->tm_sec);

	DTedit->setDateTime(QDateTime(tmp_date, tmp_time));
}

void QTScrob::connectSlots() {
	connect(actOpen, SIGNAL(triggered()), this, SLOT(open_log()));
	connect(actExit, SIGNAL(triggered()), this, SLOT(close()));
	connect(actHelp, SIGNAL(triggered()), this, SLOT(help()));
	connect(actAbout, SIGNAL(triggered()), this, SLOT(about()));
	connect(actAboutQt, SIGNAL(triggered()), this, SLOT(aboutQt()));
	connect(actOpeniTunes, SIGNAL(triggered()), this, SLOT(open_ipod()));
	connect(actConsole, SIGNAL(triggered()), this, SLOT(console()));
#ifdef HAVE_MTP
	connect(actOpenMTP, SIGNAL(triggered()), this, SLOT(open_mtp()));
	connect(btnOpenMTP, SIGNAL(clicked()), this, SLOT(open_mtp()));
#endif
	connect(btnOpeniTunes, SIGNAL(clicked()), this, SLOT(open_ipod()));
	connect(btnOpen, SIGNAL(clicked()), this, SLOT(open_log()));
	connect(btnDelete, SIGNAL(clicked()), this, SLOT(deleteRow()));
	connect(btnSubmit, SIGNAL(clicked()), this, SLOT(scrobble()));
	connect(actSettings, SIGNAL(triggered()), this, SLOT(settings()));
	connect(btnRecalcDT, SIGNAL(clicked()), this, SLOT(recalcDT()));
    connect(DTedit, SIGNAL(dateTimeChanged(QDateTime)),
            this, SLOT(updaterecalcDT(QDateTime)));
    connect(logTable, SIGNAL(cellChanged(int, int)),
            this, SLOT(changeRow(int, int)));
    connect(scrob, SIGNAL(submission_finished(bool)),
            this, SLOT(scrobbled(bool)));
	connect(scrob, SIGNAL(parsing_opened(bool)), this, SLOT(parsed(bool)));

    connect(scrob, SIGNAL(missing_times_start(int)),
            this, SLOT(missing_times_start(int)));
    connect(scrob, SIGNAL(missing_times_progress(int)),
            this, SLOT(missing_times_progress(int)));
    connect(scrob, SIGNAL(missing_times_finished()),
            this, SLOT(missing_times_finished()));
}

void QTScrob::help() {
	if (winHelp==NULL)
		winHelp = new Help(this);
	winHelp->show();
}

void QTScrob::about() {
	if (winAbout==NULL)
		winAbout = new About(this);
	winAbout->show();
}

void QTScrob::settings() {
	if (winSettings==NULL)
		winSettings = new Settings(this);
	winSettings->show();
}

void QTScrob::console() {
	if (winConsole==NULL)
		winConsole = new Console(this);
	winConsole->show();
}

void QTScrob::aboutQt() {
	QApplication::aboutQt();
}

void QTScrob::open_log()
{
	open(SCROBBLE_LOG);
}

void QTScrob::open_ipod()
{
	open(SCROBBLE_IPOD);
}

#ifdef HAVE_MTP
void QTScrob::open_mtp()
{
	open(SCROBBLE_MTP);
}
#endif

void QTScrob::open(SCROBBLE_METHOD method) {
#ifdef HAVE_MTP
	if (SCROBBLE_MTP == method)
	{
		parse(method, "");
		return;
	}
#endif
	if (scrob->conf->auto_open &&
		!scrob->conf->mru.isEmpty()
        && ( check_path(method, scrob->conf->mru.first())
             != CHECK_FAIL ))
		parse(method, scrob->conf->mru.first());
	else
	{
		QFileDialog *dlg = new QFileDialog;
		dlg->setFileMode(QFileDialog::DirectoryOnly);
		if (!scrob->conf->mru.isEmpty())
			dlg->setHistory(scrob->conf->mru);

		QString mount_root;

		if (!scrob->conf->mru.isEmpty() &&
			QDir(scrob->conf->mru.first()).exists())
			mount_root = scrob->conf->mru.first();
		else
#if defined(Q_OS_DARWIN)
			mount_root = "/Volumes";
#elif defined(Q_OS_LINUX)
			mount_root = "/media";
#else
			mount_root = "/";
#endif

		QString mount_point = dlg->getExistingDirectory(0,
#if defined(Q_OS_WIN32)
                tr("Select media player drive letter"),
#else
                tr("Select media player mountpoint"),
#endif
				mount_root,
				QFileDialog::ShowDirsOnly);

		if (mount_point.isEmpty())
			return;
		parse(method, mount_point);
	}
}

void QTScrob::parse(SCROBBLE_METHOD method, QString fileDir)
{
	if (!fileDir.isEmpty())
		last_mountpoint = fileDir;
	scrob->parse(method, fileDir);
}

void QTScrob::parsed(bool success)
{
	if (!success)
	{
		QMessageBox::warning(this, QCoreApplication::applicationName(),
							 tr("Error parsing: %1").arg(scrob->get_error_str()));
		return;
	}

	loadtable();

	lblStatus->setText(tr("Loaded %1 entries").arg(logTable->rowCount()));

	// move the most recent entry to the start of the mru stringlist
	if (!last_mountpoint.isEmpty())
	{
		if (scrob->conf->mru.contains(last_mountpoint))
			scrob->conf->mru.removeAll(last_mountpoint);

		scrob->conf->mru.prepend(last_mountpoint);
	}

	set_open_buttons();
}

void QTScrob::loadtable(void) {
	int i;
	scrob_entry scrob_tmp;

	logTable->disconnect(SIGNAL(cellChanged(int, int)));

	logTable->clearContents();

	logTable->setRowCount(scrob->get_num_tracks());

	QApplication::setOverrideCursor(Qt::WaitCursor);

	for (i = 0; i < scrob->get_num_tracks(); i++) {
		scrob_tmp = scrob->get_track(i);

		QTableWidgetItem *temp_artist = new QTableWidgetItem;
		temp_artist->setText(scrob_tmp.artist);
		logTable->setItem(i, 0, temp_artist );

		QTableWidgetItem *temp_album = new QTableWidgetItem;
		temp_album->setText(scrob_tmp.album);
		logTable->setItem(i, 1, temp_album );

		QTableWidgetItem *temp_title = new QTableWidgetItem;
		temp_title->setText(scrob_tmp.title);
		logTable->setItem(i, 2, temp_title );

		QTableWidgetItem *temp_tracknum = new QTableWidgetItem;
		temp_tracknum->setText(QString::number(scrob_tmp.tracknum));
		logTable->setItem(i, 3, temp_tracknum );

		QTableWidgetItem *temp_length = new QTableWidgetItem;
		temp_length->setText(QString::number(scrob_tmp.length, 10));
		logTable->setItem(i, 4, temp_length );

		QTableWidgetItem *temp_played = new QTableWidgetItem;
		temp_played->setText(QChar(scrob_tmp.played));
		logTable->setItem(i, 5, temp_played );

		QTableWidgetItem *temp_when = new QTableWidgetItem;
		QString when;

		if (scrob->conf->display_utc)
		{
			when = dt_format(scrob_tmp.when);
		}
		else
		{
			when = dt_format(scrob_tmp.when + scrob->get_custom_offset());
		}
		temp_when->setText(when);

		logTable->setItem(i, 6, temp_when );

	}
	QApplication::restoreOverrideCursor();

	connect(logTable, SIGNAL(cellChanged(int, int)), this, SLOT(changeRow(int, int)));
}

/*!
  * Using the built-in QDateTime::toString results in Qt being clever and
  * adjusting for locale - we just want the straight time.
  */
QString QTScrob::dt_format(time_t when)
{
	struct tm * tmp = gmtime(&when);

	return QString("%1-%2-%3 %4:%5:%6")
			.arg(QString::number(tmp->tm_year + 1900).rightJustified(4, '0'))
			.arg(QString::number(tmp->tm_mon + 1).rightJustified(2, '0'))
			.arg(QString::number(tmp->tm_mday).rightJustified(2, '0'))
			.arg(QString::number(tmp->tm_hour).rightJustified(2, '0'))
			.arg(QString::number(tmp->tm_min).rightJustified(2, '0'))
			.arg(QString::number(tmp->tm_sec).rightJustified(2, '0'));
}

void QTScrob::updaterecalcDT(QDateTime newrecalcDT)
{
	// temp value - we just want to setup the tm struct
	struct tm * tmp = gmtime(&recalc_timestamp);

	tmp->tm_year = newrecalcDT.date().year() - 1900;
	tmp->tm_mon = newrecalcDT.date().month() - 1;
	tmp->tm_mday = newrecalcDT.date().day();

	tmp->tm_hour = newrecalcDT.time().hour();
	tmp->tm_min = newrecalcDT.time().minute();
	tmp->tm_sec = newrecalcDT.time().second();

	tmp->tm_isdst = 0;
	tmp->tm_wday = newrecalcDT.date().dayOfWeek();
	tmp->tm_yday = newrecalcDT.date().dayOfYear();

#ifdef _MSC_VER
	recalc_timestamp = _mkgmtime(tmp);
#else
	tmp->tm_gmtoff = 0;
	recalc_timestamp = timegm(tmp);
#endif

	if (!scrob->conf->display_utc)
		recalc_timestamp -= scrob->get_custom_offset();
}

void QTScrob::recalcDT(void) {

	scrob->recalc_dt(recalc_timestamp);

	loadtable();
}

void QTScrob::deleteRow() {
	if (logTable->rowCount()) { // Items are displayed. 
	if (logTable->rowCount() == 1) { // Handle it as one item. 
		QTableWidgetItem *artist = new QTableWidgetItem();
		QTableWidgetItem *title = new QTableWidgetItem();
		artist = logTable->item(logTable->currentRow(),0);
		title = logTable->item(logTable->currentRow(),2);
		QMessageBox mb(QCoreApplication::applicationName(),
			tr("Are you sure you want to delete: %1 - %2?")
			.arg(artist->text(), title->text()),
				QMessageBox::Information,
				QMessageBox::Yes | QMessageBox::Default,
				QMessageBox::No, 0);
			mb.setButtonText(QMessageBox::Yes, "Yes");
			mb.setButtonText(QMessageBox::No, "No");
			switch(mb.exec()) {
				case QMessageBox::Yes:
					scrob->remove_track(logTable->currentRow());
					break;
				case QMessageBox::No:
					break;
			}
	} else { // More than one item. 
		QMessageBox mb(QCoreApplication::applicationName(), \
			tr("Are you sure you want to delete the selected items") + "?", \
			QMessageBox::Information, \
			QMessageBox::Yes | QMessageBox::Default, \
			QMessageBox::No, 0);
		mb.setButtonText(QMessageBox::Yes, "Yes");
		mb.setButtonText(QMessageBox::No, "No");
		switch(mb.exec()) {
			case QMessageBox::Yes:
				for (int i = logTable->rowCount() - 1; i >= 0; i--) { 
					if (logTable->item(i, 0)->isSelected())
						scrob->remove_track(i); 
				}

				break;
			case QMessageBox::No:
				break;
			}
		}
	}

	loadtable();
	if (scrob->get_num_tracks() == 0)
	{
		scrob->clear_method();
		set_open_buttons();
	}
}

void QTScrob::changeRow(int r, int c) {
	scrob_entry tmp = scrob->get_track(r);

	bool clear_mb = false;

	// album name and tracknum are allowed to be blank
	switch (c) {
		case 0:
			if (!logTable->item(r, c)->text().isEmpty())
			{
				tmp.artist = logTable->item(r, c)->text().toUtf8().data();
				clear_mb = true;
			}
			break;
		case 1:
			tmp.album = logTable->item(r, c)->text().toUtf8().data();
			clear_mb = true;
			break;
		case 2:
			if (!logTable->item(r, c)->text().isEmpty())
			{
				tmp.title = logTable->item(r, c)->text().toUtf8().data();
				clear_mb = true;
			}
			break;
		case 3:  // only allow numbers - check each digit...
			{
				QString tracknum = logTable->item(r, c)->text();
				bool only_num = true;
				for (int i = 0; i < tracknum.size(); i++)
				{
					if (!tracknum[i].isDigit())
						only_num = false;
				}
				if (only_num)
					tmp.tracknum = tracknum.toInt();
			}
			break;
		case 4:
			if (!logTable->item(r, c)->text().isEmpty())
				tmp.length = logTable->item(r, c)->text().toInt();
			break;
		case 5:
			if (!logTable->item(r, c)->text().isEmpty())
			{
                char rating = logTable->item(r, c)->text().toLocal8Bit().data()[0];
				if ('L' == rating || 'S' == rating)
					tmp.played = rating;
			}
			break;
		case 6:
			if (!logTable->item(r, c)->text().isEmpty())
			{
				QDateTime dt = QDateTime::fromString(logTable->item(r, c)->text(), "yyyy-MM-dd hh:mm:ss");
				dt.setTimeSpec(Qt::UTC);
				tmp.when = dt.toTime_t();
			}
			break;
	}

	/*
	  If certain track details have been changed (artist, album, title, etc)
	  then we can no longer trust the existing MB information
	  */
	if (clear_mb)
		tmp.mb_track_id = "";

	scrob->update_track(tmp, r);
	loadtable();
}

void QTScrob::scrobble() {
	if ( logTable->rowCount()==0 ) {
		QMessageBox::warning(this, QCoreApplication::applicationName(), "Nothing to submit");
		return;
	}

	// make sure the widget appears before we submit()
	QCoreApplication::flush();

	qDebug() << "Submitting data";
	lblStatus->setText(tr("Submitting data to server..."));
	btnSubmit->setEnabled(false);

    if (scrob->submit())
    {
        if (winProgress==NULL)
            winProgress = new Progress(this);
        winProgress->show();
    }
}

void QTScrob::scrobbled(bool success) {
	qDebug() << "Post submit";
	if (winProgress != NULL)
	{
		winProgress->close();
		delete winProgress;
		winProgress = NULL;
	}

	// TODO - fix this
	if(success) {
        lblStatus->setText(tr("Data submitted succesfully"));
		qDebug() << "Submission complete";
	} else {
		qDebug() << "Submission failed: " << scrob->get_error_str();

		QMessageBox::warning(this, QCoreApplication::applicationName(),
                             tr("There was a problem submitting data to the server.\n(Reason: %1)")
							 .arg( scrob->get_error_str() ));
        lblStatus->setText(tr("Submission failed"));
	}

	loadtable();
	scrob->clear_method();
	set_open_buttons();
	btnSubmit->setEnabled(true);
}

void QTScrob::set_open_buttons()
{
// don't allow more than one parser method to be opened at a time
// (unless the last method opened 0 tracks)
    bool const state(!(scrob->get_parser_busy() && scrob->get_num_tracks()));

    actOpen->setEnabled(state);
    actOpeniTunes->setEnabled(state);
    btnOpen->setEnabled(state);
    btnOpeniTunes->setEnabled(state);
#ifdef HAVE_MTP
    actOpenMTP->setEnabled(state);
    btnOpenMTP->setEnabled(state);
#endif
}

void QTScrob::settings_close()
{
	// close and delete the settings window object.
	// forces the window to reload the config changes.
	// prevents a cancelled window from showing any changed settings next time
	// it is opened
	winSettings->close();
	winSettings->disconnect();
	delete winSettings;
	winSettings = NULL;

	// reload the table to show the effect of any timezone related settings
	DT_labels();
	loadtable();
}

void QTScrob::missing_times_start(int total)
{
    progress_missing = new MissingTimeProgress(total, this);
    connect(this, SIGNAL(update_missing_times_progress(int)),
            progress_missing, SLOT(update_progress(int)));
    progress_missing->show();
}

void QTScrob::missing_times_progress(int remaining)
{
    emit update_missing_times_progress(remaining);
}

void QTScrob::missing_times_finished()
{
    progress_missing->close();
    progress_missing->disconnect();
    delete progress_missing;
    loadtable();
}

bool QTScrob::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (Qt::Key_Delete == keyEvent->key())
        {
            deleteRow();
            return true;
        }
    }

    // standard event processing
    return QObject::eventFilter(obj, event);
}
