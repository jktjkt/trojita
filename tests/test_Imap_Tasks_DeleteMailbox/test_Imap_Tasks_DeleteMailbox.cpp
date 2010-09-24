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
#include "test_Imap_Tasks_DeleteMailbox.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/Model.h"

void ImapModelDeleteMailboxTest::init()
{
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache( this, QString() );
    factory = new Imap::Mailbox::FakeSocketFactory();
    Imap::Mailbox::TaskFactoryPtr taskFactory( new Imap::Mailbox::TestingTaskFactory() );
    taskFactoryUnsafe = static_cast<Imap::Mailbox::TestingTaskFactory*>( taskFactory.get() );
    taskFactoryUnsafe->fakeOpenConnectionTask = true;
    taskFactoryUnsafe->fakeListChildMailboxes = true;
    model = new Imap::Mailbox::Model( this, cache, Imap::Mailbox::SocketFactoryPtr( factory ), taskFactory, false );
    deletedSpy = new QSignalSpy( model, SIGNAL(mailboxDeletionSucceded(QString)) );
    failedSpy = new QSignalSpy( model, SIGNAL(mailboxDeletionFailed(QString,QString)) );
}

void ImapModelDeleteMailboxTest::cleanup()
{
    delete model;
    model = 0;
    taskFactoryUnsafe = 0;
    delete deletedSpy;
    deletedSpy = 0;
    delete failedSpy;
    failedSpy = 0;
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
}

void ImapModelDeleteMailboxTest::initTestCase()
{
    model = 0;
    deletedSpy = 0;
    failedSpy = 0;
}

#define SOCK static_cast<Imap::FakeSocket*>( factory->lastSocket() )

void ImapModelDeleteMailboxTest::_initWithOne()
{
    // Init with one example mailbox
    taskFactoryUnsafe->fakeListChildMailboxesMap[ QString::fromAscii("") ] = QStringList() << QString::fromAscii("a");
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 2 );
    QModelIndex idxA = model->index( 1, 0, QModelIndex() );
    QCOMPARE( model->data( idxA, Qt::DisplayRole ), QVariant(QString::fromAscii("a")) );
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );
}

void ImapModelDeleteMailboxTest::testDeleteOne()
{
    _initWithOne();

    // Now test the actual creating process
    model->deleteMailbox( "a" );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 DELETE a\r\n") );
    SOCK->fakeReading( QByteArray("y0 OK deleted\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 1 );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
    QCOMPARE( deletedSpy->size(), 1 );
    QVERIFY( failedSpy->isEmpty() );
}

void ImapModelDeleteMailboxTest::testDeleteFail()
{
    _initWithOne();

    // Test failure of the DELETE command
    model->deleteMailbox( "a" );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 DELETE a\r\n") );
    SOCK->fakeReading( QByteArray("y0 NO muhehe\r\n") );
    QCoreApplication::processEvents();

    QCOMPARE( model->rowCount( QModelIndex() ), 2 );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
    QCOMPARE( failedSpy->size(), 1 );
    QVERIFY( deletedSpy->isEmpty() );
}

QTEST_MAIN( ImapModelDeleteMailboxTest )
