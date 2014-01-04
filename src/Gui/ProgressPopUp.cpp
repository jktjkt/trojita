/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ProgressPopUp.h"
#include "ui_ProgressPopUp.h"

namespace Gui {

ProgressPopUp::ProgressPopUp(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::ProgressPopUp)
{
    ui->setupUi(this);
}

ProgressPopUp::~ProgressPopUp()
{
    delete ui;
}

void ProgressPopUp::changeEvent(QEvent *e)
{
    QFrame::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void ProgressPopUp::setMinimum(const int minimum)
{
    ui->progressBar->setMinimum(minimum);
}

void ProgressPopUp::setMaximum(const int maximum)
{
    ui->progressBar->setMaximum(maximum);
}

void ProgressPopUp::setValue(const int value)
{
    ui->progressBar->setValue(value);
}

void ProgressPopUp::setLabelText(const QString &text)
{
    ui->label->setText(text);
}

}
