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
#include "test_Imap_Tasks_ObtainSynchronizedMailbox.h"
#include "../headless_test.h"
#include "test_LibMailboxSync/FakeCapabilitiesInjector.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Tasks/ObtainSynchronizedMailboxTask.h"

#define SOCK static_cast<Imap::FakeSocket*>( factory->lastSocket() )

/** Verify syncing of an empty mailbox with just the EXISTS response

Verify that we can synchronize a mailbox which is empty even if the server
ommits most of the required replies and sends us just the EXISTS one. Also
check the cache for valid state.
*/
void ImapModelObtainSynchronizedMailboxTest::testSyncEmptyMinimal()
{
    model->setProperty( "trojita-imap-noop-period", 10 );

    // Ask the model to sync stuff
    QCOMPARE( model->rowCount( msgListA ), 0 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );

    // Try to feed it with absolute minimum data
    SOCK->fakeReading( QByteArray("* 0 exists\r\n")
                                  + t.last("OK done\r\n") );
    QCoreApplication::processEvents();

    // Verify that we indeed received what we wanted
    QCOMPARE( model->rowCount( msgListA ), 0 );
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->fetched() );
    //QTest::qWait( 100 );
    QVERIFY( SOCK->writtenStuff().isEmpty() );

    // Now, let's try to re-sync it once again; the difference is that our cache now has "something"
    model->resyncMailbox( idxA );
    QCoreApplication::processEvents();

    // Verify that it indeed caused a re-synchronization
    list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->loading() );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );
    SOCK->fakeReading( QByteArray("* 0 exists\r\n")
                                  + t.last("OK done\r\n") );
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );

    // Check the cache
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QString::fromAscii("a") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.flags(), QStringList() );
    QCOMPARE( syncState.isComplete(), false );
    QCOMPARE( syncState.isUsableForSyncing(), false );
    QCOMPARE( syncState.permanentFlags(), QStringList() );
    QCOMPARE( syncState.recent(), 0u );
    QCOMPARE( syncState.uidNext(), 0u );
    QCOMPARE( syncState.uidValidity(), 0u );
    QCOMPARE( syncState.unSeen(), 0u );

    // No errors
    QVERIFY( errorSpy->isEmpty() );
}

/** @short Verify synchronization of an empty mailbox against a compliant IMAP4rev1 server

This test verifies that we handle a compliant server's responses when we sync an empty mailbox.
A check of the state of the cache after is completed, too.
*/
void ImapModelObtainSynchronizedMailboxTest::testSyncEmptyNormal()
{
    // Ask the model to sync stuff
    QCOMPARE( model->rowCount( msgListA ), 0 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );

    // Try to feed it with absolute minimum data
    SOCK->fakeReading( QByteArray("* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n"
                                  "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft \\*)] Flags permitted.\r\n"
                                  "* 0 EXISTS\r\n"
                                  "* 0 RECENT\r\n"
                                  "* OK [UIDVALIDITY 666] UIDs valid\r\n"
                                  "* OK [UIDNEXT 3] Predicted next UID\r\n")
                                  + t.last("OK [READ-WRITE] Select completed.\r\n") );
    QCoreApplication::processEvents();

    // Verify that we indeed received what we wanted
    QCOMPARE( model->rowCount( msgListA ), 0 );
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->fetched() );
    QVERIFY( SOCK->writtenStuff().isEmpty() );

    // Check the cache
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QString::fromAscii("a") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.flags(), QStringList() << QString::fromAscii("\\Answered") <<
              QString::fromAscii("\\Flagged") << QString::fromAscii("\\Deleted") <<
              QString::fromAscii("\\Seen") << QString::fromAscii("\\Draft") );
    QCOMPARE( syncState.isComplete(), false );
    QCOMPARE( syncState.isUsableForSyncing(), true );
    QCOMPARE( syncState.permanentFlags(), QStringList() << QString::fromAscii("\\Answered") <<
              QString::fromAscii("\\Flagged") << QString::fromAscii("\\Deleted") <<
              QString::fromAscii("\\Seen") << QString::fromAscii("\\Draft") << QString::fromAscii("\\*") );
    QCOMPARE( syncState.recent(), 0u );
    QCOMPARE( syncState.uidNext(), 3u );
    QCOMPARE( syncState.uidValidity(), 666u );
    QCOMPARE( syncState.unSeen(), 0u );

    // Now, let's try to re-sync it once again; the difference is that our cache now has "something"
    // and that we feed it with a rather limited set of responses
    model->resyncMailbox( idxA );
    QCoreApplication::processEvents();

    // Verify that it indeed caused a re-synchronization
    list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->loading() );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );
    SOCK->fakeReading( QByteArray("* 0 exists\r\n* NO a random no in inserted here\r\n")
                                  + t.last("OK done\r\n") );
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );

    // Check the cache; now it should be almost empty
    syncState = model->cache()->mailboxSyncState( QString::fromAscii("a") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.flags(), QStringList() );
    QCOMPARE( syncState.isComplete(), false );
    QCOMPARE( syncState.isUsableForSyncing(), false );
    QCOMPARE( syncState.permanentFlags(), QStringList() );
    QCOMPARE( syncState.recent(), 0u );

    // No errors
    QVERIFY( errorSpy->isEmpty() );
}

/** @short Initial sync of a mailbox that contains some messages */
void ImapModelObtainSynchronizedMailboxTest::testSyncWithMessages()
{
    existsA = 33;
    uidValidityA = 333666;
    for ( uint i = 1; i <= existsA; ++i ) {
        uidMapA.append( i * 1.3 );
    }
    uidNextA = qMax( 666u, uidMapA.last() );
    helperSyncAWithMessagesEmptyState();
    helperVerifyUidMapA();
}

/** @short Go back to a selected mailbox after some time, the mailbox doesn't have any modifications */
void ImapModelObtainSynchronizedMailboxTest::testResyncNoArrivals()
{
    existsA = 42;
    uidValidityA = 1337;
    for ( uint i = 1; i <= existsA; ++i ) {
        uidMapA.append( 6 + i * 3.6 );
    }
    uidNextA = qMax( 666u, uidMapA.last() );
    helperSyncAWithMessagesEmptyState();
    helperSyncBNoMessages();
    helperSyncAWithMessagesNoArrivals();
    helperSyncBNoMessages();
    helperSyncAWithMessagesNoArrivals();
    helperVerifyUidMapA();
    helperOneFlagUpdate( idxA.child( 0,0 ).child( 10, 0 ) );
}

/** @short Test new message arrivals happening on each resync */
void ImapModelObtainSynchronizedMailboxTest::testResyncOneNew()
{
    existsA = 17;
    uidValidityA = 800500;
    for ( uint i = 1; i <= existsA; ++i ) {
        uidMapA.append( 3 + i * 1.3 );
    }
    uidNextA = qMax( 666u, uidMapA.last() );
    helperSyncAWithMessagesEmptyState();
    helperSyncBNoMessages();
    helperSyncASomeNew( 1 );
    helperVerifyUidMapA();
    helperSyncBNoMessages();
    helperSyncASomeNew( 3 );
    helperVerifyUidMapA();
}

/** @short Test inconsistency in the local cache where UIDNEXT got decreased without UIDVALIDITY change */
void ImapModelObtainSynchronizedMailboxTest::testDecreasedUidNext()
{
    // Initial state
    existsA = 3;
    uidValidityA = 333666;
    for ( uint i = 1; i <= existsA; ++i ) {
        uidMapA.append(i);
    }
    uidNextA = uidMapA.last();
    helperSyncAWithMessagesEmptyState();
    helperVerifyUidMapA();
    helperSyncBNoMessages();

    // Now perform the nasty change...
    --existsA;
    uidMapA.removeLast();
    --uidNextA;

    // ...and resync again, this should scream loud, but not crash
    QCOMPARE( model->rowCount( msgListA ), 3 );
    model->switchToMailbox( idxA );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    helperSyncAFullSync();
}


#if 0
{
    // First re-sync: one message added, nothing else changed
    model->resyncMailbox( idxA );
    QCoreApplication::processEvents();
    list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgList.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->loading() );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y3 SELECT a\r\n") );

    SOCK->fakeReading( QByteArray("* 18 EXISTS\r\n"
                                  "* OK [UIDVALIDITY 1226524607] UIDs valid\r\n"
                                  "* OK [UIDNEXT 19] Predicted next UID\r\n"
                                  "y3 OK [READ-WRITE] Select completed.\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QCOMPARE( SOCK->writtenStuff(), QByteArray("y4 UID SEARCH UID 18:*\r\n") );
    SOCK->fakeReading( "* SEARCH 18\r\n"
                       "y4 OK johoho\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QCOMPARE( model->rowCount( msgList ), 18 );
    QVERIFY( errorSpy->isEmpty() );*/



    // FIXME: work work work
    QVERIFY( errorSpy->isEmpty() );
    return;
    // FIXME: add unit tests for different scenarios than the "initial one"


    // Again, re-sync and chech that everything's gone from the cache
    model->resyncMailbox( idxA );
    QCoreApplication::processEvents();

    // Verify that it indeed caused a re-synchronization
    list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgList.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->loading() );
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );
    SOCK->fakeReading( QByteArray("* 0 exists\r\n")
                                  +t.last("OK done\r\n") );
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );

    // Check the cache; now it should be almost empty
    syncState = model->cache()->mailboxSyncState( QString::fromAscii("a") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.flags(), QStringList() );
    QCOMPARE( syncState.isComplete(), false );
    QCOMPARE( syncState.isUsableForSyncing(), false );
    QCOMPARE( syncState.permanentFlags(), QStringList() );
    QCOMPARE( syncState.recent(), 0u );

    // No errors
    QVERIFY( errorSpy->isEmpty() );
}
#endif

/**
Test that going from an empty mailbox to a bigger one works correctly, especially that the untagged
EXISTS response which belongs to the SELECT gets picked up by the new mailbox and not the old one
*/
void ImapModelObtainSynchronizedMailboxTest::testSyncTwoLikeCyrus()
{
    // Ask the model to sync stuff
    QCOMPARE( model->rowCount( msgListB ), 0 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT b\r\n") );

    SOCK->fakeReading( QByteArray("* 0 EXISTS\r\n"
                                  "* 0 RECENT\r\n"
                                  "* FLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen)\r\n"
                                  "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen \\*)] Ok\r\n"
                                  "* OK [UIDVALIDITY 1290594339] Ok\r\n"
                                  "* OK [UIDNEXT 1] Ok\r\n"
                                  "* OK [HIGHESTMODSEQ 1] Ok\r\n"
                                  "* OK [URLMECH INTERNAL] Ok\r\n")
                       + t.last("OK [READ-WRITE] Completed\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Verify that we indeed received what we wanted
    Imap::Mailbox::TreeItemMsgList* listB = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListB.internalPointer() ) );
    Q_ASSERT( listB );
    QVERIFY( listB->fetched() );

    QCOMPARE( model->rowCount( msgListB ), 0 );
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );
    QVERIFY( errorSpy->isEmpty() );

    QCOMPARE( model->rowCount( msgListA ), 0 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );

    SOCK->fakeReading( QByteArray("* 1 EXISTS\r\n"
                                  "* 0 RECENT\r\n"
                                  "* FLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen hasatt hasnotd)\r\n"
                                  "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen hasatt hasnotd \\*)] Ok\r\n"
                                  "* OK [UIDVALIDITY 1290593745] Ok\r\n"
                                  "* OK [UIDNEXT 2] Ok\r\n"
                                  "* OK [HIGHESTMODSEQ 9] Ok\r\n"
                                  "* OK [URLMECH INTERNAL] Ok\r\n")
                       + t.last("OK [READ-WRITE] Completed") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    Imap::Mailbox::TreeItemMsgList* listA = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( listA );
    QVERIFY( ! listA->fetched() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    //SOCK->fakeReading( QByteArray("* 1 FETCH (FLAGS (\\Seen hasatt hasnotd) UID 1 RFC822.SIZE 13021)\r\n")
    //                   + t.last("OK fetch completed\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );
    QVERIFY( errorSpy->isEmpty() );
}

void ImapModelObtainSynchronizedMailboxTest::testSyncTwoInParallel()
{
    // This will select both mailboxes, one after another
    QCOMPARE( model->rowCount( msgListA ), 0 );
    QCOMPARE( model->rowCount( msgListB ), 0 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );
    SOCK->fakeReading( QByteArray("* 1 EXISTS\r\n"
                                  "* 0 RECENT\r\n"
                                  "* FLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen hasatt hasnotd)\r\n"
                                  "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen hasatt hasnotd \\*)] Ok\r\n"
                                  "* OK [UIDVALIDITY 1290593745] Ok\r\n"
                                  "* OK [UIDNEXT 2] Ok\r\n"
                                  "* OK [HIGHESTMODSEQ 9] Ok\r\n"
                                  "* OK [URLMECH INTERNAL] Ok\r\n")
                       + t.last("OK [READ-WRITE] Completed\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("UID SEARCH ALL\r\n") );
    QCOMPARE( model->rowCount( msgListA ), 0 );
    QCOMPARE( model->rowCount( msgListB ), 0 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( msgListA ), 0 );
    QCOMPARE( model->rowCount( msgListB ), 0 );
    // ...none of them are synced yet

    SOCK->fakeReading( QByteArray("* SEARCH 1\r\n")
                       + t.last("OK Completed (1 msgs in 0.000 secs)\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( msgListA ), 1 );
    // the first one should contain a message now

    QCOMPARE( SOCK->writtenStuff(), QByteArray(t.mk("FETCH 1 (FLAGS)\r\n")) );
    SOCK->fakeReading( QByteArray("* 1 FETCH (FLAGS (\\Seen hasatt hasnotd))\r\n")
                       + t.last("OK Completed (0.000 sec)\r\n"));
    QModelIndex msg1A = model->index( 0, 0, msgListA );

    // Requesting message data should delay entering the second mailbox
    QCOMPARE( model->data( msg1A, Imap::Mailbox::RoleMessageMessageId ), QVariant() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( msgListA ), 1 );
    QCoreApplication::processEvents();

    QCOMPARE( SOCK->writtenStuff(), t.mk(QByteArray("UID FETCH 1 (ENVELOPE BODYSTRUCTURE RFC822.SIZE)\r\n")) );
    // let's try to get around without specifying ENVELOPE and BODYSTRUCTURE
    SOCK->fakeReading( QByteArray("* 1 FETCH (UID 1 RFC822.SIZE 13021)\r\n")
                       + t.last("OK Completed\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QCOMPARE( SOCK->writtenStuff(), t.mk(QByteArray("SELECT b\r\n")));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QVERIFY( SOCK->writtenStuff().isEmpty() );
    QVERIFY( errorSpy->isEmpty() );
}

/** @short Test whether a change in the UIDVALIDITY results in a complete resync */
void ImapModelObtainSynchronizedMailboxTest::testResyncUidValidity()
{
    existsA = 42;
    uidValidityA = 1337;
    for ( uint i = 1; i <= existsA; ++i ) {
        uidMapA.append( 6 + i * 3.6 );
    }
    uidNextA = qMax( 666u, uidMapA.last() );
    helperSyncAWithMessagesEmptyState();

    // Change UIDVALIDITY
    uidValidityA = 333666;
    helperSyncBNoMessages();

    QCOMPARE( model->rowCount( msgListA ), 42 );
    model->switchToMailbox( idxA );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    helperSyncAFullSync();
}

#if 0
void ImapModelObtainSynchronizedMailboxTest::testBenchmarkParserModelInteraction()
{
    existsA = 100;
    uidValidityA = 1;
    for ( uint i = 1; i <= existsA; ++i ) {
        uidMapA.append( i );
    }
    uidNextA = qMax( 666u, uidMapA.last() );
    helperSyncAWithMessagesEmptyState();

    x = 0;
    connect(model, SIGNAL(logParserLineReceived(uint,QByteArray)), this, SLOT(gotLine()) );
    QByteArray foo;
    /* This is really interesting -- if we create just 10 objects here and Model::respnseReceived() and
    Model::slotParserLineReceived() use sender() to find out which object sent the corresponding signal,
    processing 100k responses takes roughly five seconds. Increasing the object count to 1000 and the
    loop runs 6 seconds, and when we work with 10k objects, the loop for processing responses takes 40
    seconds to complete.
    Switching the corresponding functions in Model not to call sender() improves performance in an
    extremely significant way, bringing the time back to roughly 5 seconds.
*/
    for ( int i = 0; i < 10000; ++i ) {
        QObject *foo = new QObject(model);
        connect(foo, SIGNAL(destroyed()), model, SLOT(slotTaskDying()));
    }
    for ( int i = 0; i < 100 * 1000 + 10; ++i ) {
        foo += "* OK ping pong\r\n";
    }
    qDebug() << "...starting...";
    ttt.restart();
    SOCK->fakeReading(foo);
    QCoreApplication::processEvents();
    qDebug() << "....and processed" << x << "items.";
}

void ImapModelObtainSynchronizedMailboxTest::gotLine()
{
    ++x;
    uint num = 100 * 1000;
    if ( x == num ) {
        qDebug() << "Time for" << num << "iterations:" << ttt.elapsed();
    }
}

#endif

TROJITA_HEADLESS_TEST( ImapModelObtainSynchronizedMailboxTest )
