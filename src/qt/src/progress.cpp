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

#include "progress.h"
#include "qtscrob.h"

Progress_UI::Progress_UI(QWidget *parent): QWidget(parent)
{
    layout = new QFormLayout(this);
    progress = new QProgressBar(this);
    name = new QLabel(this);
    progress->setMinimumWidth(200);
    progress->setMaximumWidth(200);
    layout->addRow(name, progress);
}

Progress::Progress(QTScrob *parent) : QDialog( parent ) {
    this->setWindowTitle(tr("Submission Progress"));

    qtscrob = parent;
    layout = new QVBoxLayout(this);
    btn_cancel = new QPushButton(tr("Cancel"), this);

    connect(btn_cancel, SIGNAL(clicked()), this, SLOT(cancel()));

    QHashIterator<int, Submit *> i(qtscrob->scrob->submissions);
    while (i.hasNext())
    {
        i.next();
        int index = i.value()->index;
        Progress_UI *prog = new Progress_UI(this);
        prog->name->setText(SITE_NAME[index]);
        layout->addWidget(prog);
        layout->setAlignment(prog, Qt::AlignRight);
        layout->addSpacing(10);
        sites.insert(index, prog);

        connect(i.value(),
                SIGNAL(progress(int, int, int)),
                this,
                SLOT(update_progress(int, int, int)));
    }

    layout->addWidget(btn_cancel);
    setLayout(layout);
}

Progress::~Progress()
{
    disconnect(btn_cancel, SIGNAL(clicked()), this, SLOT(cancel()));
}

void Progress::cancel(void)
{
    this->hide();
    qtscrob->scrob->cancel_submission();
}

void Progress::update_progress(int index, int batch_current, int batch_total)
{
    Progress_UI *tmp = sites.value(index);

    tmp->progress->setMaximum(batch_total);
    tmp->progress->setValue(batch_current);
}
