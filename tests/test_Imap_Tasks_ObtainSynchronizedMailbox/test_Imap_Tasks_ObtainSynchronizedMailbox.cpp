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
#include "Streams/FakeSocket.h"
#include "Imap/Model/Cache.h"
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
    taskFactoryUnsafe->fakeListChildMailboxesMap[ QString::fromAscii("") ] = QStringList() << QString::fromAscii("a");
    model = new Imap::Mailbox::Model( this, cache, Imap::Mailbox::SocketFactoryPtr( factory ), taskFactory, false );
    task = 0;

    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 2 );
    QModelIndex idxA = model->index( 1, 0, QModelIndex() );
    QCOMPARE( model->data( idxA, Qt::DisplayRole ), QVariant(QString::fromAscii("a")) );
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );
}

void ImapModelObtainSynchronizedMailboxTest::cleanup()
{
    delete model;
    model = 0;
    taskFactoryUnsafe = 0;
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
}

void ImapModelObtainSynchronizedMailboxTest::cleanupTestCase()
{
}

void ImapModelObtainSynchronizedMailboxTest::initTestCase()
{
    model = 0;
    task = 0;
}

void ImapModelObtainSynchronizedMailboxTest::testSyncEmpty()
{
    // Boring stuff
    QModelIndex idxA = model->index( 1, 0, QModelIndex() );
    QModelIndex msgList = model->index( 0, 0, idxA );

    // Ask the model to sync stuff
    QCOMPARE( model->rowCount( msgList ), 0 );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 SELECT a\r\n") );

    // Try to feed it with absolute minimum data
    SOCK->fakeReading( QByteArray("* 0 exists\r\n"
                                  "y0 OK done\r\n") );
    QCoreApplication::processEvents();

    // Verify that we indeed received what we wanted
    QCOMPARE( model->rowCount( msgList ), 0 );
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgList.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->fetched() );
    QVERIFY( SOCK->writtenStuff().isEmpty() );

    // Now, let's try to re-sync it once again; the difference is that our cache now has "something"
    model->resyncMailbox( idxA );
    QCoreApplication::processEvents();

    // Verify that it indeed caused a re-synchronization
    list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgList.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->loading() );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 SELECT a\r\n") );
    SOCK->fakeReading( QByteArray("* 0 exists\r\n"
                                  "y1 OK done\r\n") );
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
}

QTEST_MAIN( ImapModelObtainSynchronizedMailboxTest )
