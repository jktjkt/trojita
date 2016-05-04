/* Copyright (C) 2014 - 2015 Stephan Platz <trojita@paalsteek.de>
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

#include "Cryptography/MessageModel.h"
#include "Cryptography/MessagePart.h"
#include "Cryptography/PartReplacer.h"
#include "Common/MetaTypes.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

namespace Cryptography {

MessageModel::MessageModel(QObject *parent, const QModelIndex &message)
    : QAbstractItemModel(parent)
    , m_message(message)
    , m_rootPart(std::move(new TopLevelMessage(m_message, this)))
{
    Q_ASSERT(m_message.isValid());
    connect(m_message.model(), &QAbstractItemModel::dataChanged, this, &MessageModel::mapDataChanged);
}

void MessageModel::mapDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (!m_message.isValid()) {
        return;
    }

    Q_ASSERT(m_message.model());
    Q_ASSERT(topLeft.parent() == bottomRight.parent());

    QModelIndex root = index(0,0);
    if (!root.isValid())
        return;

    auto topLeftIt = m_map.constFind(topLeft);
    auto bottomRightIt = m_map.constFind(bottomRight);
    if (topLeftIt != m_map.constEnd() && bottomRightIt != m_map.constEnd()) {
        emit dataChanged(
                    createIndex(topLeft.row(), topLeft.column(), *topLeftIt),
                    createIndex(bottomRight.row(), bottomRight.column(), *bottomRightIt)
                    );
    }
}

QModelIndex MessageModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_ASSERT(!parent.isValid() || parent.model() == this);
    auto parentPart = translatePtr(parent);
    Q_ASSERT(parentPart);
    auto childPtr = parentPart->child(const_cast<MessageModel *>(this), row, column);
    if (!childPtr) {
        return QModelIndex();
    } else {
        return createIndex(row, column, childPtr);
    }
}

QModelIndex MessageModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();
    Q_ASSERT(child.model() == this);
    auto childPart = translatePtr(child);
    Q_ASSERT(childPart);
    auto parentPart = childPart->parent();
    return (parentPart && parentPart != m_rootPart.get()) ? createIndex(parentPart->row(), 0, parentPart) : QModelIndex();
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    Q_ASSERT(index.model() == this);

    return translatePtr(index)->data(role);
}

QModelIndex MessageModel::message() const
{
     return m_message;
}

void MessageModel::insertSubtree(const QModelIndex &parent, MessagePart::Ptr tree)
{
    std::vector<MessagePart::Ptr> parts;
    parts.emplace_back(std::move(tree));
    insertSubtree(parent, std::move(parts));
}

void MessageModel::insertSubtree(const QModelIndex &parent, std::vector<Cryptography::MessagePart::Ptr> &&parts)
{
    Q_ASSERT(!parent.isValid() || parent.model() == this);
    auto *part = translatePtr(parent);
    Q_ASSERT(part);

#ifdef MIME_TREE_DEBUG
    qDebug() << "Whole tree:\n" << *m_rootPart;
    qDebug() << "Inserting at:\n" << (void*)(part);
    qDebug() << "New items:\n" << *(tree.get());
#endif

    beginInsertRows(parent, 0, parts.size());

    Q_ASSERT(part->m_children.empty());
    for (size_t i = 0; i < parts.size(); ++i) {
        parts[i]->m_parent = part;
        part->m_children[i] = std::move(parts[i]);
    }

    bool needsDataChanged = false;
    if (LocalMessagePart *localPart = dynamic_cast<LocalMessagePart*>(part)) {
        localPart->m_localState = MessagePart::FetchingState::DONE;
        needsDataChanged = true;
    }

    endInsertRows();

    if (needsDataChanged) {
        emit dataChanged(parent, parent);
    }
}

void MessageModel::replaceMeWithSubtree(const QModelIndex &parent, MessagePart *partToReplace, MessagePart::Ptr tree)
{
    Q_ASSERT(!parent.isValid() || parent.model() == this);
    auto *part = translatePtr(parent);
    Q_ASSERT(part);

#ifdef MIME_TREE_DEBUG
    qDebug() << "Whole tree:\n" << *m_rootPart;
    qDebug() << "Replacing:\n" << (void*)(partToReplace);
    qDebug() << "New items:\n" << *(tree.get());
#endif

    Q_ASSERT(partToReplace);
    Q_ASSERT(tree->row() == partToReplace->row());

    auto it = part->m_children.find(partToReplace->row());
    Q_ASSERT(it != part->m_children.end());
    Q_ASSERT(it->second->row() == partToReplace->row());
    Q_ASSERT(partToReplace->row() == tree->row());

    emit layoutAboutToBeChanged(QList<QPersistentModelIndex>() << parent);

    auto oldIndexes = persistentIndexList();
    auto newIndexes = oldIndexes;
    for (int i = 0; i < oldIndexes.size(); ++i) {
        const auto &index = oldIndexes[i];
        if (index.parent() == parent && index.column() == 0 && index.row() == tree->row()) {
            newIndexes[i] = createIndex(tree->row(), 0, tree.get());
        }
    }
    changePersistentIndexList(oldIndexes, newIndexes);

    tree->m_parent = part;
    MessagePart::Ptr partGoingAway = std::move(it->second);
    it->second = std::move(tree);
    emit layoutChanged(QList<QPersistentModelIndex>() << parent);

    emit dataChanged(parent, parent);

#ifdef MIME_TREE_DEBUG
    qDebug() << "After replacement:\n" << *m_rootPart;
#endif
}

/** @short Get the underlying MessagePart * for the passed index */
MessagePart *MessageModel::translatePtr(const QModelIndex &part) const
{
    if (!part.isValid())
        return m_rootPart.get();
    return static_cast<MessagePart *>(part.internalPointer());
}

int MessageModel::rowCount(const QModelIndex &parent) const
{
    return translatePtr(parent)->rowCount(const_cast<MessageModel *>(this));
}

int MessageModel::columnCount(const QModelIndex &parent) const
{
    return translatePtr(parent)->columnCount(const_cast<MessageModel *>(this));
}

void MessageModel::registerPartHandler(std::shared_ptr<PartReplacer> module)
{
    m_partHandlers.push_back(module);
}

#ifdef MIME_TREE_DEBUG
void MessageModel::debugDump()
{
    qDebug() << "MIME tree:\n" << *m_rootPart;
}
#endif

}
