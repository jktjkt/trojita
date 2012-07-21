/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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
    connect(m_flatteningModel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SIGNAL(hasVisibleTasksChanged()));
    connect(m_flatteningModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SIGNAL(hasVisibleTasksChanged()));
    connect(m_flatteningModel, SIGNAL(modelReset()), this, SIGNAL(hasVisibleTasksChanged()));
    connect(m_flatteningModel, SIGNAL(layoutChanged()), this, SIGNAL(hasVisibleTasksChanged()));
    QHash<int, QByteArray> roleNames;
    roleNames[RoleTaskCompactName] = "compactName";
    setRoleNames(roleNames);
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
