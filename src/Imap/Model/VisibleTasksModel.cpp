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

#include "VisibleTasksModel.h"
#include "kdeui-itemviews/kdescendantsproxymodel.h"
#include "ItemRoles.h"

namespace Imap
{
namespace Mailbox
{

/** @short Construct the required proxy model hierarchy */
VisibleTasksModel::VisibleTasksModel(QObject *parent, QAbstractItemModel *taskModel) :
    QSortFilterProxyModel(parent)
{
    m_flatteningModel = new KDescendantsProxyModel(this);
    m_flatteningModel->setSourceModel(taskModel);
    setSourceModel(m_flatteningModel);
    setDynamicSortFilter(true);
    connect(m_flatteningModel, &QAbstractItemModel::rowsInserted, this, &VisibleTasksModel::hasVisibleTasksChanged);
    connect(m_flatteningModel, &QAbstractItemModel::rowsRemoved, this, &VisibleTasksModel::hasVisibleTasksChanged);
    connect(m_flatteningModel, &QAbstractItemModel::modelReset, this, &VisibleTasksModel::hasVisibleTasksChanged);
    connect(m_flatteningModel, &QAbstractItemModel::layoutChanged, this, &VisibleTasksModel::hasVisibleTasksChanged);
}

QHash<int, QByteArray> VisibleTasksModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[RoleTaskCompactName] = "compactName";
    return roleNames;
}

/** @short Reimplemented from QSortFilterProxyModel */
bool VisibleTasksModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    if (index.data(RoleTaskIsParserState).toBool()) {
        return false;
    }

    return index.data(RoleTaskIsVisible).toBool();
}

bool VisibleTasksModel::hasVisibleTasks() const
{
    return rowCount();
}

}
}
