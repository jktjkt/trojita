/* Copyright (C) 2010 Jan Kundrát <jkt@gentoo.org>

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
#include "test_Imap_Model.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/MailboxModel.h"

//#define WITH_GUI

#ifdef WITH_GUI
#include <QTreeView>
#endif

void ImapModelTest::initTestCase()
{
    model = 0;
    mboxModel = 0;
}

void ImapModelTest::init()
{
    cache = Imap::Mailbox::CachePtr( new Imap::Mailbox::MemoryCache( QString() ) );
    factory = new Imap::Mailbox::FakeSocketFactory();
    model = new Imap::Mailbox::Model( this, cache, Imap::Mailbox::SocketFactoryPtr( factory ), false );
}

void ImapModelTest::cleanup()
{
    delete model;
    model = 0;
}

#define SOCK static_cast<Imap::FakeSocket*>( factory->lastSocket() )

void ImapModelTest::testSyncMailbox()
{
    model->rowCount( QModelIndex() );
    SOCK->fakeReading( "* PREAUTH foo\r\n" );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 CAPABILITY\r\ny0 LIST \"\" \"%\"\r\n") );
    SOCK->fakeReading( "* LIST (\\HasNoChildren) \".\" \"INBOX\"\r\n"
                       "* CAPABILITY IMAP4rev1\r\n"
                       "y1 OK capability completed\r\n"
                       "y0 ok list completed\r\n" );
    QCoreApplication::processEvents();
    QModelIndex inbox = model->index( 1, 0, QModelIndex() );
    QCOMPARE( model->data( inbox, Qt::DisplayRole ), QVariant("INBOX") );
    QCoreApplication::processEvents();

    // FIXME: more stuff

#ifdef WITH_GUI
    QTreeView* w = new QTreeView();
    w->setModel( model );
    w->show();
    QTest::qWait( 5000 );
#endif
}

void ImapModelTest::testInboxCaseSensitivity()
{
    mboxModel = new Imap::Mailbox::MailboxModel( this, model );
    mboxModel->rowCount( QModelIndex() );
    SOCK->fakeReading( "* PREAUTH foo\r\n" );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 CAPABILITY\r\ny0 LIST \"\" \"%\"\r\n") );
    SOCK->fakeReading( "* LIST (\\Noinferiors) \".\" \"Inbox\"\r\n"
                       "* CAPABILITY IMAP4rev1\r\n"
                       "y1 OK capability completed\r\n"
                       "y0 ok list completed\r\n" );
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
    SOCK->fakeReading( "* PREAUTH foo\r\n" );
    QCoreApplication::processEvents();

    // Ask for capabilities and list top-level mailboxes
    // These commands are interleaved with each other
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 CAPABILITY\r\ny0 LIST \"\" \"%\"\r\n") );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\n"
                       "* LIST (\\HasNoChildren) \".\" \"INBOX\"\r\n"
                       "* LIST (\\HasChildren) \".\" \"SomeParent\"\r\n"
                       "* LIST (\\HasNoChildren) \".\" \"one\"\r\n"
                       "* LIST (\\HasNoChildren) \".\" two\r\n"
                       "y1 OK capability completed\r\n"
                       "y0 ok list completed\r\n" );
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
    model->createMailbox( QString::fromAscii("zzz_newlyCreated") );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y2 CREATE \"zzz_newlyCreated\"\r\n") );

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
    SOCK->fakeReading( "y2 NO go away\r\n" );
    QCoreApplication::processEvents();
    QCOMPARE( creationFailed.count(), 1 );
    QList<QVariant> args = creationFailed.takeFirst();
    QCOMPARE( args.size(), 2 );
    QCOMPARE( args[0], QVariant("zzz_newlyCreated") );
    QCOMPARE( args[1], QVariant("go away") );
    QCOMPARE( creationSucceded.count(), 0 );
    QCOMPARE( deletionFailed.count(), 0 );
    QCOMPARE( deletionSucceded.count(), 0 );
    QVERIFY( noParseError.isEmpty() );

    // Now test its succesfull completion
    model->createMailbox( QString::fromAscii("zzz_newlyCreated2") );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y3 CREATE \"zzz_newlyCreated2\"\r\n") );
    SOCK->fakeReading( "y3 OK mailbox created\r\n" );
    // This one results in issuing another command -> got to make two passes through the event loop
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y4 LIST \"\" \"zzz_newlyCreated2\"\r\n") );
    SOCK->fakeReading( "* LIST (\\HasNoChildren) \".\" zzz_newlyCreated2\r\ny4 OK x\r\n");
    QCOMPARE( creationFailed.count(), 0 );
    QCOMPARE( creationSucceded.count(), 1 );
    args = creationSucceded.takeFirst();
    QCOMPARE( args.size(), 1 );
    QCOMPARE( args[0], QVariant("zzz_newlyCreated2") );
    QCOMPARE( deletionFailed.count(), 0 );
    QCOMPARE( deletionSucceded.count(), 0 );
    QCoreApplication::processEvents();
    QModelIndex mbox_zzz = model->index( 5, 0, QModelIndex() );
    QCOMPARE( model->data( mbox_zzz, Qt::DisplayRole ), QVariant("zzz_newlyCreated2") );
    QVERIFY( noParseError.isEmpty() );

}

QTEST_MAIN( ImapModelTest )
