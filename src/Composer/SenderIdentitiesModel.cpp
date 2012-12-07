/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

#include "SenderIdentitiesModel.h"

namespace Composer
{

ItemSenderIdentity::ItemSenderIdentity(const QString &realName, const QString &emailAddress):
    realName(realName), emailAddress(emailAddress)
{
}

SenderIdentitiesModel::SenderIdentitiesModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

int SenderIdentitiesModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return COLUMN_LAST;
}

int SenderIdentitiesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_identities.size();
}

QVariant SenderIdentitiesModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() >= m_identities.size() || index.column() >= COLUMN_LAST)
        return QVariant();

    if (role != Qt::DisplayRole) {
        // For simplicity, we don't need anything fancy here
        return QVariant();
    }

    switch (index.column()) {
    case COLUMN_NAME:
        return m_identities[index.row()].realName;
    case COLUMN_EMAIL:
        return m_identities[index.row()].emailAddress;
    }
    Q_ASSERT(false);
    return QVariant();
}

QVariant SenderIdentitiesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical)
        return QVariant();
    if (role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case COLUMN_NAME:
        return tr("Name");
    case COLUMN_EMAIL:
        return tr("E-mail");
    default:
        return QVariant();
    }
}

void SenderIdentitiesModel::appendIdentity(const ItemSenderIdentity &item)
{
    beginInsertRows(QModelIndex(), m_identities.size(), m_identities.size());
    m_identities << item;
    endInsertRows();
}

void SenderIdentitiesModel::removeIdentityAt(const int position)
{
    Q_ASSERT(position >= 0);
    Q_ASSERT(position < m_identities.size());
    beginRemoveRows(QModelIndex(), position, position);
    m_identities.removeAt(position);
    endRemoveRows();
}

}
