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

#include "SubtreeModel.h"

namespace Imap
{
namespace Mailbox
{

SubtreeModel::SubtreeModel(QObject *parent): QAbstractProxyModel(parent)
{
}

void SubtreeModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    Q_ASSERT(qobject_cast<ModelType*>(sourceModel));

    if (this->sourceModel()) {
        disconnect(this->sourceModel(), 0, this, 0);
    }
    QAbstractProxyModel::setSourceModel(sourceModel);
    if (sourceModel) {
        // FIXME: will need to be expanded when the source model supports more signals...
        connect(sourceModel, SIGNAL(modelAboutToBeReset()), this, SLOT(handleModelAboutToBeReset()));
        connect(sourceModel, SIGNAL(modelReset()), this, SLOT(handleModelReset()));
        connect(sourceModel, SIGNAL(layoutAboutToBeChanged()), this, SIGNAL(layoutAboutToBeChanged()));
        connect(sourceModel, SIGNAL(layoutChanged()), this, SIGNAL(layoutChanged()));
        connect(sourceModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));
        connect(sourceModel, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)), this, SLOT(handleRowsAboutToBeRemoved(QModelIndex,int,int)));
        connect(sourceModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(handleRowsRemoved(QModelIndex,int,int)));
        connect(sourceModel, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)), this, SLOT(handleRowsAboutToBeInserted(QModelIndex,int,int)));
        connect(sourceModel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(handleRowsInserted(QModelIndex,int,int)));
    }
}

void SubtreeModel::setRootItem(QModelIndex rootIndex)
{
    Q_ASSERT(rootIndex.isValid());
    beginResetModel();
    setSourceModel(const_cast<QAbstractItemModel*>(rootIndex.model()));
    m_rootIndex = rootIndex;
    endResetModel();
}

void SubtreeModel::handleModelAboutToBeReset()
{
    beginResetModel();
}

void SubtreeModel::handleModelReset()
{
    endResetModel();
}

void SubtreeModel::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
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
        // FIXME: batched updates aren't supported yet
        Q_ASSERT(false);
        return;
    }
}

QModelIndex SubtreeModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid())
        return QModelIndex();

    if (!sourceModel())
        return QModelIndex();

    return qobject_cast<ModelType*>(sourceModel())->createIndex(
                proxyIndex.row(), proxyIndex.column(), proxyIndex.internalPointer());
}

QModelIndex SubtreeModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid())
        return QModelIndex();

    Q_ASSERT(sourceIndex.model() == sourceModel());

    if (!isVisibleIndex(sourceIndex))
        return QModelIndex();

    Q_ASSERT(sourceIndex.column() == 0);

    if (sourceIndex.internalPointer() == m_rootIndex.internalPointer())
        return QModelIndex();

    return createIndex(sourceIndex.row(), 0, sourceIndex.internalPointer());
}

/** @short Return true iff the passed source index is in the source model located in the current subtree */
bool SubtreeModel::isVisibleIndex(QModelIndex sourceIndex) const
{
    if (!m_rootIndex.isValid()) {
        Q_ASSERT(false);
        return false;
    }
    while (sourceIndex.isValid()) {
        if (sourceIndex == m_rootIndex)
            return true;
        sourceIndex = sourceIndex.parent();
    }
    return false;
}

QModelIndex SubtreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!sourceModel())
        return QModelIndex();

    if (row < 0 || column != 0)
        return QModelIndex();

    if (parent.isValid()) {
        return mapFromSource(mapToSource(parent).child(row, column));
    } else {
        return mapFromSource(m_rootIndex.child(row, column));
    }
}

QModelIndex SubtreeModel::parent(const QModelIndex &child) const
{
    return mapFromSource(mapToSource(child).parent());
}

int SubtreeModel::rowCount(const QModelIndex &parent) const
{
    return !sourceModel() ? 0 :
                            parent.isValid() ?
                                sourceModel()->rowCount(mapToSource(parent)) :
                                sourceModel()->rowCount(m_rootIndex);
}

int SubtreeModel::columnCount(const QModelIndex &parent) const
{
    return !sourceModel() ? 0 :
                            parent.isValid() ?
                                qMin(sourceModel()->columnCount(mapToSource(parent)), 1) :
                                qMin(sourceModel()->columnCount(m_rootIndex), 1);
}

void SubtreeModel::handleRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    if (!isVisibleIndex(parent))
        return;
    beginRemoveRows(mapFromSource(parent), first, last);
}

void SubtreeModel::handleRowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(first);
    Q_UNUSED(last);

    if (!m_rootIndex.isValid()) {
        // Everything got replaced, apparently
        reset();
        return;
    }

    if (!isVisibleIndex(parent))
        return;
    endRemoveRows();
}

void SubtreeModel::handleRowsAboutToBeInserted(const QModelIndex &parent, int first, int last)
{
    if (!isVisibleIndex(parent))
        return;
    beginInsertRows(mapFromSource(parent), first, last);
}

void SubtreeModel::handleRowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(first);
    Q_UNUSED(last);

    if (!isVisibleIndex(parent))
        return;
    endInsertRows();
}


}
}
