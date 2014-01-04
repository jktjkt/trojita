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

#ifndef TEST_IMAP_THREADING
#define TEST_IMAP_THREADING

#include "Utils/LibMailboxSync.h"

typedef QMap<QString, int> Mapping;
typedef QMap<QString, QPersistentModelIndex> IndexMapping;

/** @short Test the THREAD response processing and the ThreadingMsgListModel's correctness */
class ImapModelThreadingTest : public LibMailboxSync
{
    Q_OBJECT
private slots:
    void testStaticThreading();
    void testStaticThreading_data();
    void testDynamicThreading();
    void testThreadDeletionsAdditions();
    void testThreadDeletionsAdditions_data();
    void testDynamicSorting();
    void testDynamicSortingContext();
    void testDynamicSearch();
    void testIncrementalThreading();
    void testRemovingRootWithThreadingInFlight();
    void testMultipleExpunges();
    void testThreadingPerformance();
    void testSortingPerformance();
    void testSearchingPerformance();

    void helper_multipleExpunges();
protected slots:
    virtual void init();
private:
    void complexMapping(Mapping &m, QByteArray &response);
    static QByteArray prepareHugeUntaggedThread(const uint num);

    void verifyMapping(const Mapping &mapping);
    QModelIndex findItem(const QString &where);
    IndexMapping buildIndexMap(const Mapping &mapping);
    void verifyIndexMap(const IndexMapping &indexMap, const Mapping &map);
    QByteArray treeToThreading(QModelIndex index);
    QByteArray numListToString(const QList<uint> &seq);

    template<typename T> void reverseContainer(T &container);

    QPersistentModelIndex helper_indexMultipleExpunges_1;
    int helper_multipleExpunges_hit;
};

#endif
