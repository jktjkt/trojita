/* Copyright (C) 2012 - 2013 Peter Amidon <peter@picnicpark.org>

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

#include <QtTest>
#include "ModelEvents.h"
#include "Common/MetaTypes.h"

namespace QTest
{
    template<>
    char *toString(const ModelInsertRemoveEvent &event)
    {
        QString buf;
        QDebug(&buf) << "parent:" << event.parent
                     << "start:" << event.start
                     << "end:" << event.end;
        return qstrdup(buf.toUtf8().constData());
    }

    template<>
    char *toString(const ModelMoveEvent &event)
    {
        QString buf;
        QDebug(&buf) << "sourceParent:" << event.sourceParent << "sourceStart:" << event.sourceStart
                     << "sourceEnd:" << event.sourceEnd << "destinationParent:" << event.destinationParent
                     << "destinationRow:" << event.destinationRow;
        return qstrdup(buf.toUtf8().constData());
    }
}

ModelInsertRemoveEvent::ModelInsertRemoveEvent(const QVariantList &values)
{
    parent = qvariant_cast<QModelIndex>(values.at(0));
    start = values.at(1).toInt();
    end = values.at(2).toInt();
}

ModelInsertRemoveEvent::ModelInsertRemoveEvent(const QModelIndex &parent, int start, int end) :
    parent(parent), start(start), end(end)
{
}

bool ModelInsertRemoveEvent::operator==(const ModelInsertRemoveEvent &b) const
{
    return ((parent == b.parent) && (start == b.start) && (end == b.end));
}

ModelMoveEvent::ModelMoveEvent(const QVariantList &values)
{
    sourceParent = qvariant_cast<QModelIndex>(values.at(0));
    sourceStart = values.at(1).toInt();
    sourceEnd = values.at(2).toInt();
    destinationParent = qvariant_cast<QModelIndex>(values.at(3));
    destinationRow = values.at(4).toInt();
}

ModelMoveEvent::ModelMoveEvent(const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
                               const QModelIndex &destinationParent, int destinationRow) :
    sourceParent(sourceParent), sourceStart(sourceStart), sourceEnd(sourceEnd),
    destinationParent(destinationParent), destinationRow(destinationRow)
{
}

bool ModelMoveEvent::operator==(const ModelMoveEvent &b) const
{
    return (sourceParent == b.sourceParent) && (sourceStart == b.sourceStart) && (sourceEnd == b.sourceEnd) &&
      (destinationParent == b.destinationParent) && (destinationRow == b.destinationRow);
}
