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

#include "OverlayWidget.h"
#include <QResizeEvent>
#include <QHBoxLayout>

#include <QDebug>

namespace Gui
{

OverlayWidget::OverlayWidget(QWidget *what, QWidget *parent): QWidget(parent), m_widget(what)
{
    m_widget->setParent(this);
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(m_widget);
    layout->addStretch(1);
    parent->installEventFilter(this);
    resize(parent->size());
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

bool OverlayWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != parent())
        return false;

    if (event->type() == QEvent::Resize) {
        QResizeEvent *e = static_cast<QResizeEvent*>(event);
        resize(e->size());
    }
    return false;
}

}

