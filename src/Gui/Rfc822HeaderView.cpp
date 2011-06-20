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
#include "Rfc822HeaderView.h"

#include <QModelIndex>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

namespace Gui {

Rfc822HeaderView::Rfc822HeaderView(QWidget* parent, QModelIndex index_):
    QLabel(parent)
{
    Q_ASSERT(index_.isValid());

    // We have to obtain the underlying index
    const Imap::Mailbox::Model *constModel;
    Imap::Mailbox::TreeItemPart *part = dynamic_cast<Imap::Mailbox::TreeItemPart*>(Imap::Mailbox::Model::realTreeItem(index_, &constModel, &index_));
    Q_ASSERT(part);
    index = index_;

    Imap::Mailbox::Model *model = const_cast<Imap::Mailbox::Model*>(constModel);  // the const_cast is required because QModelIndex::model() returns const
    part->fetch(model);
    if (part->fetched()) {
        setCorrectText();
    } else if (part->isUnavailable(model)) {
        setText(tr("Offline"));
    } else {
        setText(tr("Loading..."));
        connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));
    }
    setTextInteractionFlags( Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse );
}

void Rfc822HeaderView::handleDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    if (!topLeft.isValid()) {
        // For example when reloading a top-level mailbox -> do nothing...
        return;
    }
    if (!index.isValid()) {
        // Our message is gone, so there's no point in checking further
        disconnect(sender(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));
        if (text() == tr("Loading...") || text() == tr("Offline")) {
            setCorrectText();
        }
        return;
    }

    Q_UNUSED(bottomRight);
    // FIXME: verify that th dataChanged() is emitted separately for each message
    Q_ASSERT(topLeft.model() == index.model());
    if (topLeft == index)
        setCorrectText();
}

void Rfc822HeaderView::setCorrectText()
{
    setText(index.isValid() ? index.data(Imap::Mailbox::RolePartData).toString() : tr("<i>Message deleted</i>"));
}

}


