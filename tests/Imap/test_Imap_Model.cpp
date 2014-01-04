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
#include "test_Imap_Model.h"
#include "Utils/headless_test.h"
#include "Utils/LibMailboxSync.h"
#include "Common/MetaTypes.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/MailboxModel.h"

void ImapModelTest::initTestCase()
{
    Common::registerMetaTypes();
    model = 0;
    mboxModel = 0;
}

void ImapModelTest::init()
{
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache(this);
    factory = new Streams::FakeSocketFactory(Imap::CONN_STATE_CONNECTED_PRETLS_PRECAPS);
    Imap::Mailbox::TaskFactoryPtr taskFactory(new Imap::Mailbox::TaskFactory());
    model = new Imap::Mailbox::Model(this, cache, Imap::Mailbox::SocketFactoryPtr(factory), std::move(taskFactory));
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    QCoreApplication::processEvents();
}

void ImapModelTest::cleanup()
{
    delete model;
    model = 0;
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
}

#define SOCK static_cast<Streams::FakeSocket*>( factory->lastSocket() )

void ImapModelTest::testSyncMailbox()
{
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    SOCK->fakeReading( "* PREAUTH [CAPABILITY Imap4Rev1] foo\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 LIST \"\" \"%\"\r\n") );
    SOCK->fakeReading( "* LIST (\\HasNoChildren) \".\" \"INBOX\"\r\n"
                       "* CAPABILITY IMAP4rev1\r\n"
                       "y0 ok list completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QModelIndex inbox = model->index( 1, 0, QModelIndex() );
    QCOMPARE( model->data( inbox, Qt::DisplayRole ), QVariant("INBOX") );
    QCoreApplication::processEvents();

    // further tests are especially in the Imap_Task_ObtainSynchronizedMailboxTest
}

void ImapModelTest::testInboxCaseSensitivity()
{
    mboxModel = new Imap::Mailbox::MailboxModel( this, model );
    mboxModel->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    SOCK->fakeReading( "* PREAUTH [Capability imap4rev1] foo\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 LIST \"\" \"%\"\r\n") );
    SOCK->fakeReading( "* LIST (\\Noinferiors) \".\" \"Inbox\"\r\n"
                       "y0 ok list completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( mboxModel->data( mboxModel->index( 0, 0, QModelIndex() ), Qt::DisplayRole ), QVariant("INBOX") );
    mboxModel->deleteLater();
    mboxModel = 0;
}

void ImapModelTest::testCreationDeletionHandling()
{
    QSignalSpy noParseError( model, SIGNAL(connectionError(QString)) );
    QVERIFY( noParseError.isValid() );
    // Start the conversation
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    SOCK->fakeReading( "* PREAUTH [CAPABILITY imap4rev1] foo\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Ask for capabilities and list top-level mailboxes
    // These commands are interleaved with each other
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 LIST \"\" \"%\"\r\n") );
    SOCK->fakeReading( "* LIST (\\HasNoChildren) \".\" \"INBOX\"\r\n"
                       "* LIST (\\HasChildren) \".\" \"SomeParent\"\r\n"
                       "* LIST (\\HasNoChildren) \".\" \"one\"\r\n"
                       "* LIST (\\HasNoChildren) \".\" two\r\n"
                       "y0 ok list completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Note that the ordering is case-insensitive
    QModelIndex mbox_inbox = model->index( 1, 0, QModelIndex() );
    QModelIndex mbox_one = model->index( 2, 0, QModelIndex() );
    QModelIndex mbox_SomeParent = model->index( 3, 0, QModelIndex() );
    QModelIndex mbox_two = model->index( 4, 0, QModelIndex() );
    QCOMPARE( model->data( mbox_inbox, Qt::DisplayRole ), QVariant("INBOX") );
    QCOMPARE( model->data( mbox_one, Qt::DisplayRole ), QVariant("one") );
    QCOMPARE( model->data( mbox_two, Qt::DisplayRole ), QVariant("two") );
    QCOMPARE( model->data( mbox_SomeParent, Qt::DisplayRole ), QVariant("SomeParent") );
    QVERIFY( noParseError.isEmpty() );

    // Try to create mailbox
    model->createMailbox( QLatin1String("zzz_newly-Created") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 CREATE zzz_newly-Created\r\n") );

    // Sane invariants
    QSignalSpy creationFailed( model, SIGNAL(mailboxCreationFailed(QString,QString)) );
    QVERIFY( creationFailed.isValid() );
    QSignalSpy creationSucceded( model, SIGNAL(mailboxCreationSucceded(QString)) );
    QVERIFY( creationSucceded.isValid() );
    QSignalSpy deletionFailed( model, SIGNAL(mailboxDeletionFailed(QString,QString)) );
    QVERIFY( deletionFailed.isValid() );
    QSignalSpy deletionSucceded( model, SIGNAL(mailboxDeletionSucceded(QString)) );
    QVERIFY( deletionSucceded.isValid() );

    // Test that we handle failure of the CREATE command
    SOCK->fakeReading( "y1 NO go away\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( creationFailed.count(), 1 );
    QList<QVariant> args = creationFailed.takeFirst();
    QCOMPARE( args.size(), 2 );
    QCOMPARE( args[0], QVariant("zzz_newly-Created") );
    QCOMPARE( args[1], QVariant("go away") );
    QCOMPARE( creationSucceded.count(), 0 );
    QCOMPARE( deletionFailed.count(), 0 );
    QCOMPARE( deletionSucceded.count(), 0 );
    QVERIFY( noParseError.isEmpty() );

    // Now test its successful completion
    model->createMailbox( QLatin1String("zzz_newly-Created2") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y2 CREATE zzz_newly-Created2\r\n") );
    SOCK->fakeReading( "y2 OK mailbox created\r\n" );
    // This one results in issuing another command -> got to make two passes through the event loop
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y3 LIST \"\" zzz_newly-Created2\r\n") );
    SOCK->fakeReading( "* LIST (\\HasNoChildren) \".\" zzz_newly-Created2\r\ny3 OK x\r\n");
    QCOMPARE( creationFailed.count(), 0 );
    QCOMPARE( creationSucceded.count(), 1 );
    args = creationSucceded.takeFirst();
    QCOMPARE( args.size(), 1 );
    QCOMPARE( args[0], QVariant("zzz_newly-Created2") );
    QCOMPARE( deletionFailed.count(), 0 );
    QCOMPARE( deletionSucceded.count(), 0 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QModelIndex mbox_zzz = model->index( 5, 0, QModelIndex() );
    QCOMPARE( model->data( mbox_zzz, Qt::DisplayRole ), QVariant("zzz_newly-Created2") );
    QVERIFY( noParseError.isEmpty() );

}

TROJITA_HEADLESS_TEST( ImapModelTest )
