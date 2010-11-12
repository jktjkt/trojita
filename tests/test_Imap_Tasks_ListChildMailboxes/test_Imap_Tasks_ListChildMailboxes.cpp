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
#include "test_Imap_Tasks_ListChildMailboxes.h"
#include "../headless_test.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/Model.h"
#include "Imap/Tasks/Fake_ListChildMailboxesTask.h"

void ImapModelListChildMailboxesTest::init()
{
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache( this, QString() );
    factory = new Imap::Mailbox::FakeSocketFactory();
    Imap::Mailbox::TaskFactoryPtr taskFactory( new Imap::Mailbox::TestingTaskFactory() );
    taskFactoryUnsafe = static_cast<Imap::Mailbox::TestingTaskFactory*>( taskFactory.get() );
    taskFactoryUnsafe->fakeOpenConnectionTask = true;
    model = new Imap::Mailbox::Model( this, cache, Imap::Mailbox::SocketFactoryPtr( factory ), taskFactory, false );
    task = 0;
}

void ImapModelListChildMailboxesTest::cleanup()
{
    delete model;
    model = 0;
    taskFactoryUnsafe = 0;
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
}

void ImapModelListChildMailboxesTest::initTestCase()
{
    model = 0;
    task = 0;
}

#define SOCK static_cast<Imap::FakeSocket*>( factory->lastSocket() )

void ImapModelListChildMailboxesTest::testSimpleListing()
{
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 LIST \"\" \"%\"\r\n") );
    SOCK->fakeReading( "* LIST (\\HasNoChildren) \".\" \"b\"\r\n"
                       "* LIST (\\HasChildren) \".\" \"a\"\r\n"
                       "* LIST (\\Noselect \\HasChildren) \".\" \"xyz\"\r\n"
                       "* LIST (\\HasNoChildren) \".\" \"INBOX\"\r\n"
                       "y0 OK List done.\r\n");
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 5 );
    // the first one will be "list of messages"
    QModelIndex idxInbox = model->index( 1, 0, QModelIndex() );
    QModelIndex idxA = model->index( 2, 0, QModelIndex() );
    QModelIndex idxB = model->index( 3, 0, QModelIndex() );
    QModelIndex idxXyz = model->index( 4, 0, QModelIndex() );
    QCOMPARE( model->data( idxInbox, Qt::DisplayRole ), QVariant(QString::fromAscii("INBOX")) );
    QCOMPARE( model->data( idxA, Qt::DisplayRole ), QVariant(QString::fromAscii("a")) );
    QCOMPARE( model->data( idxB, Qt::DisplayRole ), QVariant(QString::fromAscii("b")) );
    QCOMPARE( model->data( idxXyz, Qt::DisplayRole ), QVariant(QString::fromAscii("xyz")) );
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );
    QCOMPARE( model->rowCount( idxInbox ), 1 ); // just the "list of messages"
    QCOMPARE( model->rowCount( idxB ), 1 ); // just the "list of messages"
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray() );
    model->rowCount( idxA );
    model->rowCount( idxXyz );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 LIST \"\" \"a.%\"\r\n" "y2 LIST \"\" \"xyz.%\"\r\n") );
    SOCK->fakeReading( "* LIST (\\HasNoChildren) \".\" \"a.aa\"\r\n"
                       "* LIST (\\HasNoChildren) \".\" \"a.ab\"\r\n"
                       "y1 OK List completed.\r\n"
                       "* LIST (\\HasNoChildren) \".\" \"xyz.a\"\r\n"
                       "* LIST (\\HasNoChildren) \".\" \"xyz.c\"\r\n"
                       "y2 OK List completed.\r\n" );
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( idxA ), 3 );
    QCOMPARE( model->rowCount( idxXyz ), 3 );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
}

void ImapModelListChildMailboxesTest::testFakeListing()
{
    taskFactoryUnsafe->fakeListChildMailboxes = true;
    taskFactoryUnsafe->fakeListChildMailboxesMap[ QString::fromAscii("") ] = QStringList() << QString::fromAscii("a") << QString::fromAscii("b");
    taskFactoryUnsafe->fakeListChildMailboxesMap[ QString::fromAscii("a") ] = QStringList() << QString::fromAscii("aa") << QString::fromAscii("ab");
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 3 );
    QModelIndex idxA = model->index( 1, 0, QModelIndex() );
    QModelIndex idxB = model->index( 2, 0, QModelIndex() );
    QCOMPARE( model->data( idxA, Qt::DisplayRole ), QVariant(QString::fromAscii("a")) );
    QCOMPARE( model->data( idxB, Qt::DisplayRole ), QVariant(QString::fromAscii("b")) );
    model->rowCount( idxA );
    model->rowCount( idxB );
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( idxA ), 3 );
    QCOMPARE( model->rowCount( idxB ), 1 );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
}

TROJITA_HEADLESS_TEST( ImapModelListChildMailboxesTest )
