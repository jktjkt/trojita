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
#include <QAuthenticator>
#include "test_Imap_Tasks_OpenConnection.h"
#include "Utils/headless_test.h"
#include "Utils/LibMailboxSync.h"
#include "Common/MetaTypes.h"
#include "Streams/FakeSocket.h"
#include "Streams/TrojitaZlibStatus.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/MailboxModel.h"
#include "Imap/Tasks/OpenConnectionTask.h"

void ImapModelOpenConnectionTest::initTestCase()
{
    model = 0;
    completedSpy = 0;
    m_enableAutoLogin = true;
}

void ImapModelOpenConnectionTest::init()
{
    Common::registerMetaTypes();
    qRegisterMetaType<Imap::Mailbox::ImapTask*>();
    init( false );
}

void ImapModelOpenConnectionTest::init( bool startTlsRequired )
{
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache(this);
    factory = new Streams::FakeSocketFactory(Imap::CONN_STATE_CONNECTED_PRETLS_PRECAPS);
    factory->setStartTlsRequired( startTlsRequired );
    Imap::Mailbox::TaskFactoryPtr taskFactory( new Imap::Mailbox::TaskFactory() ); // yes, the real one
    model = new Imap::Mailbox::Model(this, cache, Imap::Mailbox::SocketFactoryPtr( factory ), std::move(taskFactory));
    connect(model, SIGNAL(authRequested()), this, SLOT(provideAuthDetails()), Qt::QueuedConnection);
    connect(model, SIGNAL(needsSslDecision(QList<QSslCertificate>,QList<QSslError>)),
            this, SLOT(acceptSsl(QList<QSslCertificate>,QList<QSslError>)), Qt::QueuedConnection);
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    QCoreApplication::processEvents();
    task = new Imap::Mailbox::OpenConnectionTask(model);
    completedSpy = new QSignalSpy(task, SIGNAL(completed(Imap::Mailbox::ImapTask*)));
    failedSpy = new QSignalSpy(task, SIGNAL(failed(QString)));
    authSpy = new QSignalSpy(model, SIGNAL(authRequested()));
    connErrorSpy = new QSignalSpy(model, SIGNAL(connectionError(QString)));
    startTlsUpgradeSpy = new QSignalSpy(model, SIGNAL(requireStartTlsInFuture()));
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
    delete startTlsUpgradeSpy;
    startTlsUpgradeSpy = 0;
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
}

#define SOCK static_cast<Streams::FakeSocket*>( factory->lastSocket() )

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
    QVERIFY(startTlsUpgradeSpy->isEmpty());
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
    QVERIFY(startTlsUpgradeSpy->isEmpty());
}

/** @short What happens when the server responds with PREAUTH and we want STARTTLS? */
void ImapModelOpenConnectionTest::testPreauthWithStartTlsWanted()
{
    cleanup(); init(true); // yuck, but I can't come up with anything better...

    cEmpty();
    cServer("* PREAUTH hi there\r\n");
    QCOMPARE(failedSpy->size(), 1);
    QVERIFY(completedSpy->isEmpty());
    QVERIFY(authSpy->isEmpty());
    QVERIFY(startTlsUpgradeSpy->isEmpty());
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
    QVERIFY(startTlsUpgradeSpy->isEmpty());
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
    QCoreApplication::processEvents();
    QCOMPARE( authSpy->size(), 1 );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 LOGIN luzr sikrit\r\n") );
    SOCK->fakeReading( "y0 OK [CAPABILITY IMAP4rev1] logged in\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE( authSpy->size(), 1 );
    QVERIFY(startTlsUpgradeSpy->isEmpty());
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
    QVERIFY(startTlsUpgradeSpy->isEmpty());
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
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y2 LOGIN luzr sikrit\r\n") );
    QCOMPARE( authSpy->size(), 1 );
    SOCK->fakeReading( "y2 OK [CAPABILITY IMAP4rev1] logged in\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE( authSpy->size(), 1 );
    QCOMPARE(startTlsUpgradeSpy->size(), 1);
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
    QVERIFY(startTlsUpgradeSpy->isEmpty());
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
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y3 LOGIN luzr sikrit\r\n") );
    QCOMPARE( authSpy->size(), 1 );
    SOCK->fakeReading( "y3 OK [CAPABILITY IMAP4rev1] logged in\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( completedSpy->size(), 1 );
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE( authSpy->size(), 1 );
    QCOMPARE(startTlsUpgradeSpy->size(), 1);
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
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("[*** STARTTLS ***]y2 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    QVERIFY( authSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny2 OK capability completed\r\n" );
    QCoreApplication::processEvents();
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
    QVERIFY(startTlsUpgradeSpy->isEmpty());
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
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("[*** STARTTLS ***]y2 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    QVERIFY( authSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny2 OK capability completed\r\n" );
    QCoreApplication::processEvents();
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
    QVERIFY(startTlsUpgradeSpy->isEmpty());
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
    QVERIFY(startTlsUpgradeSpy->isEmpty());
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
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("[*** STARTTLS ***]y1 CAPABILITY\r\n") );
    QVERIFY( completedSpy->isEmpty() );
    QVERIFY( authSpy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny1 OK capability completed\r\n" );
    QCoreApplication::processEvents();
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
    QVERIFY(startTlsUpgradeSpy->isEmpty());
}

/** @short Test how COMPRESS=DEFLATE gets activated and its interaction with further tasks */
void ImapModelOpenConnectionTest::testCompressDeflateOk()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading("* OK [capability imap4rev1] hi there\r\n");
    QVERIFY(completedSpy->isEmpty());
    QCoreApplication::processEvents();
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
#if TROJITA_COMPRESS_DEFLATE
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y1 COMPRESS DEFLATE\r\n"));
    SOCK->fakeReading("y1 OK compressing\r\n");
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QByteArray("[*** DEFLATE ***]"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y2 ID NIL\r\n"));
    SOCK->fakeReading("* ID nil\r\ny2 OK you courious peer\r\n");
#else
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y1 ID NIL\r\n"));
    SOCK->fakeReading("* ID nil\r\ny1 OK you courious peer\r\n");
#endif
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QVERIFY(startTlsUpgradeSpy->isEmpty());
}

/** @short Test that denied COMPRESS=DEFLATE doesn't result in compression being active */
void ImapModelOpenConnectionTest::testCompressDeflateNo()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading("* OK [capability imap4rev1] hi there\r\n");
    QVERIFY(completedSpy->isEmpty());
    QCoreApplication::processEvents();
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
#if TROJITA_COMPRESS_DEFLATE
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y1 COMPRESS DEFLATE\r\n"));
    SOCK->fakeReading("y1 NO I just don't want to\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y2 ID NIL\r\n"));
    SOCK->fakeReading("* ID nil\r\ny2 OK you courious peer\r\n");
#else
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y1 ID NIL\r\n"));
    SOCK->fakeReading("* ID nil\r\ny1 OK you courious peer\r\n");
#endif
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QVERIFY(startTlsUpgradeSpy->isEmpty());
}

/** @short Make sure that as long as the OpenConnectionTask has not finished its job, nothing else will get queued */
void ImapModelOpenConnectionTest::testOpenConnectionShallBlock()
{
    model->rowCount(QModelIndex());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading("* OK [capability imap4rev1] hi there\r\n");
    QVERIFY(completedSpy->isEmpty());
    QCoreApplication::processEvents();
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
#if TROJITA_COMPRESS_DEFLATE
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y1 COMPRESS DEFLATE\r\n"));
    SOCK->fakeReading("y1 NO I just don't want to\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y2 ID NIL\r\ny3 LIST \"\" \"%\"\r\n"));
    SOCK->fakeReading("* ID nil\r\ny3 OK listed, nothing like that in there\r\ny2 OK you courious peer\r\n");
#else
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y1 ID NIL\r\ny2 LIST \"\" \"%\"\r\n"));
    SOCK->fakeReading("* ID nil\r\ny2 OK listed, nothing like that in there\r\ny1 OK you courious peer\r\n");
#endif
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QVERIFY(startTlsUpgradeSpy->isEmpty());
}

/** @short Test that no tasks can skip over a task which is blocking for login */
void ImapModelOpenConnectionTest::testLoginDelaysOtherTasks()
{
    using namespace Imap::Mailbox;
    MemoryCache *cache = dynamic_cast<MemoryCache *>(model->cache());
    Q_ASSERT(cache);
    cache->setChildMailboxes(QString(),
                             QList<MailboxMetadata>() << MailboxMetadata(QString::fromUtf8("a"), QString(), QStringList())
                             << MailboxMetadata(QString::fromUtf8("b"), QString(), QStringList())
                             );
    m_enableAutoLogin = false;
    // The cache is not queried synchronously
    QCOMPARE(model->rowCount(QModelIndex()), 1);
    QCoreApplication::processEvents();
    // The cache should have been consulted by now
    QCOMPARE(model->rowCount(QModelIndex()), 3);
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading("* OK [capability imap4rev1] hi there\r\n");
    QVERIFY(completedSpy->isEmpty());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // The login must not be sent (as per m_enableAutoLogin being false)
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QModelIndex mailboxA = model->index(1, 0, QModelIndex());
    QVERIFY(mailboxA.isValid());
    QCOMPARE(mailboxA.data(RoleMailboxName).toString(), QString::fromUtf8("a"));
    QModelIndex mailboxB = model->index(2, 0, QModelIndex());
    QVERIFY(mailboxB.isValid());
    QCOMPARE(mailboxB.data(RoleMailboxName).toString(), QString::fromUtf8("b"));
    QModelIndex msgListA = mailboxA.child(0, 0);
    Q_ASSERT(msgListA.isValid());
    QModelIndex msgListB = mailboxB.child(0, 0);
    Q_ASSERT(msgListB.isValid());

    // Request syncing the mailboxes
    QCOMPARE(model->rowCount(msgListA), 0);
    QCOMPARE(model->rowCount(msgListB), 0);
    for (int i = 0; i < 10; ++i)
        QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());

    // Unblock the login
    m_enableAutoLogin = true;
    provideAuthDetails();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y0 LOGIN luzr sikrit\r\n"));
    SOCK->fakeReading("y0 OK [CAPABILITY IMAP4rev1] logged in\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // FIXME: selecting the "b" shall definitely wait until "a" is completed
    QCOMPARE(SOCK->writtenStuff(), QByteArray("y1 LIST \"\" \"%\"\r\ny2 SELECT a\r\ny3 SELECT b\r\n"));
    SOCK->fakeReading("y1 OK listed, nothing like that in there\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QVERIFY(startTlsUpgradeSpy->isEmpty());
}

/** @short Test that we respect an initial BYE and don't proceed with login */
void ImapModelOpenConnectionTest::testInitialBye()
{
    model->rowCount(QModelIndex());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading("* BYE Sorry, we're offline\r\n");
    for (int i = 0; i < 10; ++i)
        QCoreApplication::processEvents();
    QCOMPARE(failedSpy->size(), 1);
    QVERIFY(completedSpy->isEmpty());
    QVERIFY(connErrorSpy->isEmpty());
    QVERIFY(startTlsUpgradeSpy->isEmpty());
}

/** @short Test how we react on some crazy garbage instead of a proper IMAP4 greeting */
void ImapModelOpenConnectionTest::testInitialGarbage()
{
    QFETCH(QByteArray, greetings);

    model->rowCount(QModelIndex());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    SOCK->fakeReading(greetings);
    for (int i = 0; i < 10; ++i)
        QCoreApplication::processEvents();
    //qDebug() << QList<QVariantList>(*connErrorSpy);
    QCOMPARE(connErrorSpy->size(), 1);
    QCOMPARE(failedSpy->size(), 1);
    QVERIFY(completedSpy->isEmpty());
    QVERIFY(startTlsUpgradeSpy->isEmpty());
}

void ImapModelOpenConnectionTest::testInitialGarbage_data()
{
    QTest::addColumn<QByteArray>("greetings");

    QTest::newRow("utter-garbage")
        << QByteArray("blesmrt trojita\r\n");

    QTest::newRow("pop3")
        << QByteArray("+OK InterMail POP3 server ready.\r\n");
}

// FIXME: verify how LOGINDISABLED even after STARTLS ends up

void ImapModelOpenConnectionTest::provideAuthDetails()
{
    if (m_enableAutoLogin) {
        model->setImapUser(QLatin1String("luzr"));
        model->setImapPassword(QLatin1String("sikrit"));
    }
}


TROJITA_HEADLESS_TEST( ImapModelOpenConnectionTest )
