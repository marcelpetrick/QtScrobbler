/***************************************************************************
 *   Copyright (C) 2008 Robert Keevil                                      *
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

#ifndef PROGRESS_H
#define PROGRESS_H

#include <QApplication>
#include <QtGui>

class QTScrob;

class Progress_UI : public QWidget
{
    Q_OBJECT
public:
    Progress_UI(QWidget *parent = NULL);
    QFormLayout *layout;
    QProgressBar *progress;
    QLabel *name;
};

class Progress : public QDialog {
    Q_OBJECT
private:
    QTScrob *qtscrob;
    QVBoxLayout *layout;
    QPushButton *btn_cancel;
    QHash<int, Progress_UI *> sites;

public slots:
    void cancel();
    void update_progress(int, int, int);

public:
    Progress(QTScrob *parent = 0);
    ~Progress();
};

#endif
