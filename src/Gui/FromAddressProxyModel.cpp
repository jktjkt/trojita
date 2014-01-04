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

#include "FromAddressProxyModel.h"
#include "Composer/SenderIdentitiesModel.h"

namespace Gui
{

FromAddressProxyModel::FromAddressProxyModel(QObject *parent) :
    QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void FromAddressProxyModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    Q_ASSERT(!sourceModel || qobject_cast<Composer::SenderIdentitiesModel*>(sourceModel));
    QSortFilterProxyModel::setSourceModel(sourceModel);
}

bool FromAddressProxyModel::filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent);
    return source_column == 0;
}

QVariant FromAddressProxyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return QSortFilterProxyModel::data(index, role);
    QModelIndex sourceIndex = mapToSource(index);
    Q_ASSERT(sourceIndex.isValid());
    QModelIndex nameIndex = sourceIndex.sibling(sourceIndex.row(), Composer::SenderIdentitiesModel::COLUMN_NAME);
    QModelIndex emailIndex = sourceIndex.sibling(sourceIndex.row(), Composer::SenderIdentitiesModel::COLUMN_EMAIL);
    Q_ASSERT(nameIndex.isValid());
    Q_ASSERT(emailIndex.isValid());
    return QString::fromUtf8("%1 <%2>").arg(nameIndex.data().toString(), emailIndex.data().toString());
}

}
