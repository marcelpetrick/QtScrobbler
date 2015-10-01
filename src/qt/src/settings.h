/***************************************************************************
 *   Copyright (C) 2006-2008 by Tomasz Moń                                 *
 *   Copyright (C) 2009-2010 by Robert Keevil                              *
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

#ifndef SETTINGS_H
#define SETTINGS_H

#include "ui_settingsWin.h"
#include <QApplication>
#include <QtGui>

class QTScrob;

class Site_UI : public QWidget
{
	Q_OBJECT
public:
	Site_UI(QWidget *parent = NULL);
	QFormLayout *layout;
	QCheckBox *enabled;
	QLineEdit *host;
	QLineEdit *name;
	QLineEdit *username;
	QLineEdit *password;
};

class Settings : public QDialog, private Ui::settingsWin {
		Q_OBJECT
	private:
		QTScrob *qtscrob;
		QList<Site_UI *> site_tabs;

	private slots:
		void save();

	public:
		Settings(QTScrob *parent = 0);
};

#endif
