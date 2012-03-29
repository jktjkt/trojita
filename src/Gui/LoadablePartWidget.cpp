/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#include "LoadablePartWidget.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Utils.h"

#include <QPushButton>

namespace Gui
{

LoadablePartWidget::LoadablePartWidget(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex  &part,
                                       QObject *wheelEventFilter):
    QStackedWidget(parent), manager(manager), partIndex(part), realPart(0), wheelEventFilter(wheelEventFilter)
{
    Q_ASSERT(partIndex.isValid());
    loadButton = new QPushButton(tr("Load %1 (%2)").arg(partIndex.data(Imap::Mailbox::RolePartMimeType).toString(),
                                 Imap::Mailbox::PrettySize::prettySize(partIndex.data(Imap::Mailbox::RolePartOctets).toUInt())),
                                 this);
    connect(loadButton, SIGNAL(clicked()), this, SLOT(loadClicked()));
    addWidget(loadButton);
}

void LoadablePartWidget::loadClicked()
{
    if (!partIndex.isValid()) {
        loadButton->setEnabled(false);
        return;
    }
    realPart = new SimplePartWidget(this, manager, partIndex);
    realPart->installEventFilter(wheelEventFilter);
    addWidget(realPart);
    setCurrentIndex(1);
}

QString LoadablePartWidget::quoteMe() const
{
    return realPart ? realPart->quoteMe() : QString();
}

}


