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

#include <QtTest>
#include "test_Imap_DisappearingMailboxes.h"
#include "Utils/headless_test.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/TaskPresentationModel.h"
#include "Streams/FakeSocket.h"
#include "Utils/FakeCapabilitiesInjector.h"

/** @short This test verifies that we don't segfault during offline -> online transition */
void ImapModelDisappearingMailboxTest::testGoingOfflineOnline()
{
    helperSyncBNoMessages();
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_OFFLINE);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), t.mk("LOGOUT\r\n"));
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    t.reset();

    // We can't call helperSyncBNoMessages() here, it relies on msgListB's validity,
    // but that index is not necessary valid because of our kludgy fake listing...

    QCOMPARE( model->rowCount( msgListB ), 0 );
    model->switchToMailbox( idxB );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT b\r\n") );
    SOCK->fakeReading( QByteArray("* 0 exists\r\n")
                                  + t.last("ok completed\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

void ImapModelDisappearingMailboxTest::testGoingOfflineOnlineExamine()
{
    helperTestGoingReallyOfflineOnline(false);
}

void ImapModelDisappearingMailboxTest::testGoingOfflineOnlineUnselect()
{
    helperTestGoingReallyOfflineOnline(true);
}

/** @short Simulate what happens when user goes offline with views attached

This is intended to be very similar to how real application behaves, reacting to events etc.

This is a test for issue #88 where the ObtainSynchronizedMailboxTask failed to account for the possibility
of indexes getting invalidated while the sync is in progress.
*/
void ImapModelDisappearingMailboxTest::helperTestGoingReallyOfflineOnline(bool withUnselect)
{
    // At first, open mailbox B
    helperSyncBNoMessages();

    // Make sure the socket is present
    QPointer<Streams::Socket> socketPtr(factory->lastSocket());
    Q_ASSERT(!socketPtr.isNull());

    // Go offline
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_OFFLINE);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QCOMPARE(SOCK->writtenStuff(), t.mk("LOGOUT\r\n"));
    SOCK->fakeReading(QByteArray("* BYE see ya\r\n")
                      + t.last("ok logged out\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // It should be gone by now
    QVERIFY(socketPtr.isNull());

    // So now we're offline and want to reconnect back to see if we break.

    // Try a reconnect
    taskFactoryUnsafe->fakeListChildMailboxes = false;
    t.reset();
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The trick here is that the reconnect resulted in querying a mailbox listing again
    QCOMPARE(SOCK->writtenStuff(), t.mk("LIST \"\" \"%\"\r\n"));
    QByteArray listResponse = QByteArray("* LIST (\\HasNoChildren) \".\" \"b\"\r\n"
                                         "* LIST (\\HasNoChildren) \".\" \"a\"\r\n")
                              + t.last("OK List done.\r\n");

    if (withUnselect) {
        // We'll need the UNSELECT later on
        FakeCapabilitiesInjector injector(model);
        injector.injectCapability(QLatin1String("UNSELECT"));
    }

    // But before we "receive" the LIST responses, GUI could easily request syncing of mailbox B again,
    // which is what we do here
    QCOMPARE( model->rowCount( msgListB ), 0 );
    model->switchToMailbox( idxB );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT b\r\n") );
    QByteArray selectResponse =  QByteArray("* 0 exists\r\n") + t.last("ok completed\r\n");

    // Nice, so we're in the middle of a SELECT. Let's confuse things a bit by finalizing the LIST now :).
    SOCK->fakeReading(listResponse);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // At this point, the msgListB should be invalidated
    QVERIFY(!idxB.isValid());
    QVERIFY(!msgListB.isValid());
    // ... and therefore the SELECT handler should take care not to rely on it being valid
    SOCK->fakeReading(selectResponse);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    if (withUnselect) {
        // It should've noticed that the index is gone, and try to get out of there
        QCOMPARE(SOCK->writtenStuff(), t.mk("UNSELECT\r\n"));
    } else {
        // The actual mailbox contains a timestamp, so let's take a shortcut here
        QVERIFY(SOCK->writtenStuff().startsWith(t.mk("EXAMINE \"trojita non existing ")));
    }

    // Make sure it really ignores stuff
    SOCK->fakeReading(QByteArray("* 666 FETCH (FLAGS ())\r\n")
                      // and make it happy by switching away from that mailbox
                      + t.last("OK gone from mailbox\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Verify the shape of the tree now
    QCOMPARE(model->rowCount(QModelIndex()), 3);
    // the first one will be "list of messages"
    idxA = model->index(1, 0, QModelIndex());
    idxB = model->index(2, 0, QModelIndex());
    QVERIFY(idxA.isValid());
    QVERIFY(idxB.isValid());
    QCOMPARE( model->data(idxA, Qt::DisplayRole), QVariant(QLatin1String("a")));
    QCOMPARE( model->data(idxB, Qt::DisplayRole), QVariant(QLatin1String("b")));
    msgListA = idxA.child(0, 0);
    msgListB = idxB.child(0, 0);
    QVERIFY(msgListA.isValid());
    QVERIFY(msgListB.isValid());

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    if (!withUnselect) {
        QVERIFY(SOCK->writtenStuff().startsWith(t.mk("EXAMINE \"trojita non existing ")));
        cServer(t.last("NO no such mailbox\r\n"));
    }

    QVERIFY(SOCK->writtenStuff().isEmpty());
}

/** @short Simulate traffic into a selected mailbox whose index got invalidated

This is a test for issue #124 where Trojita's KeepMailboxOpenTask assert()ed on an index getting invalidated
while the mailbox was still selected and synced properly.
*/
void ImapModelDisappearingMailboxTest::testTrafficAfterSyncedMailboxGoesAway()
{
    existsA = 2;
    uidValidityA = 333;
    uidMapA << 666 << 686;
    uidNextA = 1337;
    helperSyncAWithMessagesEmptyState();

    // disable preload
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_EXPENSIVE);

    // and request some FETCH command
    QModelIndex messageIdx = msgListA.child(0, 0);
    Q_ASSERT(messageIdx.isValid());
    QCOMPARE(messageIdx.data(Imap::Mailbox::RoleMessageSubject), QVariant());
    cClient(t.mk("UID FETCH 666 (" FETCH_METADATA_ITEMS ")\r\n"));
    QByteArray fetchResponse = helperCreateTrivialEnvelope(1, 666, QString("blah")) + t.last("OK fetched\r\n");

    // Request going to another mailbox, eventually
    QCOMPARE(model->rowCount(msgListB), 0);

    // Ask for mailbox metadata
    QCOMPARE(idxB.data(Imap::Mailbox::RoleTotalMessageCount), QVariant());
    cClient(t.mk("STATUS b (MESSAGES UNSEEN RECENT)\r\n"));
    QByteArray statusBResp = QByteArray("* STATUS b (MESSAGES 3 UNSEEN 0 RECENT 0)\r\n") + t.last("OK status sent\r\n");

    // We want to control this stuff
    taskFactoryUnsafe->fakeListChildMailboxes = false;

    model->reloadMailboxList();
    // And for simplicity, let's enable UNSELECT
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QLatin1String("UNSELECT"));

    // The trick here is that the reconnect resulted in querying a mailbox listing again
    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    cServer(QByteArray("* LIST (\\HasNoChildren) \".\" \"b\"\r\n"
                       "* LIST (\\HasChildren) \".\" \"a\"\r\n"
                       "* LIST (\\HasNoChildren) \".\" \"c\"\r\n")
            + t.last("OK List done.\r\n"));

    // We have to refresh the indexes, of course
    idxA = model->index(1, 0, QModelIndex());
    idxB = model->index(2, 0, QModelIndex());
    idxC = model->index(3, 0, QModelIndex());
    QCOMPARE(model->data(idxA, Qt::DisplayRole), QVariant(QLatin1String("a")));
    QCOMPARE(model->data(idxB, Qt::DisplayRole), QVariant(QLatin1String("b")));
    QCOMPARE(model->data(idxC, Qt::DisplayRole), QVariant(QLatin1String("c")));
    msgListA = model->index(0, 0, idxA);
    msgListB = model->index(0, 0, idxB);
    msgListC = model->index(0, 0, idxC);

    // Add some unsolicited untagged data
    cServer(QByteArray("* 666 FETCH (FLAGS ())\r\n"));
    cClient(t.mk("UNSELECT\r\n"));

    // ...once again
    cServer(QByteArray("* 333 FETCH (FLAGS ())\r\n"));

    // At this point, send also a tagged OK for the fetch command; this used to hit an assert
    cServer(fetchResponse);
    cServer(t.last("OK unselected\r\n"));

    // Queue a few requests for status of a few mailboxes
    QCOMPARE(idxA.data(Imap::Mailbox::RoleTotalMessageCount), QVariant());

    // now receive the bits about the (long forgotten) STATUS b
    cServer(statusBResp);
    QCOMPARE(idxB.data(Imap::Mailbox::RoleTotalMessageCount), QVariant(3));
    // because STATUS responses are handled through the Model itself, we get correct data here

    // ...answer the STATUS a
    cClient(t.mk("STATUS a (MESSAGES UNSEEN RECENT)\r\n"));
    cServer(t.last("OK status sent\r\n"));

    // And yet another mailbox request
    QCOMPARE(model->rowCount(msgListC), 0);

    cClient(t.mk("SELECT c\r\n"));
    cServer(t.last("OK selected\r\n"));

    cEmpty();
}

/** @short Connection going offline shall not be reused for further requests for message structure

The code in the Imap::Mailbox::Model already checks for connection status before asking for message structure.
*/
void ImapModelDisappearingMailboxTest::testSlowOfflineMsgStructure()
{
    // Initialize the environment
    existsA = 1;
    uidValidityA = 1;
    uidMapA << 1;
    uidNextA = 2;
    helperSyncAWithMessagesEmptyState();
    idxA = model->index(1, 0, QModelIndex());
    QVERIFY(idxA.isValid());
    QCOMPARE(model->data(idxA, Qt::DisplayRole), QVariant(QLatin1String("a")));
    msgListA = idxA.child(0, 0);
    QVERIFY(msgListA.isValid());
    QModelIndex msg = msgListA.child(0, 0);
    QVERIFY(msg.isValid());
    Streams::FakeSocket *origSocket = SOCK;

    // Switch the connection to an offline mode, but postpone the BYE response
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_OFFLINE);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), t.mk("LOGOUT\r\n"));

    // Ask for the bodystructure of this message
    QCOMPARE(model->rowCount(msg), 0);

    // Make sure that nothing else happens
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QVERIFY(SOCK == origSocket);
}

/** @short Test that requests for updating message flags will fail when offline */
void ImapModelDisappearingMailboxTest::testSlowOfflineFlags()
{
    // Initialize the environment
    existsA = 1;
    uidValidityA = 1;
    uidMapA << 1;
    uidNextA = 2;
    helperSyncAWithMessagesEmptyState();
    idxA = model->index(1, 0, QModelIndex());
    idxB = model->index(2, 0, QModelIndex());
    QVERIFY(idxA.isValid());
    QVERIFY(idxB.isValid());
    QCOMPARE(model->data(idxA, Qt::DisplayRole), QVariant(QLatin1String("a")));
    QCOMPARE(model->data(idxB, Qt::DisplayRole), QVariant(QLatin1String("b")));
    msgListA = idxA.child(0, 0);
    QVERIFY(msgListA.isValid());
    QModelIndex msg = msgListA.child(0, 0);
    QVERIFY(msg.isValid());
    Streams::FakeSocket *origSocket = SOCK;

    // Switch the connection to an offline mode, but postpone the BYE response
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_OFFLINE);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), t.mk("LOGOUT\r\n"));

    // Ask for the bodystructure of this message
    model->markMessagesDeleted(QModelIndexList() << msg, Imap::Mailbox::FLAG_ADD);

    // Make sure that nothing else happens
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QVERIFY(SOCK == origSocket);
}

/** @short Test what happens when we switch to offline after the flag update request, but before the underlying task gets activated */
void ImapModelDisappearingMailboxTest::testSlowOfflineFlags2()
{
    // Initialize the environment
    existsA = 1;
    uidValidityA = 1;
    uidMapA << 1;
    uidNextA = 2;
    helperSyncAWithMessagesEmptyState();
    idxA = model->index(1, 0, QModelIndex());
    idxB = model->index(2, 0, QModelIndex());
    QVERIFY(idxA.isValid());
    QVERIFY(idxB.isValid());
    QCOMPARE(model->data(idxA, Qt::DisplayRole), QVariant(QLatin1String("a")));
    QCOMPARE(model->data(idxB, Qt::DisplayRole), QVariant(QLatin1String("b")));
    msgListA = idxA.child(0, 0);
    QVERIFY(msgListA.isValid());
    QModelIndex msg = msgListA.child(0, 0);
    QVERIFY(msg.isValid());
    Streams::FakeSocket *origSocket = SOCK;

    // Ask for the bodystructure of this message
    model->markMessagesDeleted(QModelIndexList() << msg, Imap::Mailbox::FLAG_ADD);

    // Switch the connection to an offline mode, but postpone the BYE response
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_OFFLINE);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), t.mk("LOGOUT\r\n"));

    // Make sure that nothing else happens
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QVERIFY(SOCK == origSocket);

}

/** @short Test what happens when we switch to offline after the flag update request and the task got activated, but before the tagged response */
void ImapModelDisappearingMailboxTest::testSlowOfflineFlags3()
{
    // Initialize the environment
    existsA = 1;
    uidValidityA = 1;
    uidMapA << 1;
    uidNextA = 2;
    helperSyncAWithMessagesEmptyState();
    idxA = model->index(1, 0, QModelIndex());
    idxB = model->index(2, 0, QModelIndex());
    QVERIFY(idxA.isValid());
    QVERIFY(idxB.isValid());
    QCOMPARE(model->data(idxA, Qt::DisplayRole), QVariant(QLatin1String("a")));
    QCOMPARE(model->data(idxB, Qt::DisplayRole), QVariant(QLatin1String("b")));
    msgListA = idxA.child(0, 0);
    QVERIFY(msgListA.isValid());
    QModelIndex msg = msgListA.child(0, 0);
    QVERIFY(msg.isValid());
    Streams::FakeSocket *origSocket = SOCK;

    // Ask for the bodystructure of this message
    model->markMessagesDeleted(QModelIndexList() << msg, Imap::Mailbox::FLAG_ADD);

    // Switch the connection to an offline mode, but postpone the BYE response
    QCoreApplication::processEvents();
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_OFFLINE);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QByteArray writtenStuff = t.mk("UID STORE 1 +FLAGS (\\Deleted)\r\n");
    writtenStuff += t.mk("LOGOUT\r\n");
    QCOMPARE(SOCK->writtenStuff(), writtenStuff);

    // Make sure that nothing else happens
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QVERIFY(SOCK == origSocket);
}

void ImapModelDisappearingMailboxTest::testMailboxHoping()
{
    int mailboxes = model->rowCount(QModelIndex());
    // The "off-by-one" is intentional, the first item is TreeItemMsgList
    for (int i = 1; i < mailboxes; ++i) {
        QModelIndex mailboxIndex = model->index(i, 0, QModelIndex());
        model->switchToMailbox(mailboxIndex);
    }
    //Imap::Mailbox::dumpModelContents(model->taskModel());
    cClient(t.mk("SELECT a\r\n"));
    cEmpty();
    cServer("* 0 EXISTS\r\n* OK [UIDNEXT 0] x\r\n* OK [UIDVALIDITY 1] x\r\n");
    cEmpty();
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("SELECT b\r\n"));
    cServer(t.last("OK B selected\r\n"));
    //Imap::Mailbox::dumpModelContents(model->taskModel());
    cClient(t.mk("SELECT c\r\n"));
    cEmpty();
}

// FIXME: write test for the UnSelectTask and its interaction with different scenarios about opened/to-be-opened tasks
// Redmine #486

TROJITA_HEADLESS_TEST( ImapModelDisappearingMailboxTest )
