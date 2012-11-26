/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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
#include "LoadablePartWidget.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Utils.h"

#include <QPushButton>

namespace Gui
{

LoadablePartWidget::LoadablePartWidget(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex  &part,
                                       QObject *wheelEventFilter, QObject *contextMenuTarget):
    QStackedWidget(parent), manager(manager), partIndex(part), realPart(0), wheelEventFilter(wheelEventFilter),
    contextMenuTarget(contextMenuTarget)
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
    connect(realPart, SIGNAL(customContextMenuRequested(QPoint)), contextMenuTarget, SLOT(partContextMenuRequested(QPoint)));
    addWidget(realPart);
    setCurrentIndex(1);
}

QString LoadablePartWidget::quoteMe() const
{
    return realPart ? realPart->quoteMe() : QString();
}

}


