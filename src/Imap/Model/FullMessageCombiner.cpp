/* Copyright (C) 2013  Ahmed Ibrahim Khalil <ahmedibrahimkhali@gmail.com>
   Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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

#include "FullMessageCombiner.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"

namespace Imap
{
namespace Mailbox
{


FullMessageCombiner::FullMessageCombiner(const QModelIndex &messageIndex, QObject *parent) :
    QObject(parent), m_model(0), m_messageIndex(messageIndex)
{
    Imap::Mailbox::Model::realTreeItem(messageIndex, &m_model);
    Q_ASSERT(m_model);
    Imap::Mailbox::TreeItemPart *headerPart = headerPartPtr();
    Imap::Mailbox::TreeItemPart *bodyPart = bodyPartPtr();

    Q_ASSERT(headerPart);
    Q_ASSERT(bodyPart);

    m_headerPartIndex = headerPart->toIndex(const_cast<Mailbox::Model *>(m_model));
    Q_ASSERT(m_headerPartIndex.isValid());

    m_bodyPartIndex = bodyPart->toIndex(const_cast<Mailbox::Model *>(m_model));
    Q_ASSERT(m_bodyPartIndex.isValid());

    connect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(slotDataChanged(QModelIndex,QModelIndex)));
}

QByteArray FullMessageCombiner::data() const
{
    if (loaded())
        return *(headerPartPtr()->dataPtr()) + *(bodyPartPtr()->dataPtr());

    return QByteArray();
}

bool FullMessageCombiner::loaded() const
{
    return headerPartPtr()->fetched() && bodyPartPtr()->fetched();
}

void FullMessageCombiner::load()
{
    Imap::Mailbox::TreeItemPart *headerPart = headerPartPtr();
    headerPart->fetch(const_cast<Mailbox::Model *>(m_model));
    Imap::Mailbox::TreeItemPart *bodyPart = bodyPartPtr();
    bodyPart->fetch(const_cast<Mailbox::Model *>(m_model));
    slotDataChanged(QModelIndex(), QModelIndex());
}

TreeItemPart *FullMessageCombiner::headerPartPtr() const
{
    Imap::Mailbox::TreeItem *target = m_model->realTreeItem(m_messageIndex);
    return dynamic_cast<Imap::Mailbox::TreeItemPart *>(target->specialColumnPtr(0, Imap::Mailbox::TreeItem::OFFSET_HEADER));
}

TreeItemPart *FullMessageCombiner::bodyPartPtr() const
{
    Imap::Mailbox::TreeItem *target = m_model->realTreeItem(m_messageIndex);
    return dynamic_cast<Imap::Mailbox::TreeItemPart *>(target->specialColumnPtr(0, Imap::Mailbox::TreeItem::OFFSET_TEXT));
}

void FullMessageCombiner::slotDataChanged(const QModelIndex &left, const QModelIndex &right)
{
    Q_UNUSED(left);
    Q_UNUSED(right);

    if (headerPartPtr()->fetched() && bodyPartPtr()->fetched()) {
       emit completed();
       // Disconnect this slot from its connected signal to prevent emitting completed() many times
       // when dataChanged() is emitted and the parts are already fetched.
       disconnect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(slotDataChanged(QModelIndex,QModelIndex)));
    }

    Imap::Mailbox::Model *model = const_cast<Imap::Mailbox::Model*>(m_model);
    bool headerOffline = headerPartPtr()->isUnavailable(model);
    bool bodyOffline = bodyPartPtr()->isUnavailable(model);
    if (headerOffline && bodyOffline) {
        emit failed(tr("Offline mode: uncached message data not available"));
    } else if (headerOffline) {
        emit failed(tr("Offline mode: uncached header data not available"));
    } else if (bodyOffline) {
        emit failed(tr("Offline mode: uncached body data not available"));
    }
}

}
}
