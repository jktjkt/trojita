/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

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
#include "Streams/FakeSocket.h"
#include "Imap/Model/Cache.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Tasks/ObtainSynchronizedMailboxTask.h"

#define SOCK static_cast<Imap::FakeSocket*>( factory->lastSocket() )

void ImapModelObtainSynchronizedMailboxTest::init()
{
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache( this, QString() );
    factory = new Imap::Mailbox::FakeSocketFactory();
    Imap::Mailbox::TaskFactoryPtr taskFactory( new Imap::Mailbox::TestingTaskFactory() );
    taskFactoryUnsafe = static_cast<Imap::Mailbox::TestingTaskFactory*>( taskFactory.get() );
    taskFactoryUnsafe->fakeOpenConnectionTask = true;
    taskFactoryUnsafe->fakeListChildMailboxes = true;
    taskFactoryUnsafe->fakeListChildMailboxesMap[ QString::fromAscii("") ] = QStringList() << QString::fromAscii("a") << QString::fromAscii("b");
    model = new Imap::Mailbox::Model( this, cache, Imap::Mailbox::SocketFactoryPtr( factory ), taskFactory, false );
    errorSpy = new QSignalSpy( model, SIGNAL(connectionError(QString)) );

    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 3 );
    QModelIndex idxA = model->index( 1, 0, QModelIndex() );
    QModelIndex idxB = model->index( 2, 0, QModelIndex() );
    QCOMPARE( model->data( idxA, Qt::DisplayRole ), QVariant(QString::fromAscii("a")) );
    QCOMPARE( model->data( idxB, Qt::DisplayRole ), QVariant(QString::fromAscii("b")) );
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );
}

void ImapModelObtainSynchronizedMailboxTest::cleanup()
{
    delete model;
    model = 0;
    taskFactoryUnsafe = 0;
    delete errorSpy;
    errorSpy = 0;
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
}

void ImapModelObtainSynchronizedMailboxTest::cleanupTestCase()
{
}

void ImapModelObtainSynchronizedMailboxTest::initTestCase()
{
    model = 0;
    errorSpy = 0;
}

/** Verify syncing of an empty mailbox with just the EXISTS response

Verify that we can synchronize a mailbox which is empty even if the server
ommits most of the required replies and sends us just the EXISTS one. Also
check the cache for valid state.
*/
void ImapModelObtainSynchronizedMailboxTest::testSyncEmptyMinimal()
{
    TagGenerator t;
    model->setProperty( "trojita-imap-noop-period", 10 );
    // Boring stuff
    QModelIndex idxA = model->index( 1, 0, QModelIndex() );
    QModelIndex msgList = model->index( 0, 0, idxA );

    // Ask the model to sync stuff
    QCOMPARE( model->rowCount( msgList ), 0 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );

    // Try to feed it with absolute minimum data
    SOCK->fakeReading( QByteArray("* 0 exists\r\n")
                                  + t.last("OK done\r\n") );
    QCoreApplication::processEvents();

    // Verify that we indeed received what we wanted
    QCOMPARE( model->rowCount( msgList ), 0 );
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgList.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->fetched() );
    //QTest::qWait( 100 );
    QVERIFY( SOCK->writtenStuff().isEmpty() );

    // Now, let's try to re-sync it once again; the difference is that our cache now has "something"
    model->resyncMailbox( idxA );
    QCoreApplication::processEvents();

    // Verify that it indeed caused a re-synchronization
    list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgList.internalPointer() ) );
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
    TagGenerator t;
    // Boring stuff
    QModelIndex idxA = model->index( 1, 0, QModelIndex() );
    QModelIndex msgList = model->index( 0, 0, idxA );

    // Ask the model to sync stuff
    QCOMPARE( model->rowCount( msgList ), 0 );
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
    QCOMPARE( model->rowCount( msgList ), 0 );
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgList.internalPointer() ) );
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
    list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgList.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->loading() );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );
    SOCK->fakeReading( QByteArray("* 0 exists\r\n")
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

void ImapModelObtainSynchronizedMailboxTest::testSyncWithMessages()
{
    TagGenerator t;

    helperSyncAWithMessagesEmptyState( t );
    helperSyncBNoMessages( t );
}


void ImapModelObtainSynchronizedMailboxTest::helperSyncAWithMessagesEmptyState( TagGenerator &t )
{
    // Boring stuff
    QModelIndex idxA = model->index( 1, 0, QModelIndex() );
    QModelIndex msgList = model->index( 0, 0, idxA );

    // Ask the model to sync stuff
    QCOMPARE( model->rowCount( msgList ), 0 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );

    // Try to feed it with absolute minimum data
    SOCK->fakeReading( QByteArray("* 17 EXISTS\r\n"
                                  "* OK [UIDVALIDITY 1226524607] UIDs valid\r\n"
                                  "* OK [UIDNEXT 18] Predicted next UID\r\n")
                                  + t.last("OK [READ-WRITE] Select completed.\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Verify that we indeed received what we wanted
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgList.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( ! list->fetched() );
    QCOMPARE( SOCK->writtenStuff(), t.mk("UID SEARCH ALL\r\n") );

    SOCK->fakeReading( QByteArray("* SEARCH 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17\r\n")
                                  + t.last("OK search\r\n") );
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( msgList ), 17 );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
    QVERIFY( errorSpy->isEmpty() );

    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("FETCH 1:17 (FLAGS)\r\n") );
    SOCK->fakeReading( QByteArray("* 1 FETCH (FLAGS (\\Seen))\r\n"
                                  "* 2 FETCH (FLAGS (\\Seen))\r\n"
                                  "* 3 FETCH (FLAGS (\\Seen))\r\n"
                                  "* 4 FETCH (FLAGS (\\Seen))\r\n"
                                  "* 5 FETCH (FLAGS (\\Seen))\r\n"
                                  "* 6 FETCH (FLAGS (\\Seen))\r\n"
                                  "* 7 FETCH (FLAGS ())\r\n"
                                  "* 8 FETCH (FLAGS (\\Seen))\r\n"
                                  "* 9 FETCH (FLAGS (\\Seen))\r\n"
                                  "* 10 FETCH (FLAGS ())\r\n"
                                  "* 11 FETCH (FLAGS ())\r\n"
                                  "* 12 FETCH (FLAGS ())\r\n"
                                  "* 13 FETCH (FLAGS ())\r\n"
                                  "* 14 FETCH (FLAGS (\\Seen))\r\n"
                                  "* 15 FETCH (FLAGS (\\Seen))\r\n"
                                  "* 16 FETCH (FLAGS (\\Seen))\r\n"
                                  "* 17 FETCH (FLAGS (\\Seen))\r\n")
                                  + t.last("OK yay\r\n"));
    QCoreApplication::processEvents();

    // No errors
    if ( ! errorSpy->isEmpty() )
        qDebug() << errorSpy->first();
    QVERIFY( errorSpy->isEmpty() );

    // Check the cache
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QString::fromAscii("a") );
    QCOMPARE( syncState.exists(), 17u );
    QCOMPARE( syncState.isUsableForSyncing(), true );
    QCOMPARE( syncState.uidNext(), 18u );
    QCOMPARE( syncState.uidValidity(), 1226524607u );

    QVERIFY( list->fetched() );

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QVERIFY( errorSpy->isEmpty() );
    QVERIFY( SOCK->writtenStuff().isEmpty() );


    // and the first mailbox is fully synced now.
}

void ImapModelObtainSynchronizedMailboxTest::helperSyncBNoMessages( TagGenerator &t )
{
    // Try to go to second mailbox
    QModelIndex idxB = model->index( 2, 0, QModelIndex() );
    QModelIndex msgListB = model->index( 0, 0, idxB );

    QCOMPARE( model->rowCount( msgListB ), 0 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT b\r\n") );
    SOCK->fakeReading( QByteArray("* 0 exists\r\n")
                                  + t.last("ok completed\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Check the cache
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QString::fromAscii("b") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.isUsableForSyncing(), false );
    QCOMPARE( syncState.uidNext(), 0u );
    QCOMPARE( syncState.uidValidity(), 0u );

    // Verify that we indeed received what we wanted
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListB.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->fetched() );

    QVERIFY( errorSpy->isEmpty() );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
}

#if 0
{
    // And then go back to the first one
    model->switchToMailbox( idxA );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );


    /*
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
    TagGenerator t;
    // Boring stuff
    QModelIndex idxA = model->index( 1, 0, QModelIndex() );
    QModelIndex idxB = model->index( 2, 0, QModelIndex() );
    QModelIndex msgListA = model->index( 0, 0, idxA );
    QModelIndex msgListB = model->index( 0, 0, idxB );

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
    TagGenerator t;
    QModelIndex idxA = model->index( 1, 0, QModelIndex() );
    QModelIndex idxB = model->index( 2, 0, QModelIndex() );
    QModelIndex msgListA = model->index( 0, 0, idxA );
    QModelIndex msgListB = model->index( 0, 0, idxB );

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

TROJITA_HEADLESS_TEST( ImapModelObtainSynchronizedMailboxTest )
