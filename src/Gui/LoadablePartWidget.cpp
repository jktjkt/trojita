/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

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
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Utils.h"

#include <QPushButton>

namespace Gui {

LoadablePartWidget::LoadablePartWidget( QWidget* parent,
                         Imap::Network::MsgPartNetAccessManager* _manager,
                         Imap::Mailbox::TreeItemPart* _part,
                         QObject* _wheelEventFilter ):
QStackedWidget(parent), manager(_manager), part(_part), realPart(0),
wheelEventFilter(_wheelEventFilter)
{
    loadButton = new QPushButton( tr("Load %1 (%2)").arg(
            part->mimeType(), Imap::Mailbox::PrettySize::prettySize( part->octets() ) ), this );
    connect( loadButton, SIGNAL(clicked()), this, SLOT(loadClicked()) );
    addWidget( loadButton );
}

void LoadablePartWidget::loadClicked()
{
    realPart = new SimplePartWidget( this, manager, part );
    realPart->installEventFilter( wheelEventFilter );
    addWidget( realPart );
    setCurrentIndex( 1 );
}

QString LoadablePartWidget::quoteMe() const
{
    return realPart ? realPart->quoteMe() : QString();
}

}


