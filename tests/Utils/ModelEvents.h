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

#ifndef TEST_MODEL_EVENTS
#define TEST_MODEL_EVENTS

#include <QModelIndex>

class ModelInsertRemoveEvent
{

public:
    ModelInsertRemoveEvent(const QList<QVariant> &values);
    ModelInsertRemoveEvent(const QModelIndex &parent, int start, int end);

    bool operator==(const ModelInsertRemoveEvent &b) const;


    QModelIndex parent;
    int start;
    int end;
};

class ModelMoveEvent
{

public:
    ModelMoveEvent(const QVariantList &values);
    ModelMoveEvent(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationparent,
                   int destinationRow);

    bool operator==(const ModelMoveEvent &b) const;


    QModelIndex sourceParent;
    int sourceStart;
    int sourceEnd;
    QModelIndex destinationParent;
    int destinationRow;
};

namespace QTest {

/** @short Helper for pretty-printing in QCOMPARE */
template<>
char *toString(const ModelInsertRemoveEvent &event);

/** @short Helper for pretty-printing in QCOMPARE */
template<>
char *toString(const ModelMoveEvent &event);

}

#endif
