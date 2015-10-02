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

#include "help.h"
#include "qtscrob.h"

Help::Help(QTScrob *parent) : QDialog( parent ) {
	qtscrob = parent;
	this->setWindowTitle(tr("QTScrobbler Help"));
	resize(QSize(800, 600));

	edtHelp = new QTextEdit();
	edtHelp->setReadOnly(true);

	QFile file(":/index.html");
	if (file.open(QIODevice::ReadOnly))
		edtHelp->setHtml(file.readAll());

	QVBoxLayout *layout = new QVBoxLayout;
	layout->setMargin(0);
	layout->addWidget(edtHelp);

	setLayout(layout);

}

