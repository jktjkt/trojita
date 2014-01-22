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

#include "MailboxModel.h"
#include "MailboxTree.h"
#include "ItemRoles.h"

#include <QDebug>
#include <QMimeData>

namespace Imap
{
namespace Mailbox
{

MailboxModel::MailboxModel(QObject *parent, Model *model): QAbstractProxyModel(parent)
{
    setSourceModel(model);

    // FIXME: will need to be expanded when Model supports more signals...
    connect(model, SIGNAL(modelAboutToBeReset()), this, SLOT(handleModelAboutToBeReset()));
    connect(model, SIGNAL(modelReset()), this, SLOT(handleModelReset()));
    connect(model, SIGNAL(layoutAboutToBeChanged()), this, SIGNAL(layoutAboutToBeChanged()));
    connect(model, SIGNAL(layoutChanged()), this, SIGNAL(layoutChanged()));
    connect(model, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)),
            this, SLOT(handleDataChanged(const QModelIndex &, const QModelIndex &)));
    connect(model, SIGNAL(rowsAboutToBeRemoved(const QModelIndex &, int, int)),
            this, SLOT(handleRowsAboutToBeRemoved(const QModelIndex &, int, int)));
    connect(model, SIGNAL(rowsRemoved(const QModelIndex &, int, int)),
            this, SLOT(handleRowsRemoved(const QModelIndex &, int, int)));
    connect(model, SIGNAL(rowsAboutToBeInserted(const QModelIndex &, int, int)),
            this, SLOT(handleRowsAboutToBeInserted(const QModelIndex &, int, int)));
    connect(model, SIGNAL(rowsInserted(const QModelIndex &, int, int)),
            this, SLOT(handleRowsInserted(const QModelIndex &, int, int)));
    connect(model, SIGNAL(messageCountPossiblyChanged(const QModelIndex &)),
            this, SLOT(handleMessageCountPossiblyChanged(const QModelIndex &)));

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    // There's no virtual in Qt4.
    setRoleNames(trojitaProxyRoleNames());
#else
    // In Qt5, the roleNames() is virtual and will work just fine.
#endif
}

// The following code is pretty much a huge PITA. The handling of roleNames() has changed between Qt4 and Qt5 in a way which makes
// it rather convoluted to support both in the same code base. Oh well.
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
QHash<int, QByteArray> MailboxModel::trojitaProxyRoleNames() const
#else
QHash<int, QByteArray> MailboxModel::roleNames() const
#endif
{
    static QHash<int, QByteArray> roleNames;
    if (roleNames.isEmpty()) {
        roleNames[RoleIsFetched] = "isFetched";
        roleNames[RoleShortMailboxName] = "shortMailboxName";
        roleNames[RoleMailboxName] = "mailboxName";
        roleNames[RoleMailboxSeparator] = "mailboxSeparator";
        roleNames[RoleMailboxHasChildMailboxes] = "mailboxHasChildMailboxes";
        roleNames[RoleMailboxIsINBOX] = "mailboxIsINBOX";
        roleNames[RoleMailboxIsSelectable] = "mailboxIsSelectable";
        roleNames[RoleMailboxNumbersFetched] = "mailboxNumbersFetched";
        roleNames[RoleTotalMessageCount] = "totalMessageCount";
        roleNames[RoleUnreadMessageCount] = "unreadMessageCount";
        roleNames[RoleRecentMessageCount] = "recentMessageCount";
        roleNames[RoleMailboxItemsAreLoading] = "mailboxItemsAreLoading";
    }
    return roleNames;
}

void MailboxModel::handleModelAboutToBeReset()
{
    beginResetModel();
}

void MailboxModel::handleModelReset()
{
    endResetModel();
}

bool MailboxModel::hasChildren(const QModelIndex &parent) const
{
    if (parent.isValid() && parent.column() != 0)
        return false;

    QModelIndex index = mapToSource(parent);

    TreeItemMailbox *mbox = dynamic_cast<TreeItemMailbox *>(
                                static_cast<TreeItem *>(
                                    index.internalPointer()
                                ));
    return mbox ?
           mbox->hasChildMailboxes(static_cast<Model *>(sourceModel())) :
           sourceModel()->hasChildren(index);
}

void MailboxModel::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    QModelIndex first = mapFromSource(topLeft);
    QModelIndex second = mapFromSource(bottomRight);

    if (! first.isValid() || ! second.isValid()) {
        // It's something completely alien...
        return;
    }

    if (first.parent() == second.parent() && first.column() == second.column()) {
        emit dataChanged(first, second);
    } else {
        // FIXME: batched updates aren't used yet
        Q_ASSERT(false);
        return;
    }
}

QModelIndex MailboxModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column != 0)
        return QModelIndex();

    if (parent.column() != 0 && parent.column() != -1)
        return QModelIndex();

    QModelIndex translatedParent = mapToSource(parent);

    if (row < sourceModel()->rowCount(translatedParent) - 1) {
        void *ptr = sourceModel()->index(row + 1, 0, translatedParent).internalPointer();
        Q_ASSERT(ptr);
        return createIndex(row, column, ptr);
    } else {
        return QModelIndex();
    }
}

QModelIndex MailboxModel::parent(const QModelIndex &index) const
{
    return mapFromSource(mapToSource(index).parent());
}

int MailboxModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() != 0 && parent.column() != -1)
        return 0;
    int res = sourceModel()->rowCount(mapToSource(parent));
    if (res > 0)
        --res;
    return res;
}

int MailboxModel::columnCount(const QModelIndex &parent) const
{
    return parent.column() == 0 || parent.column() == -1 ? 1 : 0;
}

QModelIndex MailboxModel::mapToSource(const QModelIndex &proxyIndex) const
{
    int row = proxyIndex.row();
    if (row < 0 || proxyIndex.column() != 0)
        return QModelIndex();
    ++row;
    return static_cast<Imap::Mailbox::Model *>(sourceModel())->createIndex(row, 0, proxyIndex.internalPointer());
}

QModelIndex MailboxModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid())
        return QModelIndex();

    if (! dynamic_cast<Imap::Mailbox::TreeItemMailbox *>(
            static_cast<Imap::Mailbox::TreeItem *>(sourceIndex.internalPointer())))
        return QModelIndex();

    int row = sourceIndex.row();
    if (row == 0)
        return QModelIndex();
    if (row > 0)
        --row;
    if (sourceIndex.column() != 0)
        return QModelIndex();

    return createIndex(row, 0, sourceIndex.internalPointer());
}

QVariant MailboxModel::data(const QModelIndex &proxyIndex, int role) const
{
    if (! proxyIndex.isValid() || proxyIndex.model() != this)
        return QVariant();

    if (proxyIndex.column() != 0)
        return QVariant();

    TreeItemMailbox *mbox = dynamic_cast<TreeItemMailbox *>(
                                static_cast<TreeItem *>(proxyIndex.internalPointer())
                            );
    Q_ASSERT(mbox);
    if (role > RoleBase && role < RoleInvalidLastOne)
        return mbox->data(static_cast<Imap::Mailbox::Model *>(sourceModel()), role);
    else
        return QAbstractProxyModel::data(createIndex(proxyIndex.row(), 0, proxyIndex.internalPointer()), role);
}

void MailboxModel::handleMessageCountPossiblyChanged(const QModelIndex &mailbox)
{
    QModelIndex translated = mapFromSource(mailbox);
    if (translated.isValid()) {
        emit dataChanged(translated, translated);
    }
}

Qt::ItemFlags MailboxModel::flags(const QModelIndex &index) const
{
    if (! index.isValid())
        return QAbstractProxyModel::flags(index);

    TreeItemMailbox *mbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(index.internalPointer()));
    Q_ASSERT(mbox);

    Qt::ItemFlags res = QAbstractProxyModel::flags(index);
    if (!mbox->isSelectable()) {
        res &= ~Qt::ItemIsSelectable;
        res |= Qt::ItemIsEnabled;
    }
    if (static_cast<Model *>(sourceModel())->isNetworkAvailable()) {
        res |= Qt::ItemIsDropEnabled;
    }
    return res;
}

Qt::DropActions MailboxModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList MailboxModel::mimeTypes() const
{
    return QStringList() << QLatin1String("application/x-trojita-message-list");
}

bool MailboxModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(row); Q_UNUSED(column);
    if (action != Qt::CopyAction && action != Qt::MoveAction)
        return false;

    if (! parent.isValid())
        return false;

    if (! static_cast<Model *>(sourceModel())->isNetworkAvailable())
        return false;

    TreeItemMailbox *target = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(parent.internalPointer()));
    Q_ASSERT(target);

    if (! target->isSelectable())
        return false;

    QByteArray encodedData = data->data("application/x-trojita-message-list");
    QDataStream stream(&encodedData, QIODevice::ReadOnly);

    Q_ASSERT(! stream.atEnd());
    QString origMboxName;
    stream >> origMboxName;
    TreeItemMailbox *origMbox = static_cast<Model *>(sourceModel())->findMailboxByName(origMboxName);
    if (! origMbox) {
        qDebug() << "Can't find original mailbox when performing a drag&drop on messages";
        return false;
    }

    uint uidValidity;
    stream >> uidValidity;
    if (uidValidity != origMbox->syncState.uidValidity()) {
        qDebug() << "UID validity for original mailbox got changed, can't copy messages";
        return false;
    }

    QList<uint> uids;
    stream >> uids;

    static_cast<Model *>(sourceModel())->copyMoveMessages(origMbox, target->mailbox(), uids,
            (action == Qt::MoveAction) ? MOVE : COPY);
    return true;
}

void MailboxModel::handleRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    TreeItemMailbox *parentMbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(parent.internalPointer()));
    if (parent.internalPointer() && ! parentMbox)
        return;
    if (! parentMbox)
        parentMbox = static_cast<Imap::Mailbox::Model *>(sourceModel())->m_mailboxes;
    Q_ASSERT(first >= 1);
    Q_ASSERT(last <= parentMbox->m_children.size() - 1);
    Q_ASSERT(first <= last);
    beginRemoveRows(mapFromSource(parent), first - 1, last - 1);
}

void MailboxModel::handleRowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(first);
    Q_UNUSED(last);
    TreeItemMailbox *parentMbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(parent.internalPointer()));
    if (parent.internalPointer() && ! parentMbox)
        return;
    endRemoveRows();
}

void MailboxModel::handleRowsAboutToBeInserted(const QModelIndex &parent, int first, int last)
{
    if (parent.internalPointer() && ! dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(parent.internalPointer())))
        return;
    if (first == 0 && last == 0)
        return;
    beginInsertRows(mapFromSource(parent), first - 1, last - 1);
}

void MailboxModel::handleRowsInserted(const QModelIndex &parent, int first, int last)
{
    if (parent.internalPointer() && ! dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(parent.internalPointer())))
        return;
    if (first == 0 && last == 0)
        return;
    endInsertRows();
}


}
}
