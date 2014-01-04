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
#include "test_Imap_Tasks_DeleteMailbox.h"
#include "Utils/headless_test.h"
#include "Utils/LibMailboxSync.h"
#include "Common/MetaTypes.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/Model.h"

void ImapModelDeleteMailboxTest::init()
{
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache(this);
    factory = new Streams::FakeSocketFactory(Imap::CONN_STATE_AUTHENTICATED);
    Imap::Mailbox::TaskFactoryPtr taskFactory(new Imap::Mailbox::TestingTaskFactory());
    taskFactoryUnsafe = static_cast<Imap::Mailbox::TestingTaskFactory*>(taskFactory.get());
    taskFactoryUnsafe->fakeOpenConnectionTask = true;
    taskFactoryUnsafe->fakeListChildMailboxes = true;
    model = new Imap::Mailbox::Model(this, cache, Imap::Mailbox::SocketFactoryPtr(factory), std::move(taskFactory));
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    deletedSpy = new QSignalSpy(model, SIGNAL(mailboxDeletionSucceded(QString)));
    failedSpy = new QSignalSpy(model, SIGNAL(mailboxDeletionFailed(QString,QString)));
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
    Common::registerMetaTypes();
    model = 0;
    deletedSpy = 0;
    failedSpy = 0;
}

#define SOCK static_cast<Streams::FakeSocket*>( factory->lastSocket() )

void ImapModelDeleteMailboxTest::_initWithOne()
{
    // Init with one example mailbox
    taskFactoryUnsafe->fakeListChildMailboxesMap[ QLatin1String("") ] = QStringList() << QLatin1String("a");
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 2 );
    QModelIndex idxA = model->index( 1, 0, QModelIndex() );
    QCOMPARE( model->data( idxA, Qt::DisplayRole ), QVariant(QLatin1String("a")) );
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );
}

void ImapModelDeleteMailboxTest::testDeleteOne()
{
    _initWithOne();

    // Now test the actual creating process
    model->deleteMailbox( "a" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 DELETE a\r\n") );
    SOCK->fakeReading( QByteArray("y0 OK deleted\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 1 );
    QCoreApplication::processEvents();
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
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 DELETE a\r\n") );
    SOCK->fakeReading( QByteArray("y0 NO muhehe\r\n") );
    QCoreApplication::processEvents();

    QCOMPARE( model->rowCount( QModelIndex() ), 2 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );
    QCOMPARE( failedSpy->size(), 1 );
    QVERIFY( deletedSpy->isEmpty() );
}

TROJITA_HEADLESS_TEST( ImapModelDeleteMailboxTest )
