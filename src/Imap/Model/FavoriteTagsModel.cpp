/*
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

#include <QBrush>
#include <QColor>
#include <QSettings>
#include "Common/SettingsNames.h"
#include "FavoriteTagsModel.h"

namespace Imap
{

namespace Mailbox
{

ItemFavoriteTagItem::ItemFavoriteTagItem(const QString &name, const QString &color):
    name(name), color(color)
{
}

ItemFavoriteTagItem::ItemFavoriteTagItem()
{
}

FavoriteTagsModel::FavoriteTagsModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

/** @short Lookup for favorite tags in order and return with the color of the first matched */
QColor FavoriteTagsModel::findBestColorForTags(const QStringList &tags) const
{
    auto result = std::find_first_of(m_tags.begin(), m_tags.end(), tags.begin(), tags.end(),
            [](const ItemFavoriteTagItem &tag, const QString &tag_name) {
        return tag.name == tag_name;
    });

    return result != m_tags.end() ? QColor(result->color) : QColor();
}

/** @short Return with tag name by index or emtpy string if not found */
QString FavoriteTagsModel::tagNameByIndex(const int index) const
{
    if (index >= 0 && index < m_tags.size()) {
        return m_tags[index].name;
    } else {
        return QString();
    }
}

int FavoriteTagsModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return COLUMN_LAST;
}

int FavoriteTagsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_tags.size();
}

QVariant FavoriteTagsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_tags.size() || index.column() >= COLUMN_LAST)
        return QVariant();

    QColor color;
    switch (role) {
    case Qt::ForegroundRole:
        switch (index.column()) {
        case COLUMN_NAME:
            color.setNamedColor(m_tags[index.row()].color);
            return color.isValid() ? QVariant(QBrush(color)) : QVariant();
        default:
            return QVariant();
        }
    case Qt::DisplayRole:
    case Qt::EditRole:
        switch (index.column()) {
        case COLUMN_INDEX:
            return index.row() + 1;
        case COLUMN_NAME:
            return m_tags[index.row()].name;
        case COLUMN_COLOR:
            return m_tags[index.row()].color;
        }
    default:
        return QVariant();
    }
    Q_ASSERT(false);
    return QVariant();
}

QVariant FavoriteTagsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical)
        return QVariant();
    if (role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case COLUMN_INDEX:
        return tr("#");
    case COLUMN_NAME:
        return tr("Name");
    case COLUMN_COLOR:
        return tr("Color");
    default:
        return QVariant();
    }
}

bool FavoriteTagsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;

    switch (index.column()) {
    case COLUMN_NAME:
        m_tags[index.row()].name = value.toString();
        break;
    case COLUMN_COLOR:
        m_tags[index.row()].color = value.toString();
        break;
    default:
        Q_ASSERT(false);
        return false;
    }
    emit dataChanged(index, index);
    return true;
}

void FavoriteTagsModel::moveTag(const int from, const int to)
{
    Q_ASSERT(to >= 0);
    Q_ASSERT(from >= 0);
    Q_ASSERT(to < m_tags.size());
    Q_ASSERT(from < m_tags.size());
    Q_ASSERT(from != to);

    int targetOffset = to;
    if (to > from) {
        ++targetOffset;
    }

    bool ok = beginMoveRows(QModelIndex(), from, from, QModelIndex(), targetOffset);
    Q_ASSERT(ok); Q_UNUSED(ok);

    m_tags.move(from, to);
    endMoveRows();
}

void FavoriteTagsModel::appendTag(const ItemFavoriteTagItem &item)
{
    beginInsertRows(QModelIndex(), m_tags.size(), m_tags.size());
    m_tags << item;
    endInsertRows();
}

void FavoriteTagsModel::removeTagAt(const int position)
{
    Q_ASSERT(position >= 0);
    Q_ASSERT(position < m_tags.size());
    beginRemoveRows(QModelIndex(), position, position);
    m_tags.removeAt(position);
    endRemoveRows();
}

void FavoriteTagsModel::loadFromSettings(QSettings &s)
{
    beginResetModel();
    m_tags.clear();

    int num = s.beginReadArray(Common::SettingsNames::favoriteTagsKey);
    for (int i = 0; i < num; ++i) {
        s.setArrayIndex(i);
        m_tags << ItemFavoriteTagItem(
            s.value(Common::SettingsNames::tagNameKey).toString(),
            s.value(Common::SettingsNames::tagColorKey).toString()
        );
    }
    s.endArray();
    endResetModel();
}

void FavoriteTagsModel::saveToSettings(QSettings &s) const
{
    s.beginWriteArray(Common::SettingsNames::favoriteTagsKey);
    for (int i = 0; i < m_tags.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(Common::SettingsNames::tagNameKey, m_tags[i].name);
        s.setValue(Common::SettingsNames::tagColorKey, m_tags[i].color);
    }
    s.endArray();
}

}

}
