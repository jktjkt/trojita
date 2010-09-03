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
#include <QAuthenticator>
#include "test_Imap_Model_OpenConnectionTask.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/MailboxModel.h"
#include "Imap/Tasks/OpenConnectionTask.h"

void ImapModelOpenConnectionTaskTest::initTestCase()
{
    model = 0;
    completedSpy = 0;
    qRegisterMetaType<QAuthenticator*>("QAuthenticator*");
}

void ImapModelOpenConnectionTaskTest::init()
{
    init( false );
}

void ImapModelOpenConnectionTaskTest::init( bool startTlsRequired )
{
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache( this, QString() );
    factory = new Imap::Mailbox::FakeSocketFactory();
    factory->setStartTlsRequired( startTlsRequired );
    Imap::Mailbox::TaskFactoryPtr taskFactory( new Imap::Mailbox::TaskFactory() ); // yes, the real one
    model = new Imap::Mailbox::Model( this, cache, Imap::Mailbox::SocketFactoryPtr( factory ), taskFactory, false );
    connect(model, SIGNAL(authRequested(QAuthenticator*)), this, SLOT(provideAuthDetails(QAuthenticator*)) );
    task = new Imap::Mailbox::OpenConnectionTask( model );
    completedSpy = new QSignalSpy( task, SIGNAL(completed()) );
    authSpy = new QSignalSpy( model, SIGNAL(authRequested(QAuthenticator*)) );
}

void ImapModelOpenConnectionTaskTest::cleanup()
{
    delete model;
    model = 0;
    task = 0;
    delete completedSpy;
    completedSpy = 0;
    delete authSpy;
    authSpy = 0;
}

#define SOCK static_cast<Imap::FakeSocket*>( factory->lastSocket() )

/** @short Test for explicitly obtaining capability when greeted by PREAUTH */
void ImapModelOpenConnectionTaskTest::testPreauth()
{
    SOCK->fakeReading( "* PREAUTH foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny0 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY( authSpy->isEmpty() );
}

/** @short Test that we can obtain capability when embedded in PREAUTH */
void ImapModelOpenConnectionTaskTest::testPreauthWithCapability()
{
    SOCK->fakeReading( "* PREAUTH [CAPABILITY IMAP4rev1] foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray() );
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY( authSpy->isEmpty() );
}

/** @short Test for obtaining capability and logging in without any STARTTLS */
void ImapModelOpenConnectionTaskTest::testOk()
{
    SOCK->fakeReading( "* OK foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny0 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCOMPARE( authSpy->size(), 1 );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 LOGIN luzr sikrit\r\n") );
    SOCK->fakeReading( "y1 OK logged in\r\n");
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QCOMPARE( authSpy->size(), 1 );
}

/** @short Test with capability inside the OK greetings, no STARTTLS */
void ImapModelOpenConnectionTaskTest::testOkWithCapability()
{
    SOCK->fakeReading( "* OK [CAPABILITY IMAP4rev1] foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCOMPARE( authSpy->size(), 1 );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 LOGIN luzr sikrit\r\n") );
    SOCK->fakeReading( "y0 OK logged in\r\n");
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QCOMPARE( authSpy->size(), 1 );
}

/** @short Test to honor embedded LOGINDISABLED */
void ImapModelOpenConnectionTaskTest::testOkLogindisabled()
{
    SOCK->fakeReading( "* OK [CAPABILITY IMAP4rev1 LoginDisabled] foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 STARTTLS\r\n") );
    SOCK->fakeReading( "y0 OK will establish secure layer immediately\r\n");
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("[*** STARTTLS ***]y1 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    QVERIFY( authSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny1 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y2 LOGIN luzr sikrit\r\n") );
    QCOMPARE( authSpy->size(), 1 );
    SOCK->fakeReading( "y2 OK logged in\r\n");
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QCOMPARE( authSpy->size(), 1 );
}

/** @short Test for an explicit CAPABILITY retrieval and automatic STARTTLS when LOGINDISABLED */
void ImapModelOpenConnectionTaskTest::testOkLogindisabledLater()
{
    SOCK->fakeReading( "* OK foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1 LoGINDISABLED\r\ny0 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 STARTTLS\r\n") );
    SOCK->fakeReading( "y1 OK will establish secure layer immediately\r\n");
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("[*** STARTTLS ***]y2 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    QVERIFY( authSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny2 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y3 LOGIN luzr sikrit\r\n") );
    QCOMPARE( authSpy->size(), 1 );
    SOCK->fakeReading( "y3 OK logged in\r\n");
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QCOMPARE( authSpy->size(), 1 );
}

/** @short Test conf-requested STARTTLS when not faced with embedded capabilities in OK greetings */
void ImapModelOpenConnectionTaskTest::testOkStartTls()
{
    cleanup(); init(true); // yuck, but I can't come up with anything better...

    SOCK->fakeReading( "* OK foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 STARTTLS\r\n") );
    SOCK->fakeReading( "y0 OK will establish secure layer immediately\r\n");
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("[*** STARTTLS ***]y1 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    QVERIFY( authSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny1 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y2 LOGIN luzr sikrit\r\n") );
    QCOMPARE( authSpy->size(), 1 );
    SOCK->fakeReading( "y2 OK logged in\r\n");
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QCOMPARE( authSpy->size(), 1 );
}

/** @short Test to re-request formerly embedded capabilities when launching STARTTLS */
void ImapModelOpenConnectionTaskTest::testOkStartTlsDiscardCaps()
{
    cleanup(); init(true); // yuck, but I can't come up with anything better...

    SOCK->fakeReading( "* OK [Capability imap4rev1] foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 STARTTLS\r\n") );
    SOCK->fakeReading( "y0 OK will establish secure layer immediately\r\n");
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("[*** STARTTLS ***]y1 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    QVERIFY( authSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny1 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y2 LOGIN luzr sikrit\r\n") );
    QCOMPARE( authSpy->size(), 1 );
    SOCK->fakeReading( "y2 OK logged in\r\n");
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QCOMPARE( authSpy->size(), 1 );
}

// FIXME: verify how LOGINDISABLED even after STARTLS ends up

void ImapModelOpenConnectionTaskTest::provideAuthDetails( QAuthenticator* auth )
{
    if ( auth ) {
        auth->setUser( QLatin1String("luzr") );
        auth->setPassword( QLatin1String("sikrit") );
    }
}


QTEST_MAIN( ImapModelOpenConnectionTaskTest )
