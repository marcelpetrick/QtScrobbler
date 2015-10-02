/***************************************************************************
 *   Copyright (C) 2010 Robert Keevil                                      *
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
#include "missingtimeprogress.h"

MissingTimeProgress::MissingTimeProgress(int count, QWidget *parent) : QDialog( parent )
{
    progress = new QProgressBar(this);
    lblprogress = new QLabel(this);
    progress->setMinimumWidth(200);
    progress->setMaximumWidth(200);

    progress->setMaximum(count);

    lblprogress->setText(tr("Fetching missing track lengths"));
    this->setWindowTitle(tr("Please wait"));

    layout = new QVBoxLayout(this);
    layout->addWidget(lblprogress);
    layout->addWidget(progress);

    setLayout(layout);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog);
    setFixedSize(sizeHint());
}

MissingTimeProgress::~MissingTimeProgress()
{
}

void MissingTimeProgress::update_progress(int remaining)
{
    progress->setValue(progress->maximum() - remaining);
}
