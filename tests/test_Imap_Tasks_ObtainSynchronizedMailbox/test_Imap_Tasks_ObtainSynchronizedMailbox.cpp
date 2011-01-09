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

/** @short Class for persuading the model that the parser supports certain capabilities */
class FakeCapabilitiesInjector
{
public:
    FakeCapabilitiesInjector(Imap::Mailbox::Model *_model): model(_model)
    {}

    /** @short Add the specified capability to the list of capabilities "supported" by the server */
    void injectCapability(const QString& cap)
    {
        Q_ASSERT(!model->_parsers.isEmpty());
        model->updateCapabilities( model->_parsers.begin().key(), model->capabilities() << cap );
    }
private:
    Imap::Mailbox::Model *model;
};

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
    idxA = model->index( 1, 0, QModelIndex() );
    idxB = model->index( 2, 0, QModelIndex() );
    QCOMPARE( model->data( idxA, Qt::DisplayRole ), QVariant(QString::fromAscii("a")) );
    QCOMPARE( model->data( idxB, Qt::DisplayRole ), QVariant(QString::fromAscii("b")) );
    msgListA = model->index( 0, 0, idxA );
    msgListB = model->index( 0, 0, idxB );
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );
    t.reset();
    existsA = 0;
    uidNextA = 0;
    uidValidityA = 0;
    uidMapA.clear();
}

void ImapModelObtainSynchronizedMailboxTest::cleanup()
{
    delete model;
    model = 0;
    taskFactoryUnsafe = 0;
    QVERIFY( errorSpy->isEmpty() );
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


/** @short Helper: simulate sync of mailbox A that contains some messages from an empty state */
void ImapModelObtainSynchronizedMailboxTest::helperSyncAWithMessagesEmptyState()
{
    // Ask the model to sync stuff
    QCOMPARE( model->rowCount( msgListA ), 0 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    helperSyncAFullSync();
}

/** @short Helper: perform a full sync of the mailbox A */
void ImapModelObtainSynchronizedMailboxTest::helperSyncAFullSync()
{
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );

    helperFakeExistsUidValidityUidNext();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Verify that we indeed received what we wanted
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( ! list->fetched() );

    helperFakeUidSearch();

    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( msgListA ), static_cast<int>( existsA ) );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
    QVERIFY( errorSpy->isEmpty() );

    helperSyncFlags();

    // No errors
    if ( ! errorSpy->isEmpty() )
        qDebug() << errorSpy->first();
    QVERIFY( errorSpy->isEmpty() );

    helperCheckCache();

    QVERIFY( list->fetched() );

    // and the first mailbox is fully synced now.
}

void ImapModelObtainSynchronizedMailboxTest::helperFakeExistsUidValidityUidNext()
{
    // Try to feed it with absolute minimum data
    QByteArray buf;
    QTextStream ss( &buf );
    ss << "* " << existsA << " EXISTS\r\n";
    ss << "* OK [UIDVALIDITY " << uidValidityA << "] UIDs valid\r\n";
    ss << "* OK [UIDNEXT " << uidNextA << "] Predicted next UID\r\n";
    ss.flush();
    SOCK->fakeReading( buf + t.last("OK [READ-WRITE] Select completed.\r\n") );
}

void ImapModelObtainSynchronizedMailboxTest::helperFakeUidSearch( uint start )
{
    Q_ASSERT( start < existsA );
    if ( start == 0  ) {
        QCOMPARE( SOCK->writtenStuff(), t.mk("UID SEARCH ALL\r\n") );
    } else {
        QCOMPARE( SOCK->writtenStuff(), t.mk("UID SEARCH UID ") + QString::number( uidMapA[ start ] ).toAscii() + QByteArray(":*\r\n") );
    }

    QByteArray buf;
    QTextStream ss( &buf );
    ss << "* SEARCH";
    for ( uint i = start; i < existsA; ++i ) {
        ss << " " << uidMapA[i];
    }
    ss << "\r\n";
    ss.flush();
    SOCK->fakeReading( buf + t.last("OK search\r\n") );
}

/** @short Helper: make the parser switch to mailbox B which is actually empty

This function is useful for making sure that the parser switches to another mailbox and will perform
a fresh SELECT when it goes back to the original mailbox.
*/
void ImapModelObtainSynchronizedMailboxTest::helperSyncBNoMessages()
{
    // Try to go to second mailbox
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

/** @short Simulates what happens when mailbox A gets opened again, assuming that nothing has changed since the last time etc */
void ImapModelObtainSynchronizedMailboxTest::helperSyncAWithMessagesNoArrivals()
{
    // assume we've got some messages from the last case
    QCOMPARE( model->rowCount( msgListA ), static_cast<int>(existsA) );
    model->switchToMailbox( idxA );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );

    helperFakeExistsUidValidityUidNext();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Verify that we indeed received what we wanted
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->fetched() );

    QCOMPARE( model->rowCount( msgListA ), static_cast<int>(existsA) );
    QVERIFY( errorSpy->isEmpty() );

    helperSyncFlags();

    // No errors
    if ( ! errorSpy->isEmpty() )
        qDebug() << errorSpy->first();
    QVERIFY( errorSpy->isEmpty() );

    helperCheckCache();

    QVERIFY( list->fetched() );
}

/** @short Simulates fetching flags for messages 1:$exists */
void ImapModelObtainSynchronizedMailboxTest::helperSyncFlags()
{
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("FETCH 1:") + QString::number(existsA).toAscii() + QByteArray(" (FLAGS)\r\n") );
    QByteArray buf;
    for ( uint i = 1; i <= existsA; ++i ) {
        if ( i % 2 )
            buf += QString::fromAscii("* %1 FETCH (FLAGS (\\Seen))\r\n").arg(i).toAscii();
        else
            buf += QString::fromAscii("* %1 FETCH (FLAGS ())\r\n").arg(i).toAscii();
    }
    SOCK->fakeReading( buf + t.last("OK yay\r\n") );
    QCoreApplication::processEvents();
}

/** @short Helper: update flags for some message */
void ImapModelObtainSynchronizedMailboxTest::helperOneFlagUpdate( const QModelIndex &message )
{
    SOCK->fakeReading( QString::fromAscii("* %1 FETCH (FLAGS (\\Seen foo bar))\r\n").arg( message.row() + 1 ).toAscii() );
    QCoreApplication::processEvents();
    QVERIFY( message.data( Imap::Mailbox::RoleMessageFlags ).toStringList() == QStringList() << QLatin1String("\\Seen") << QLatin1String("foo") << QLatin1String("bar") );
    QVERIFY(errorSpy->isEmpty());
}

void ImapModelObtainSynchronizedMailboxTest::helperSyncASomeNew( int number )
{
    QCOMPARE( model->rowCount( msgListA ), static_cast<int>(existsA) );
    model->switchToMailbox( idxA );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );

    uint oldExistsA = existsA;
    for ( int i = 0; i < number; ++i ) {
        ++existsA;
        uidMapA.append( uidNextA );
        ++uidNextA;
    }
    helperFakeExistsUidValidityUidNext();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Verify that we indeed received what we wanted
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( ! list->fetched() );

    helperFakeUidSearch( oldExistsA );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY( list->fetched() );

    QCOMPARE( model->rowCount( msgListA ), static_cast<int>( existsA ) );
    QVERIFY( errorSpy->isEmpty() );

    helperSyncFlags();

    // No errors
    if ( ! errorSpy->isEmpty() )
        qDebug() << errorSpy->first();
    QVERIFY( errorSpy->isEmpty() );

    helperCheckCache();

    QVERIFY( list->fetched() );
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

/** @short Helper: make sure that UID mapping is correct */
void ImapModelObtainSynchronizedMailboxTest::helperVerifyUidMapA()
{
    QCOMPARE( model->rowCount( msgListA ), static_cast<int>(existsA) );
    Q_ASSERT( static_cast<int>(existsA) == uidMapA.size() );
    for ( int i = 0; i < uidMapA.size(); ++i ) {
        QModelIndex message = model->index( i, 0, msgListA );
        Q_ASSERT( message.isValid() );
        QCOMPARE( message.data( Imap::Mailbox::RoleMessageUid ).toUInt(), uidMapA[i] );
    }
}

/** @short Helper: verify that values recorded in the cache are valid */
void ImapModelObtainSynchronizedMailboxTest::helperCheckCache()
{
    // Check the cache
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QString::fromAscii("a") );
    QCOMPARE( syncState.exists(), existsA );
    QCOMPARE( syncState.isUsableForSyncing(), true );
    QCOMPARE( syncState.uidNext(), uidNextA );
    QCOMPARE( syncState.uidValidity(), uidValidityA );
    QCOMPARE( model->cache()->uidMapping( QString::fromAscii("a") ), uidMapA );


    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QVERIFY( errorSpy->isEmpty() );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
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

/** @short Test a NO reply to IDLE command */
void ImapModelObtainSynchronizedMailboxTest::testIdleNo()
{
    model->setProperty("trojita-imap-idle-delayedEnter", QVariant(30));
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QLatin1String("IDLE"));
    existsA = 3;
    uidValidityA = 6;
    uidMapA << 1 << 7 << 9;
    uidNextA = 16;
    helperSyncAWithMessagesEmptyState();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QTest::qWait(40);
    QCOMPARE( SOCK->writtenStuff(), t.mk("IDLE\r\n") );
    SOCK->fakeReading(t.last("NO you can't idle now\r\n"));
    QTest::qWait(40);
    QVERIFY(SOCK->writtenStuff().isEmpty());
    helperSyncBNoMessages();
    QVERIFY(errorSpy->isEmpty());
}

/** @short Test what happens when IDLE terminates by an OK, but without our "DONE" input */
void ImapModelObtainSynchronizedMailboxTest::testIdleImmediateReturn()
{
    model->setProperty("trojita-imap-idle-delayedEnter", QVariant(30));
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QLatin1String("IDLE"));
    existsA = 3;
    uidValidityA = 6;
    uidMapA << 1 << 7 << 9;
    uidNextA = 16;
    helperSyncAWithMessagesEmptyState();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QTest::qWait(40);
    QCOMPARE( SOCK->writtenStuff(), t.mk("IDLE\r\n") );
    SOCK->fakeReading(QByteArray("+ blah\r\n") + t.last("OK done\r\n"));
    QTest::qWait(40);
    QCOMPARE( SOCK->writtenStuff(), t.mk("IDLE\r\n") );
}

/** @short Test automatic IDLE renewal */
void ImapModelObtainSynchronizedMailboxTest::testIdleRenewal()
{
    model->setProperty("trojita-imap-idle-delayedEnter", QVariant(30));
    model->setProperty("trojita-imap-idle-renewal", QVariant(10));
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QLatin1String("IDLE"));
    existsA = 3;
    uidValidityA = 6;
    uidMapA << 1 << 7 << 9;
    uidNextA = 16;
    helperSyncAWithMessagesEmptyState();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QTest::qWait(40);
    QCOMPARE( SOCK->writtenStuff(), t.mk("IDLE\r\n") );
    SOCK->fakeReading(QByteArray("+ blah\r\n"));
    QTest::qWait(10);
    QCOMPARE( SOCK->writtenStuff(), QByteArray("DONE\r\n") );
    SOCK->fakeReading(t.last("OK done\r\n"));
    QTest::qWait(40);
    QCOMPARE( SOCK->writtenStuff(), t.mk("IDLE\r\n") );
}

/** @short Test that IDLE gets immediately interrupted by any Task */
void ImapModelObtainSynchronizedMailboxTest::testIdleBreakTask()
{
    model->setProperty("trojita-imap-idle-delayedEnter", QVariant(30));
    // Intentionally leave trojita-imap-idle-renewal at its rather high default value
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QLatin1String("IDLE"));
    existsA = 3;
    uidValidityA = 6;
    uidMapA << 1 << 7 << 9;
    uidNextA = 16;
    helperSyncAWithMessagesEmptyState();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QTest::qWait(40);
    QCOMPARE( SOCK->writtenStuff(), t.mk("IDLE\r\n") );
    SOCK->fakeReading(QByteArray("+ blah\r\n"));
    QCOMPARE( msgListA.child(0,0).data(Imap::Mailbox::RoleMessageFrom).toString(), QString() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("DONE\r\n") + t.mk("UID FETCH 1,7,9 (ENVELOPE BODYSTRUCTURE RFC822.SIZE)\r\n") );
    SOCK->fakeReading(t.last("OK done\r\n"));
    QTest::qWait(40);
    QVERIFY(SOCK->writtenStuff().isEmpty());
}

/** @short Test automatic IDLE renewal when server gets really slow to respond */
void ImapModelObtainSynchronizedMailboxTest::testIdleSlowResponses()
{
    model->setProperty("trojita-imap-idle-delayedEnter", QVariant(30));
    model->setProperty("trojita-imap-idle-renewal", QVariant(10));
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QLatin1String("IDLE"));
    existsA = 3;
    uidValidityA = 6;
    uidMapA << 1 << 7 << 9;
    uidNextA = 16;
    helperSyncAWithMessagesEmptyState();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QTest::qWait(40);
    QCOMPARE( SOCK->writtenStuff(), t.mk("IDLE\r\n") );
    QTest::qWait(70);
    SOCK->fakeReading(QByteArray("+ blah\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("DONE\r\n") );
    SOCK->fakeReading(t.last("OK done\r\n"));
    QTest::qWait(40);
    QCOMPARE( SOCK->writtenStuff(), t.mk("IDLE\r\n") );
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
