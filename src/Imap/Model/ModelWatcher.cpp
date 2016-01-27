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
#include <QDebug>
#include "ModelWatcher.h"

namespace Imap
{

namespace Mailbox
{

void ModelWatcher::setModel(QAbstractItemModel *model)
{
    connect(model, &QAbstractItemModel::columnsAboutToBeInserted, this, &ModelWatcher::columnsAboutToBeInserted);
    connect(model, &QAbstractItemModel::columnsAboutToBeRemoved, this, &ModelWatcher::columnsAboutToBeRemoved);
    connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this, &ModelWatcher::rowsAboutToBeInserted);
    connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &ModelWatcher::rowsAboutToBeRemoved);
    connect(model, &QAbstractItemModel::columnsInserted, this, &ModelWatcher::columnsInserted);
    connect(model, &QAbstractItemModel::columnsRemoved, this, &ModelWatcher::columnsRemoved);
    connect(model, &QAbstractItemModel::rowsInserted, this, &ModelWatcher::rowsInserted);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &ModelWatcher::rowsRemoved);
    connect(model, &QAbstractItemModel::dataChanged, this, &ModelWatcher::dataChanged);
    connect(model, &QAbstractItemModel::headerDataChanged, this, &ModelWatcher::headerDataChanged);

    connect(model, &QAbstractItemModel::rowsAboutToBeMoved, this, &ModelWatcher::rowsAboutToBeMoved);
    connect(model, &QAbstractItemModel::rowsMoved, this, &ModelWatcher::rowsMoved);
    connect(model, &QAbstractItemModel::columnsAboutToBeMoved, this, &ModelWatcher::columnsAboutToBeMoved);
    connect(model, &QAbstractItemModel::columnsMoved, this, &ModelWatcher::columnsMoved);
    connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, &ModelWatcher::layoutAboutToBeChanged);
    connect(model, &QAbstractItemModel::layoutChanged, this, &ModelWatcher::layoutChanged);
    connect(model, &QAbstractItemModel::modelAboutToBeReset, this, &ModelWatcher::modelAboutToBeReset);
    connect(model, &QAbstractItemModel::modelReset, this, &ModelWatcher::modelReset);
}

void ModelWatcher::columnsAboutToBeInserted(const QModelIndex &parent, int start, int end)
{
    qDebug() << sender()->objectName() << "columnsAboutToBeInserted(" << parent << start << end << ")";
}

void ModelWatcher::columnsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    qDebug() << sender()->objectName() << "columnsAboutToBeRemoved(" << parent << start << end << ")";
}

void ModelWatcher::columnsInserted(const QModelIndex &parent, int start, int end)
{
    qDebug() << sender()->objectName() << "columnsInserted(" << parent << start << end << ")";
}

void ModelWatcher::columnsRemoved(const QModelIndex &parent, int start, int end)
{
    qDebug() << sender()->objectName() << "columnsRemoved(" << parent << start << end << ")";
}

void ModelWatcher::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    qDebug() << sender()->objectName() << "dataChanged(" << topLeft << bottomRight << ")";
    if (!m_ignoreData)
        qDebug() << "new data" << topLeft.data(Qt::DisplayRole);
}
void ModelWatcher::headerDataChanged(Qt::Orientation orientation, int first, int last)
{
    qDebug() << sender()->objectName() << "headerDataChanged(" << orientation << first << last << ")";
}
void ModelWatcher::layoutAboutToBeChanged()
{
    qDebug() << sender()->objectName() << "layoutAboutToBeChanged()";
}

void ModelWatcher::layoutChanged()
{
    qDebug() << sender()->objectName() << "layoutChanged()";
}

void ModelWatcher::modelAboutToBeReset()
{
    qDebug() << sender()->objectName() << "modelAboutToBeReset()";
}

void ModelWatcher::modelReset()
{
    qDebug() << sender()->objectName() << "modelReset()";
}

void ModelWatcher::rowsAboutToBeInserted(const QModelIndex &parent, int start, int end)
{
    qDebug() << sender()->objectName() << "rowsAboutToBeInserted(" << parent << start << end << ")";
}

void ModelWatcher::rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    qDebug() << sender()->objectName() << "rowsAboutToBeRemoved(" << parent << start << end << ")";
}

void ModelWatcher::rowsInserted(const QModelIndex &parent, int start, int end)
{
    qDebug() << sender()->objectName() << "rowsInserted(" << parent << start << end << ")";
}

void ModelWatcher::rowsRemoved(const QModelIndex &parent, int start, int end)
{
    qDebug() << sender()->objectName() << "rowsRemoved(" << parent << start << end << ")";
}

void ModelWatcher::rowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
                                      const QModelIndex &destinationParent, int destinationRow)
{
    qDebug() << sender()->objectName() << "rowsAboutToBeMoved" << sourceParent << sourceStart << sourceEnd <<
                destinationParent << destinationRow;
}

void ModelWatcher::rowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row)
{
    qDebug() << sender()->objectName() << "rowsMoved" << parent << start << end << destination << row;
}

void ModelWatcher::columnsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
                                         const QModelIndex &destinationParent, int destinationColumn)
{
    qDebug() << sender()->objectName() << "columnsAboutToBeMoved" << sourceParent << sourceStart << sourceEnd <<
                destinationParent << destinationColumn;
}

void ModelWatcher::columnsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int column)
{
    qDebug() << sender()->objectName() << "columnsMoved" << parent << start << end << destination << column;
}

}

}
