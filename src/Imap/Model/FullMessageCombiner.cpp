/* Copyright (C) 2013  Ahmed Ibrahim Khalil <ahmedibrahimkhali@gmail.com>
   Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

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
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

namespace Imap
{
namespace Mailbox
{


FullMessageCombiner::FullMessageCombiner(const QModelIndex &messageIndex, QObject *parent) :
    QObject(parent), m_messageIndex(messageIndex)
{
    Q_ASSERT(m_messageIndex.isValid());
    m_headerPartIndex = m_messageIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_HEADER);
    Q_ASSERT(m_headerPartIndex.isValid());
    m_bodyPartIndex = m_messageIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_TEXT);
    Q_ASSERT(m_bodyPartIndex.isValid());
    m_dataChanged = connect(m_messageIndex.model(), &QAbstractItemModel::dataChanged, this, &FullMessageCombiner::slotDataChanged);
}

QByteArray FullMessageCombiner::data() const
{
    if (loaded())
        return m_headerPartIndex.data(Imap::Mailbox::RolePartData).toByteArray() +
                m_bodyPartIndex.data(Imap::Mailbox::RolePartData).toByteArray();

    return QByteArray();
}

bool FullMessageCombiner::loaded() const
{
    if (!indexesValid())
        return false;

    return m_headerPartIndex.data(Imap::Mailbox::RoleIsFetched).toBool() && m_bodyPartIndex.data(Imap::Mailbox::RoleIsFetched).toBool();
}

void FullMessageCombiner::load()
{
    if (!indexesValid())
        return;

    m_headerPartIndex.data(Imap::Mailbox::RolePartData);
    m_bodyPartIndex.data(Imap::Mailbox::RolePartData);

    slotDataChanged(QModelIndex(), QModelIndex());
}

void FullMessageCombiner::slotDataChanged(const QModelIndex &left, const QModelIndex &right)
{
    Q_UNUSED(left);
    Q_UNUSED(right);

    if (!indexesValid()) {
        emit failed(tr("Message is gone"));
        return;
    }

    if (m_headerPartIndex.data(Imap::Mailbox::RoleIsFetched).toBool() && m_bodyPartIndex.data(Imap::Mailbox::RoleIsFetched).toBool()) {
        emit completed();
        disconnect(m_dataChanged);
    }

    bool headerOffline = m_headerPartIndex.data(Imap::Mailbox::RoleIsUnavailable).toBool();
    bool bodyOffline = m_bodyPartIndex.data(Imap::Mailbox::RoleIsUnavailable).toBool();
    if (headerOffline && bodyOffline) {
        emit failed(tr("Offline mode: uncached message data not available"));
    } else if (headerOffline) {
        emit failed(tr("Offline mode: uncached header data not available"));
    } else if (bodyOffline) {
        emit failed(tr("Offline mode: uncached body data not available"));
    }
}

bool FullMessageCombiner::indexesValid() const
{
    return m_messageIndex.isValid() && m_headerPartIndex.isValid() && m_bodyPartIndex.isValid();
}

}
}
