/***************************************************************************
 *   Copyright (C) 2009 Robert Keevil                                      *
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

#include "console.h"
#include "qtscrob.h"

Console::Console(QTScrob *parent) : QDialog(parent) {
	qtscrob = parent;
	this->setWindowTitle(tr("Console"));
	resize(QSize(800, 600));

	log_pos = 0;

	edtConsole = new QTextEdit();
	edtConsole->setReadOnly(true);

	lblCombo = new QLabel(tr("Log Level:"));

	comboLevel = new QComboBox();

	for (int i = 0; i < LOG_NUM_LEVELS; i++)
	comboLevel->insertItem(i, QString("%1 - %2")
			.arg(i).arg(LOG_LEVEL_NAMES[i]));

	comboLevel->setEditable(false);

	connect(comboLevel, SIGNAL(currentIndexChanged(int)),
		this, SLOT(set_spin_level(int)));

	current_level = LOG_DEFAULT;
	comboLevel->setCurrentIndex(current_level);

	timerConsole = new QTimer();
	connect(timerConsole, SIGNAL(timeout()), this, SLOT(update()));
	timerConsole->start(250);

	btncopy = new QPushButton(tr("Copy to clipboard"));
	connect(btncopy, SIGNAL(released()), this, SLOT(copy()));

	QHBoxLayout *btmlayout = new QHBoxLayout;
	btmlayout->addWidget(lblCombo);
	btmlayout->addWidget(comboLevel);
	btmlayout->addStretch(0);
	btmlayout->addWidget(btncopy);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->setMargin(0);
	layout->addWidget(edtConsole);
	layout->addLayout(btmlayout);

	setLayout(layout);

	read_all_logs();
}

Console::~Console()
{
	timerConsole->stop();
	timerConsole->deleteLater();
}

void Console::update()
{
    int new_pos = qtscrob->scrob->get_num_logs();
	if (new_pos > log_pos)
	{
		read_logs(log_pos, new_pos);
	}
	else if (new_pos < log_pos)
	{
		// The log has been cleared?
		read_all_logs();
	}
}

void Console::set_spin_level(int new_level)
{
	current_level = new_level;
	read_all_logs();
}

void Console::read_all_logs()
{
	edtConsole->clear();
	read_logs(0, qtscrob->scrob->get_num_logs());
}

void Console::read_logs(int start, int end)
{
    for (int i = start; i < end; i++)
	{
		log_entry tmp = qtscrob->scrob->get_log(i);
		if (tmp.level <= current_level)
		{
			QString line = QString("%0 : %1")
				.arg(LOG_LEVEL_NAMES[tmp.level], tmp.msg);
			edtConsole->append(line);
		}
	}
	log_pos = end;
}

void Console::copy()
{
	if (!edtConsole->document()->isEmpty())
		QApplication::clipboard()->setText(edtConsole->document()->toPlainText());
}
