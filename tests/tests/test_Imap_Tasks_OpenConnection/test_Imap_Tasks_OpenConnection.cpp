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
#include <QAuthenticator>
#include "test_Imap_Tasks_OpenConnection.h"
#include "../headless_test.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/MailboxModel.h"
#include "Imap/Tasks/OpenConnectionTask.h"

void ImapModelOpenConnectionTest::initTestCase()
{
    model = 0;
    completedSpy = 0;
    qRegisterMetaType<QList<QSslCertificate> >("QList<QSslCertificate>");
    qRegisterMetaType<QList<QSslError> >("QList<QSslError>");
}

void ImapModelOpenConnectionTest::init()
{
    init( false );
}

void ImapModelOpenConnectionTest::init( bool startTlsRequired )
{
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache( this, QString() );
    factory = new Imap::Mailbox::FakeSocketFactory();
    factory->setStartTlsRequired( startTlsRequired );
    Imap::Mailbox::TaskFactoryPtr taskFactory( new Imap::Mailbox::TaskFactory() ); // yes, the real one
    model = new Imap::Mailbox::Model( this, cache, Imap::Mailbox::SocketFactoryPtr( factory ), taskFactory, false );
    connect(model, SIGNAL(authRequested()), this, SLOT(provideAuthDetails()), Qt::QueuedConnection);
    connect(model, SIGNAL(needsSslDecision(QList<QSslCertificate>,QList<QSslError>)),
            this, SLOT(acceptSsl(QList<QSslCertificate>,QList<QSslError>)), Qt::QueuedConnection);
    task = new Imap::Mailbox::OpenConnectionTask( model );
    using Imap::Mailbox::ImapTask;
    qRegisterMetaType<ImapTask*>("ImapTask*");
    completedSpy = new QSignalSpy(task, SIGNAL(completed(ImapTask*)));
    failedSpy = new QSignalSpy(task, SIGNAL(failed(QString)));
    authSpy = new QSignalSpy(model, SIGNAL(authRequested()));
}

void ImapModelOpenConnectionTest::acceptSsl(const QList<QSslCertificate> &certificateChain, const QList<QSslError> &sslErrors)
{
    model->setSslPolicy(certificateChain, sslErrors, true);
}

void ImapModelOpenConnectionTest::cleanup()
{
    delete model;
    model = 0;
    task = 0;
    delete completedSpy;
    completedSpy = 0;
    delete failedSpy;
    failedSpy = 0;
    delete authSpy;
    authSpy = 0;
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
}

#define SOCK static_cast<Imap::FakeSocket*>( factory->lastSocket() )

/** @short Test for explicitly obtaining capability when greeted by PREAUTH */
void ImapModelOpenConnectionTest::testPreauth()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading( "* PREAUTH foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny0 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY(failedSpy->isEmpty());
    QVERIFY( authSpy->isEmpty() );
}

/** @short Test that we can obtain capability when embedded in PREAUTH */
void ImapModelOpenConnectionTest::testPreauthWithCapability()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading( "* PREAUTH [CAPABILITY IMAP4rev1] foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray() );
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY(failedSpy->isEmpty());
    QVERIFY( authSpy->isEmpty() );
}

/** @short Test for obtaining capability and logging in without any STARTTLS */
void ImapModelOpenConnectionTest::testOk()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading( "* OK foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny0 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( authSpy->size(), 1 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 LOGIN luzr sikrit\r\n") );
    SOCK->fakeReading( "y1 OK [CAPABILITY IMAP4rev1] logged in\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE( authSpy->size(), 1 );
}

/** @short Test with capability inside the OK greetings, no STARTTLS */
void ImapModelOpenConnectionTest::testOkWithCapability()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading( "* OK [CAPABILITY IMAP4rev1] foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( authSpy->size(), 1 );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 LOGIN luzr sikrit\r\n") );
    SOCK->fakeReading( "y0 OK [CAPABILITY IMAP4rev1] logged in\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE( authSpy->size(), 1 );
}

/** @short See what happens when the capability response doesn't contain IMAP4rev1 capability */
void ImapModelOpenConnectionTest::testOkMissingImap4rev1()
{
    SOCK->fakeReading( "* OK [CAPABILITY pwn] foo\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y0 LOGOUT\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(authSpy->isEmpty());
    QVERIFY(SOCK->writtenStuff().isEmpty());
}

/** @short Test to honor embedded LOGINDISABLED */
void ImapModelOpenConnectionTest::testOkLogindisabled()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading( "* OK [CAPABILITY IMAP4rev1 starttls LoginDisabled] foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 STARTTLS\r\n") );
    SOCK->fakeReading( "y0 OK will establish secure layer immediately\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("[*** STARTTLS ***]y1 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    QVERIFY( authSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny1 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y2 LOGIN luzr sikrit\r\n") );
    QCOMPARE( authSpy->size(), 1 );
    SOCK->fakeReading( "y2 OK [CAPABILITY IMAP4rev1] logged in\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE( authSpy->size(), 1 );
}

/** @short Test how LOGINDISABLED without a corresponding STARTTLS in the capabilities end up */
void ImapModelOpenConnectionTest::testOkLogindisabledWithoutStarttls()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading( "* OK [CAPABILITY IMAP4rev1 LoginDisabled] foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    // The capabilities do not contain STARTTLS but LOGINDISABLED is in there
    QCOMPARE(failedSpy->size(), 1);
}

/** @short Test for an explicit CAPABILITY retrieval and automatic STARTTLS when LOGINDISABLED */
void ImapModelOpenConnectionTest::testOkLogindisabledLater()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading( "* OK foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1 starttls LoGINDISABLED\r\ny0 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 STARTTLS\r\n") );
    SOCK->fakeReading( "y1 OK will establish secure layer immediately\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("[*** STARTTLS ***]y2 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    QVERIFY( authSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny2 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y3 LOGIN luzr sikrit\r\n") );
    QCOMPARE( authSpy->size(), 1 );
    SOCK->fakeReading( "y3 OK [CAPABILITY IMAP4rev1] logged in\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE( authSpy->size(), 1 );
}

/** @short Test conf-requested STARTTLS when not faced with embedded capabilities in OK greetings */
void ImapModelOpenConnectionTest::testOkStartTls()
{
    cleanup(); init(true); // yuck, but I can't come up with anything better...

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading( "* OK foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y0 CAPABILITY\r\n"));
    SOCK->fakeReading("* CAPABILITY imap4rev1 starttls\r\ny0 ok cap\r\n");
    QVERIFY( authSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 STARTTLS\r\n") );
    SOCK->fakeReading( "y1 OK will establish secure layer immediately\r\n");
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("[*** STARTTLS ***]y2 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    QVERIFY( authSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny2 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y3 LOGIN luzr sikrit\r\n") );
    QCOMPARE( authSpy->size(), 1 );
    SOCK->fakeReading( "y3 OK [CAPABILITY IMAP4rev1] logged in\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE( authSpy->size(), 1 );
}

/** @short Test that an untagged CAPABILITY after LOGIN prevents an extra CAPABILITY command */
void ImapModelOpenConnectionTest::testCapabilityAfterLogin()
{
    cleanup(); init(true); // yuck, but I can't come up with anything better...

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading( "* OK foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y0 CAPABILITY\r\n"));
    SOCK->fakeReading("* CAPABILITY imap4rev1 starttls\r\ny0 ok cap\r\n");
    QVERIFY( authSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 STARTTLS\r\n") );
    SOCK->fakeReading( "y1 OK will establish secure layer immediately\r\n");
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("[*** STARTTLS ***]y2 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    QVERIFY( authSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny2 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y3 LOGIN luzr sikrit\r\n") );
    QCOMPARE( authSpy->size(), 1 );
    SOCK->fakeReading("* CAPABILITY imap4rev1\r\n" "y3 OK logged in\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE( authSpy->size(), 1 );
    QVERIFY(SOCK->writtenStuff().isEmpty());
}

/** @short Test conf-requested STARTTLS when the server doesn't support STARTTLS at all */
void ImapModelOpenConnectionTest::testOkStartTlsForbidden()
{
    cleanup(); init(true); // yuck, but I can't come up with anything better...

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading( "* OK foo\r\n" );
    QVERIFY(completedSpy->isEmpty());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y0 CAPABILITY\r\n"));
    SOCK->fakeReading("* CAPABILITY imap4rev1\r\ny0 ok cap\r\n");
    QVERIFY(authSpy->isEmpty());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(failedSpy->size(), 1);
    QVERIFY(authSpy->isEmpty());
}

/** @short Test to re-request formerly embedded capabilities when launching STARTTLS */
void ImapModelOpenConnectionTest::testOkStartTlsDiscardCaps()
{
    cleanup(); init(true); // yuck, but I can't come up with anything better...

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading( "* OK [Capability imap4rev1 starttls] foo\r\n" );
    QVERIFY( completedSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 STARTTLS\r\n") );
    SOCK->fakeReading( "y0 OK will establish secure layer immediately\r\n");
    QCoreApplication::processEvents();
    QVERIFY( authSpy->isEmpty() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("[*** STARTTLS ***]y1 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    QVERIFY( authSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny1 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y2 LOGIN luzr sikrit\r\n") );
    QCOMPARE( authSpy->size(), 1 );
    SOCK->fakeReading( "y2 OK [CAPABILITY IMAP4rev1] logged in\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE( authSpy->size(), 1 );
}

/** @short Test how COMPRESS=DEFLATE gets activated and its interaction with further tasks */
void ImapModelOpenConnectionTest::testCompressDeflateOk()
{
    qDebug() << task;
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading("* OK [capability imap4rev1] hi there\r\n");
    QVERIFY(completedSpy->isEmpty());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y0 LOGIN luzr sikrit\r\n"));
    QCOMPARE(authSpy->size(), 1);
    SOCK->fakeReading("y0 OK [CAPABILITY IMAP4rev1 compress=deflate id] logged in\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y1 COMPRESS DEFLATE\r\n"));
    SOCK->fakeReading("y1 OK compressing\r\n");
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QByteArray("[*** DEFLATE ***]"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y2 ID NIL\r\n"));
    SOCK->fakeReading("* ID nil\r\ny2 OK you courious peer\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(SOCK->writtenStuff().isEmpty());
}

// FIXME: verify how LOGINDISABLED even after STARTLS ends up

void ImapModelOpenConnectionTest::provideAuthDetails()
{
    model->setImapUser(QLatin1String("luzr"));
    model->setImapPassword(QLatin1String("sikrit"));
}


TROJITA_HEADLESS_TEST( ImapModelOpenConnectionTest )
