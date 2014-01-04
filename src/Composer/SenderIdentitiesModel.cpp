/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2012        Mohammed Nafees <nafees.technocool@gmail.com>
   Copyright (C) 2012 Peter Amidon <peter@picnicpark.org>

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
#include <QSettings>
#include "Common/SettingsNames.h"

namespace Composer
{

ItemSenderIdentity::ItemSenderIdentity(const QString &realName, const QString &emailAddress,
                                       const QString &organisation, const QString &signature):
    realName(realName), emailAddress(emailAddress), organisation(organisation), signature(signature)
{
}

ItemSenderIdentity::ItemSenderIdentity()
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

QVariant SenderIdentitiesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_identities.size() || index.column() >= COLUMN_LAST)
        return QVariant();

    if (role != Qt::DisplayRole && role != Qt::EditRole) {
        // For simplicity, we don't need anything fancy here
        return QVariant();
    }

    switch (index.column()) {
    case COLUMN_NAME:
        return m_identities[index.row()].realName;
    case COLUMN_EMAIL:
        return m_identities[index.row()].emailAddress;
    case COLUMN_ORGANIZATION:
        return m_identities[index.row()].organisation;
    case COLUMN_SIGNATURE:
        return m_identities[index.row()].signature;
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
    case COLUMN_ORGANIZATION:
        return tr("Organization");
    case COLUMN_SIGNATURE:
        return tr("Signature");
    default:
        return QVariant();
    }
}

bool SenderIdentitiesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;

    switch (index.column()) {
    case COLUMN_NAME:
        m_identities[index.row()].realName = value.toString();
        break;
    case COLUMN_EMAIL:
        m_identities[index.row()].emailAddress = value.toString();
        break;
    case COLUMN_ORGANIZATION:
        m_identities[index.row()].organisation = value.toString();
        break;
    case COLUMN_SIGNATURE:
        m_identities[index.row()].signature = value.toString();
        break;
    default:
        Q_ASSERT(false);
        return false;
    }
    emit dataChanged(index, index);
    return true;
}

void SenderIdentitiesModel::moveIdentity(const int from, const int to)
{
    Q_ASSERT(to >= 0);
    Q_ASSERT(from >= 0);
    Q_ASSERT(to < m_identities.size());
    Q_ASSERT(from < m_identities.size());
    Q_ASSERT(from != to);

    int targetOffset = to;
    if (to > from) {
        ++targetOffset;
    }

    bool ok = beginMoveRows(QModelIndex(), from, from, QModelIndex(), targetOffset);
    Q_ASSERT(ok); Q_UNUSED(ok);

    m_identities.move(from, to);
    endMoveRows();
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

void SenderIdentitiesModel::loadFromSettings(QSettings &s)
{
    beginResetModel();
    m_identities.clear();

    int num = s.beginReadArray(Common::SettingsNames::identitiesKey);
    // The new format with multiple identities
    for (int i = 0; i < num; ++i) {
        s.setArrayIndex(i);
        m_identities << ItemSenderIdentity(
                            s.value(Common::SettingsNames::realNameKey).toString(),
                            s.value(Common::SettingsNames::addressKey).toString(),
                            s.value(Common::SettingsNames::organisationKey).toString(),
                            s.value(Common::SettingsNames::signatureKey).toString());
    }
    s.endArray();
    endResetModel();
}

void SenderIdentitiesModel::saveToSettings(QSettings &s) const
{
    s.beginWriteArray(Common::SettingsNames::identitiesKey);
    for (int i = 0; i < m_identities.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(Common::SettingsNames::realNameKey, m_identities[i].realName);
        s.setValue(Common::SettingsNames::addressKey, m_identities[i].emailAddress);
        s.setValue(Common::SettingsNames::organisationKey, m_identities[i].organisation);
        s.setValue(Common::SettingsNames::signatureKey, m_identities[i].signature);
    }
    s.endArray();
}

}
