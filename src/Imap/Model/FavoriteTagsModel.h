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

#ifndef FAVORITETAGSMODEL_H
#define FAVORITETAGSMODEL_H

#include <QAbstractTableModel>

class QSettings;

namespace Imap
{

namespace Mailbox
{

class ItemFavoriteTagItem
{
public:
    ItemFavoriteTagItem(const QString &name, const QString &color);
    ItemFavoriteTagItem();

    QString name;
    QString color;
};

/** @short Model for a list of available preset tags */
class FavoriteTagsModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum { COLUMN_INDEX, COLUMN_NAME, COLUMN_COLOR, COLUMN_LAST };

    explicit FavoriteTagsModel(QObject *parent = nullptr);
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    /** @short Reimplemented from QAbstractTableModel; required for QDataWidgetMapper. */
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    void appendTag(const ItemFavoriteTagItem &item);
    void removeTagAt(const int position);
    void moveTag(const int from, const int to);

    QColor findBestColorForTags(const QStringList &tags) const;
    QString tagNameByIndex(const int index) const;

    /** @short Replace the contents of this model by data read from a QSettings instance */
    void loadFromSettings(QSettings &s);
    /** @short Save the data into a QSettings instance */
    void saveToSettings(QSettings &s) const;

private:
    QList<ItemFavoriteTagItem> m_tags;
};

}

}

#endif // FAVORITETAGSMODEL_H
