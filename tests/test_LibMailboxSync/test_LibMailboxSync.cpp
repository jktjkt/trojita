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
#include "test_LibMailboxSync.h"
#include "../tests/headless_test.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/Cache.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/MailboxTree.h"

LibMailboxSync::~LibMailboxSync()
{
}

void LibMailboxSync::init()
{
    m_verbose = qgetenv("TROJITA_IMAP_DEBUG") == QByteArray("1");
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache( this, QString() );
    factory = new Imap::Mailbox::FakeSocketFactory();
    Imap::Mailbox::TaskFactoryPtr taskFactory( new Imap::Mailbox::TestingTaskFactory() );
    taskFactoryUnsafe = static_cast<Imap::Mailbox::TestingTaskFactory*>( taskFactory.get() );
    taskFactoryUnsafe->fakeOpenConnectionTask = true;
    taskFactoryUnsafe->fakeListChildMailboxes = true;
    taskFactoryUnsafe->fakeListChildMailboxesMap[ QString::fromAscii("") ] = QStringList() << QString::fromAscii("a") << QString::fromAscii("b");
    model = new Imap::Mailbox::Model( this, cache, Imap::Mailbox::SocketFactoryPtr( factory ), taskFactory, false );
    errorSpy = new QSignalSpy( model, SIGNAL(connectionError(QString)) );
    connect(model, SIGNAL(connectionError(QString)), this, SLOT(modelSignalsError(QString)));
    connect(model, SIGNAL(logged(uint,Imap::Mailbox::LogMessage)), this, SLOT(modelLogged(uint,Imap::Mailbox::LogMessage)));

    helperInitialListing();
}

void LibMailboxSync::modelSignalsError(const QString &message)
{
    qDebug() << message;
    QFAIL("Model emits an error");
}

void LibMailboxSync::modelLogged(uint parserId, const Imap::Mailbox::LogMessage &message)
{
    if (!m_verbose)
        return;

    qDebug() << "LOG" << parserId << message.source <<
                (message.message.endsWith(QLatin1String("\r\n")) ?
                     message.message.left(message.message.size() - 2) : message.message);
}

void LibMailboxSync::helperInitialListing()
{
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 3 );
    idxA = model->index( 1, 0, QModelIndex() );
    idxB = model->index( 2, 0, QModelIndex() );
    QCOMPARE( model->data( idxA, Qt::DisplayRole ), QVariant(QString::fromAscii("a")) );
    QCOMPARE( model->data( idxB, Qt::DisplayRole ), QVariant(QString::fromAscii("b")) );
    msgListA = model->index( 0, 0, idxA );
    msgListB = model->index( 0, 0, idxB );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );
    t.reset();
    existsA = 0;
    uidNextA = 0;
    uidValidityA = 0;
    uidMapA.clear();
}

void LibMailboxSync::cleanup()
{
    delete model;
    model = 0;
    taskFactoryUnsafe = 0;
    QVERIFY( errorSpy->isEmpty() );
    delete errorSpy;
    errorSpy = 0;
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
}

void LibMailboxSync::cleanupTestCase()
{
}

void LibMailboxSync::initTestCase()
{
    model = 0;
    errorSpy = 0;
}

/** @short Helper: simulate sync of mailbox A that contains some messages from an empty state */
void LibMailboxSync::helperSyncAWithMessagesEmptyState()
{
    // Ask the model to sync stuff
    QCOMPARE( model->rowCount( msgListA ), 0 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    helperSyncAFullSync();
}

/** @short Helper: perform a full sync of the mailbox A */
void LibMailboxSync::helperSyncAFullSync()
{
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );

    helperFakeExistsUidValidityUidNext();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Verify that we indeed received what we wanted
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( ! list->fetched() );

    // The messages are added immediately, even when their UID is not known yet
    QCOMPARE(static_cast<int>(list->childrenCount(model)), static_cast<int>(existsA));

    helperFakeUidSearch();

    QCoreApplication::processEvents();
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

void LibMailboxSync::helperFakeExistsUidValidityUidNext()
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

void LibMailboxSync::helperFakeUidSearch( uint start )
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

    // Try to be nasty and confuse the model with out-of-order UIDs
    QList<uint> shuffledMap = uidMapA;
    if ( shuffledMap.size() > 2 )
        qSwap(shuffledMap[0], shuffledMap[2]);

    for ( uint i = start; i < existsA; ++i ) {
        ss << " " << shuffledMap[i];
    }
    ss << "\r\n";
    ss.flush();
    SOCK->fakeReading( buf + t.last("OK search\r\n") );
}

/** @short Helper: make the parser switch to mailbox B which is actually empty

This function is useful for making sure that the parser switches to another mailbox and will perform
a fresh SELECT when it goes back to the original mailbox.
*/
void LibMailboxSync::helperSyncBNoMessages()
{
    // Try to go to second mailbox
    QCOMPARE( model->rowCount( msgListB ), 0 );
    model->switchToMailbox( idxB );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
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
    Q_ASSERT(msgListB.isValid());
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListB.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->fetched() );

    QVERIFY( errorSpy->isEmpty() );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
}

/** @short Helper: synchronization of an empty mailbox A

Unlike helperSyncBNoMessages(), this function actually performs the sync with all required
responses like UIDVALIDITY and UIDNEXT.

It is the caller's responsibility to provide reasonable values for uidNextA and uidValidityA.

@see helperSyncBNoMessages()
*/
void LibMailboxSync::helperSyncANoMessagesCompleteState()
{
    QCOMPARE( model->rowCount( msgListA ), 0 );
    model->switchToMailbox( idxA );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT a\r\n") );
    SOCK->fakeReading( QString::fromAscii("* 0 exists\r\n* OK [uidnext %1] foo\r\n* ok [uidvalidity %2] bar\r\n"
                                          ).arg(QString::number(uidNextA), QString::number(uidValidityA)).toAscii()
                                  + t.last("ok completed\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Check the cache
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QString::fromAscii("a") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.isUsableForSyncing(), true );
    QCOMPARE( syncState.uidNext(), uidNextA );
    QCOMPARE( syncState.uidValidity(), uidValidityA );

    existsA = 0;
    uidMapA.clear();
    helperCheckCache();
    helperVerifyUidMapA();

    // Verify that we indeed received what we wanted
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->fetched() );

    QVERIFY( errorSpy->isEmpty() );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
}


/** @short Simulates what happens when mailbox A gets opened again, assuming that nothing has changed since the last time etc */
void LibMailboxSync::helperSyncAWithMessagesNoArrivals()
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
void LibMailboxSync::helperSyncFlags()
{
    QCoreApplication::processEvents();
    QByteArray expectedFetch = t.mk("FETCH ") +
            (existsA == 1 ? QByteArray("1") : QByteArray("1:") + QString::number(existsA).toAscii()) +
            QByteArray(" (FLAGS)\r\n");
    QCOMPARE(SOCK->writtenStuff(), expectedFetch);
    QByteArray buf;
    for (uint i = 1; i <= existsA; ++i) {
        switch (i % 10) {
        case 0:
        case 1:
            buf += QString::fromAscii("* %1 FETCH (FLAGS (\\Seen))\r\n").arg(i).toAscii();
            break;
        case 2:
        case 3:
        case 4:
            buf += QString::fromAscii("* %1 FETCH (FLAGS (\\Seen \\Answered))\r\n").arg(i).toAscii();
            break;
        case 5:
            buf += QString::fromAscii("* %1 FETCH (FLAGS (\\Seen starred))\r\n").arg(i).toAscii();
        case 6:
        case 7:
        case 8:
            buf += QString::fromAscii("* %1 FETCH (FLAGS (\\Seen \\Answered $NotJunk))\r\n").arg(i).toAscii();
            break;
        case 9:
            buf += QString::fromAscii("* %1 FETCH (FLAGS ())\r\n").arg(i).toAscii();
            break;
        }
        if (buf.size() > 10*1024) {
            // Flush the output buffer roughly every 10kB
            SOCK->fakeReading(buf);
            buf.clear();
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
        }
    }
    // Now the ugly part -- we know that the Model has that habit of processing at most 100 responses at once and then returning
    // from the event handler.  The idea is to make sure that the GUI can run even in presence of incoming FLAGS in a 50k mailbox
    // (or really anything else; the point is to avoid GUI blocking).  Under normal circumstances, this shouldn't really matter as
    // the event loop will take care of processing all the events, but with tests which "emulate" the event loop through
    // repeatedly calling QCoreApplication::processEvents(), we might *very easily* leave some responses undelivered.
    // This is where our crude hack comes into play -- we know that each message generated one response, so we just make sure that
    // Model's responseReceived will surely runs enough times. Nasty, isn't it? Well, better than introducing a side channel from
    // Model to signal "I really need to process more events, KTHXBYE", IMHO.
    for (uint i = 0; i < (existsA / 100) + 1; ++i)
        QCoreApplication::processEvents();

    SOCK->fakeReading(buf + t.last("OK yay\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

/** @short Helper: update flags for some message */
void LibMailboxSync::helperOneFlagUpdate( const QModelIndex &message )
{
    SOCK->fakeReading( QString::fromAscii("* %1 FETCH (FLAGS (\\SeEn fOo bar))\r\n").arg( message.row() + 1 ).toAscii() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QStringList expectedFlags;
    expectedFlags << QLatin1String("\\Seen") << QLatin1String("fOo") << QLatin1String("bar");
    expectedFlags.sort();
    QCOMPARE(message.data(Imap::Mailbox::RoleMessageFlags).toStringList(), expectedFlags);
    QVERIFY(errorSpy->isEmpty());
}

void LibMailboxSync::helperSyncASomeNew( int number )
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

/** @short Helper: make sure that UID mapping is correct */
void LibMailboxSync::helperVerifyUidMapA()
{
    QCOMPARE( model->rowCount( msgListA ), static_cast<int>(existsA) );
    Q_ASSERT( static_cast<int>(existsA) == uidMapA.size() );
    for ( int i = 0; i < uidMapA.size(); ++i ) {
        QModelIndex message = model->index( i, 0, msgListA );
        Q_ASSERT( message.isValid() );
        QVERIFY( uidMapA[i] != 0 );
        QCOMPARE( message.data( Imap::Mailbox::RoleMessageUid ).toUInt(), uidMapA[i] );
    }
}

/** @short Helper: verify that values recorded in the cache are valid */
void LibMailboxSync::helperCheckCache(bool ignoreUidNext)
{
    // Check the cache
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QString::fromAscii("a") );
    QCOMPARE( syncState.exists(), existsA );
    QCOMPARE( syncState.isUsableForSyncing(), true );
    if ( ! ignoreUidNext ) {
        QCOMPARE( syncState.uidNext(), uidNextA );
    }
    QCOMPARE( syncState.uidValidity(), uidValidityA );
    QCOMPARE( model->cache()->uidMapping( QString::fromAscii("a") ), uidMapA );


    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QVERIFY( errorSpy->isEmpty() );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
}
