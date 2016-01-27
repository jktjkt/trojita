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

#include "SubtreeModel.h"
#include "MailboxModel.h"
#include "Model.h"

namespace Imap
{
namespace Mailbox
{

class SubtreeClassAdaptor {
public:
    virtual ~SubtreeClassAdaptor() {}
    virtual QModelIndex parentCreateIndex(const QAbstractItemModel *sourceModel,
                                          const int row, const int column, void *internalPointer) const = 0;
    virtual void assertCorrectClass(const QAbstractItemModel *model) const = 0;
};


template <typename SourceModel>
class SubtreeClassSpecificItem: public SubtreeClassAdaptor {
    virtual QModelIndex parentCreateIndex(const QAbstractItemModel *sourceModel,
                                          const int row, const int column, void *internalPointer) const {
        return qobject_cast<const SourceModel*>(sourceModel)->createIndex(row, column, internalPointer);
    }
    virtual void assertCorrectClass(const QAbstractItemModel *model) const {
        Q_ASSERT(qobject_cast<const SourceModel*>(model));
    }
};


SubtreeModelOfMailboxModel::SubtreeModelOfMailboxModel(QObject *parent):
    SubtreeModel(parent, new SubtreeClassSpecificItem<MailboxModel>())
{
}

SubtreeModelOfModel::SubtreeModelOfModel(QObject *parent):
    SubtreeModel(parent, new SubtreeClassSpecificItem<Model>())
{
}

SubtreeModel::SubtreeModel(QObject *parent, SubtreeClassAdaptor *classSpecificAdaptor):
    QAbstractProxyModel(parent), m_classAdaptor(classSpecificAdaptor), m_usingInvalidRoot(false)
{
    connect(this, &QAbstractItemModel::layoutChanged, this, &SubtreeModel::validityChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &SubtreeModel::validityChanged);
    connect(this, &QAbstractItemModel::rowsInserted, this, &SubtreeModel::validityChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &SubtreeModel::validityChanged);
}

SubtreeModel::~SubtreeModel()
{
    delete m_classAdaptor;
}

void SubtreeModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    m_classAdaptor->assertCorrectClass(sourceModel);

    if (this->sourceModel()) {
        disconnect(this->sourceModel(), nullptr, this, nullptr);
    }
    QAbstractProxyModel::setSourceModel(sourceModel);
    if (sourceModel) {
        // FIXME: will need to be expanded when the source model supports more signals...
        connect(sourceModel, &QAbstractItemModel::modelAboutToBeReset, this, &SubtreeModel::handleModelAboutToBeReset);
        connect(sourceModel, &QAbstractItemModel::modelReset, this, &SubtreeModel::handleModelReset);
        connect(sourceModel, &QAbstractItemModel::layoutAboutToBeChanged, this, &QAbstractItemModel::layoutAboutToBeChanged);
        connect(sourceModel, &QAbstractItemModel::layoutChanged, this, &QAbstractItemModel::layoutChanged);
        connect(sourceModel, &QAbstractItemModel::dataChanged, this, &SubtreeModel::handleDataChanged);
        connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &SubtreeModel::handleRowsAboutToBeRemoved);
        connect(sourceModel, &QAbstractItemModel::rowsRemoved, this, &SubtreeModel::handleRowsRemoved);
        connect(sourceModel, &QAbstractItemModel::rowsAboutToBeInserted, this, &SubtreeModel::handleRowsAboutToBeInserted);
        connect(sourceModel, &QAbstractItemModel::rowsInserted, this, &SubtreeModel::handleRowsInserted);
    }
}

void SubtreeModel::setRootItem(QModelIndex rootIndex)
{
    beginResetModel();
    if (rootIndex.model() != m_rootIndex.model() && rootIndex.model()) {
        setSourceModel(const_cast<QAbstractItemModel*>(rootIndex.model()));
    }
    m_rootIndex = rootIndex;
    m_usingInvalidRoot = !m_rootIndex.isValid();
    endResetModel();
}

void SubtreeModel::setRootItemByOffset(const int row)
{
    setRootItem(mapToSource(index(row, 0)));
}

void SubtreeModel::setRootOneLevelUp()
{
    setRootItem(parentOfRoot());
}

void SubtreeModel::setOriginalRoot()
{
    setRootItem(QModelIndex());
}

QModelIndex SubtreeModel::parentOfRoot() const
{
    return m_rootIndex.parent();
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
        if (m_usingInvalidRoot) {
            emit dataChanged(QModelIndex(), QModelIndex());
        } else {
            // It's something completely alien...
        }
        return;
    }

    if (first.parent() == second.parent() && first.column() == second.column()) {
        emit dataChanged(first, second);
    } else {
        // FIXME: batched updates aren't supported yet
        Q_ASSERT(false);
    }
}

QModelIndex SubtreeModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid())
        return m_rootIndex;

    if (!sourceModel())
        return QModelIndex();

    return m_classAdaptor->parentCreateIndex(sourceModel(), proxyIndex.row(), proxyIndex.column(), proxyIndex.internalPointer());
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
    if (m_usingInvalidRoot) {
        // everything is visible in this case
        return true;
    }

    if (!m_rootIndex.isValid()) {
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
    } else if (m_rootIndex.isValid() || m_usingInvalidRoot) {
        return mapFromSource(sourceModel()->index(row, column, m_rootIndex));
    } else {
        return QModelIndex();
    }
}

QModelIndex SubtreeModel::parent(const QModelIndex &child) const
{
    return mapFromSource(mapToSource(child).parent());
}

int SubtreeModel::rowCount(const QModelIndex &parent) const
{
    if (!sourceModel())
        return 0;
    if (parent.isValid()) {
        return sourceModel()->rowCount(mapToSource(parent));
    } else if (m_rootIndex.isValid() || m_usingInvalidRoot) {
        return sourceModel()->rowCount(m_rootIndex);
    } else {
        return 0;
    }
}

int SubtreeModel::columnCount(const QModelIndex &parent) const
{
    if (!sourceModel())
        return 0;
    if (parent.isValid()) {
        return sourceModel()->columnCount(mapToSource(parent));
    } else if (m_rootIndex.isValid() || m_usingInvalidRoot) {
        return sourceModel()->columnCount(m_rootIndex);
    } else {
        return 0;
    }
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

    if (!m_usingInvalidRoot && !m_rootIndex.isValid()) {
        // This is our chance to report back about everything being deleted
        beginResetModel();
        endResetModel();
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

bool SubtreeModel::itemsValid() const
{
    return m_usingInvalidRoot || m_rootIndex.isValid();
}

QModelIndex SubtreeModel::rootIndex() const
{
    return m_rootIndex;
}

bool SubtreeModel::hasChildren(const QModelIndex &parent) const
{
    if (!sourceModel())
        return false;
    if (parent.isValid()) {
        return sourceModel()->hasChildren(mapToSource(parent));
    } else if (m_rootIndex.isValid() || m_usingInvalidRoot) {
        return sourceModel()->hasChildren(m_rootIndex);
    } else {
        return false;
    }
}

/** @short Reimplemented; this is needed because QAbstractProxyModel does not provide this forward */
bool SubtreeModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    if (!sourceModel())
        return false;
    if (!m_usingInvalidRoot && !m_rootIndex.isValid())
        return false;
    return sourceModel()->dropMimeData(data, action, row, column,
                                       parent.isValid() ? mapToSource(parent) : QModelIndex(m_rootIndex));
}

QHash<int,QByteArray> SubtreeModel::roleNames() const
{
    if (sourceModel())
        return sourceModel()->roleNames();
    else
        return QHash<int, QByteArray>();
}

}
}
