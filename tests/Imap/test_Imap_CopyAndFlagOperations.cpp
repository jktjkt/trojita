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

#include "test_Imap_CopyAndFlagOperations.h"
#include "Utils/headless_test.h"
#include "Utils/FakeCapabilitiesInjector.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Imap/Tasks/ObtainSynchronizedMailboxTask.h"

using namespace Imap::Mailbox;

void CopyAndFlagTest::testMoveRfc3501()
{
    helperMove(JUST_3501);
}

void CopyAndFlagTest::testMoveUidPlus()
{
    helperMove(HAS_UIDPLUS);
}

void CopyAndFlagTest::testMoveRfcMove()
{
    helperMove(HAS_MOVE);
}

void CopyAndFlagTest::helperMove(const MoveFeatures serverFeatures)
{
    FakeCapabilitiesInjector injector(model);
    switch (serverFeatures) {
    case JUST_3501:
        // nothing is needed
        break;
    case HAS_UIDPLUS:
        injector.injectCapability("UIDPLUS");
        break;
    case HAS_MOVE:
        injector.injectCapability("MOVE");
        break;
    }

    // Push the data to the cache
    existsA = 3;
    uidNextA = 5;
    uidValidityA = 666;
    for (uint i = 1; i <= existsA; ++i)
        uidMapA << i;
    helperSyncAWithMessagesEmptyState();
    helperCheckCache();
    helperVerifyUidMapA();
    QString mailbox = QLatin1String("a");
    QString del = QLatin1String("\\Deleted");
    Q_FOREACH(const auto uid, uidMapA) {
        // No message shall be marked as deleted
        QVERIFY(!model->cache()->msgFlags(mailbox, uid).contains(del));
    }

    // FIXME: this API sucks rather hard; it will have to change to use indexes at some point, if only for sanity
    auto aMailboxPtr = dynamic_cast<TreeItemMailbox *>(Model::realTreeItem(idxA));
    Q_ASSERT(aMailboxPtr);
    model->copyMoveMessages(aMailboxPtr, QLatin1String("b"), QList<uint>() << 2, MOVE);

    if (serverFeatures == HAS_MOVE) {
        cClient(t.mk("UID MOVE 2 b\r\n"));
        cServer("* 2 EXPUNGE\r\n" + t.last("OK moved\r\n"));
        --existsA;
        uidMapA.removeAt(1);
        helperCheckCache();
        helperVerifyUidMapA();
        Q_FOREACH(const auto uid, uidMapA) {
            // None message shall be marked as deleted
            QVERIFY(!model->cache()->msgFlags(mailbox, uid).contains(del));
        }
        cEmpty();
    } else {
        cClient(t.mk("UID COPY 2 b\r\n"));
        cServer(t.last("OK copied\r\n"));
        cClient(t.mk("UID STORE 2 +FLAGS.SILENT \\Deleted\r\n"));
        cServer(t.last("OK stored\r\n"));
        QVERIFY(model->cache()->msgFlags(mailbox, 2).contains(del));
        if (serverFeatures == HAS_UIDPLUS) {
            cClient(t.mk("UID EXPUNGE 2\r\n"));
            cServer("* 2 EXPUNGE\r\n" + t.last("OK expunged\r\n"));
            --existsA;
            uidMapA.removeAt(1);
            helperCheckCache();
            helperVerifyUidMapA();
            Q_FOREACH(const auto uid, uidMapA) {
                // None message shall be marked as deleted
                QVERIFY(!model->cache()->msgFlags(mailbox, uid).contains(del));
            }
        }
        cEmpty();
    }
    justKeepTask();
}

void CopyAndFlagTest::testUpdateAllFlags()
{
    // Push the data to the cache
    existsA = 4;
    uidNextA = 5;
    uidValidityA = 666;
    for (uint i = 1; i <= existsA; ++i)
        uidMapA << i;
    // cannot use helperSyncAWithMessagesEmptyState(), we want to have control over the flags
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 4 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] UIDs valid\r\n"
            "* OK [UIDNEXT 5] Predicted next UID\r\n"
            + t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 3 4 2 1\r\n" + t.last("OK search\r\n"));
    cClient(t.mk("FETCH 1:4 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS ())\r\n"
            "* 2 FETCH (FLAGS (\\SEEN))\r\n"
            "* 3 FETCH (FLAGS (\\Answered \\Seen))\r\n"
            "* 4 FETCH (FLAGS (\\Answered))\r\n"
            + t.last("OK fetched\r\n"));
    helperCheckCache();
    helperVerifyUidMapA();
    QString mailbox = QLatin1String("a");
    QString seen = QLatin1String("\\Seen");
    QVERIFY(!msgListA.child(0, 0).data(RoleMessageIsMarkedRead).toBool());
    QVERIFY(!model->cache()->msgFlags(mailbox, 1).contains(seen));
    QVERIFY(msgListA.child(1, 0).data(RoleMessageIsMarkedRead).toBool());
    QVERIFY(model->cache()->msgFlags(mailbox, 2).contains(seen));
    QVERIFY(msgListA.child(2, 0).data(RoleMessageIsMarkedRead).toBool());
    QVERIFY(model->cache()->msgFlags(mailbox, 3).contains(seen));
    QVERIFY(!msgListA.child(3, 0).data(RoleMessageIsMarkedRead).toBool());
    QVERIFY(!model->cache()->msgFlags(mailbox, 4).contains(seen));

    // Mark all messages as read
    model->markMailboxAsRead(idxA);
    cClient(t.mk("STORE 1:* +FLAGS.SILENT \\Seen\r\n"));
    cServer(t.last("OK stored\r\n"));
    for (uint i = 0; i < existsA; ++i) {
        QVERIFY(msgListA.child(i, 0).data(RoleMessageIsMarkedRead).toBool());
        QVERIFY(model->cache()->msgFlags(mailbox, uidMapA[i]).contains(seen));
    }

    cEmpty();
    justKeepTask();
}

TROJITA_HEADLESS_TEST(CopyAndFlagTest)
