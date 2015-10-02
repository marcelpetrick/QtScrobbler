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

#ifndef CONSOLE_H
#define CONSOLE_H

#include <QTimer>
#include <QApplication>
#include <QDialog>
#include <QTextEdit>
#include <QComboBox>
#include <QLabel>

class QTScrob;
class QTimer;

class Console : public QDialog
{
	Q_OBJECT
private:
	QTScrob *qtscrob;
	QTextEdit *edtConsole;
	QTimer *timerConsole;
	QComboBox *comboLevel;
	QLabel *lblCombo;
	QPushButton *btncopy;
	int log_pos;
	int current_level;
	void read_all_logs();
	void read_logs(int, int);

private slots:
	void update();
	void set_spin_level(int);
	void copy();

public:
	Console(QTScrob *parent = 0);
	~Console();
};

#endif
