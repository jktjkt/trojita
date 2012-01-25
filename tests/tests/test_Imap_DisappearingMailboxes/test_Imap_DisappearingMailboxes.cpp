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

#include <QtTest>
#include "test_Imap_DisappearingMailboxes.h"
#include "../headless_test.h"
#include "Streams/FakeSocket.h"
#include "test_LibMailboxSync/FakeCapabilitiesInjector.h"

/** @short This test verifies that we don't segfault during offline -> online transition */
void ImapModelDisappearingMailboxTest::testGoingOfflineOnline()
{
    helperSyncBNoMessages();
    model->setNetworkOffline();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), t.mk("LOGOUT\r\n"));
    model->setNetworkOnline();
    t.reset();

    // We can't call helperSyncBNoMessages() here, it relies on msgListB's validity,
    // but that index is not neccessary valid because of our kludgy fake listing...

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
    QPointer<Imap::Socket> socketPtr(factory->lastSocket());
    Q_ASSERT(!socketPtr.isNull());

    // Go offline
    model->setNetworkOffline();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QCOMPARE(SOCK->writtenStuff(), t.mk("LOGOUT\r\n"));
    SOCK->fakeReading(QByteArray("* BYE see ya\r\n")
                      + t.last("ok logged out\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // It should be gone by now
    QVERIFY(socketPtr.isNull());

    // So now we're offline and want to reconnect back to see if we break.

    // Try a reconnect
    taskFactoryUnsafe->fakeListChildMailboxes = false;
    t.reset();
    model->setNetworkOnline();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // The DelayedAskForChildrenOfMailbox::askNow runs at this point
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
    QCOMPARE( model->data(idxA, Qt::DisplayRole), QVariant(QString::fromAscii("a")));
    QCOMPARE( model->data(idxB, Qt::DisplayRole), QVariant(QString::fromAscii("b")));
    msgListA = idxA.child(0, 0);
    msgListB = idxB.child(0, 0);
    QVERIFY(msgListA.isValid());
    QVERIFY(msgListB.isValid());

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
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
    model->reloadMailboxList();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    SOCK->fakeReading(QByteArray("* 666 FETCH (FLAGS ())\r\n"));
    // FIXME: this will assert, the KeepMailboxOpenTask hasn't been fixed yet
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
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
    QCOMPARE(model->data(idxA, Qt::DisplayRole), QVariant(QString::fromAscii("a")));
    msgListA = idxA.child(0, 0);
    QVERIFY(msgListA.isValid());
    QModelIndex msg = msgListA.child(0, 0);
    QVERIFY(msg.isValid());
    Imap::FakeSocket *origSocket = SOCK;

    // Switch the connection to an offline mode, but postpone the BYE response
    model->setNetworkOffline();
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

TROJITA_HEADLESS_TEST( ImapModelDisappearingMailboxTest )
