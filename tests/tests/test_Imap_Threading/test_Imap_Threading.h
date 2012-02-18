/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

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

#ifndef TEST_IMAP_THREADING
#define TEST_IMAP_THREADING

#include "test_LibMailboxSync/test_LibMailboxSync.h"

namespace Imap {
namespace Mailbox {
class ThreadingMsgListModel;
}
}

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
protected slots:
    virtual void init();
    virtual void initTestCase();
    virtual void cleanup();
private:
    void complexMapping(Mapping &m, QByteArray &response);
    void initialMessages(const uint exists);

    void verifyMapping(const Mapping &mapping);
    QModelIndex findItem(const QList<int> &where);
    QModelIndex findItem(const QString &where);
    IndexMapping buildIndexMap(const Mapping &mapping);
    void verifyIndexMap(const IndexMapping &indexMap, const Mapping &map);
    QByteArray treeToThreading(QModelIndex index);

    Imap::Mailbox::MsgListModel *msgListModel;
    Imap::Mailbox::ThreadingMsgListModel *threadingModel;
};

#endif
