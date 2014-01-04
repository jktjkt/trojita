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

#include "test_Imap_Offline.h"
#include "Utils/headless_test.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Imap/Tasks/ObtainSynchronizedMailboxTask.h"

using namespace Imap::Mailbox;

void OfflineTest::init()
{
    LibMailboxSync::init();
    MsgListModel *msgListModelA = new MsgListModel(model, model);
    ThreadingMsgListModel *threadingA = new ThreadingMsgListModel(msgListModelA);
    threadingA->setSourceModel(msgListModelA);
    MsgListModel *msgListModelB = new MsgListModel(model, model);
    ThreadingMsgListModel *threadingB = new ThreadingMsgListModel(msgListModelB);
    threadingB->setSourceModel(msgListModelB);
}

/** @short Check that the STATUS processing does not break offline "sync" */
void OfflineTest::testStatusVsExistsCached()
{
    QString mailbox = QLatin1String("a");

    // Push the data to the cache
    existsA = 10;
    uidNextA = 11;
    uidValidityA = 666;
    SyncState sync;
    sync.setExists(existsA);
    sync.setUidValidity(uidValidityA);
    sync.setUidNext(uidNextA);
    for (uint i = 1; i <= existsA; ++i)
        uidMapA << i;
    model->cache()->setMailboxSyncState(mailbox, sync);
    model->cache()->setUidMapping(mailbox, uidMapA);

    // Request the STATUS command to see how many messages are in there. This is not a synchronization, though
    // -- Trojita is *not* opening that mailbox, so the previously cached data remain.
    QCOMPARE(idxA.data(RoleTotalMessageCount).toInt(), 0);
    cClient(t.mk("STATUS a (MESSAGES UNSEEN RECENT)\r\n"));
    cServer("* STATUS a (MESSAGES 42 UNSEEN 2 RECENT 3)\r\n" + t.last("OK status\r\n"));
    QCOMPARE(idxA.data(RoleTotalMessageCount).toInt(), 42);
    LibMailboxSync::setModelNetworkPolicy(model, NETWORK_OFFLINE);
    cClient(t.mk("LOGOUT\r\n"));
    cServer(t.last("OK logged out\r\n") + "* BYE see ya\r\n");
    model->resyncMailbox(idxA);
    for (int i = 0; i < 10; ++i)
        QCoreApplication::processEvents();
    QCOMPARE(model->rowCount(msgListA), static_cast<int>(existsA));
    helperVerifyUidMapA();
    helperCheckCache();
}

TROJITA_HEADLESS_TEST(OfflineTest)
