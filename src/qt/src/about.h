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

#ifndef ABOUT_H
#define ABOUT_H

#include "ui_aboutWin.h"
#include <QApplication>
#include <QtGui>

class QTScrob;

class About : public QDialog, private Ui::aboutWin {
		Q_OBJECT
	private:
		QTScrob *qtscrob;

	public:
		About(QTScrob *parent = 0);

};

#endif
