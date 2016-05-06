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
#include "Common/MetaTypes.h"
#include "Streams/FakeSocket.h"
#include "Streams/TrojitaZlibStatus.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/MailboxModel.h"
#include "Imap/Tasks/OpenConnectionTask.h"

void ImapModelOpenConnectionTest::initTestCase()
{
    LibMailboxSync::initTestCase();
    qRegisterMetaType<Imap::Mailbox::ImapTask*>();
    completedSpy = 0;
    m_enableAutoLogin = true;
}

void ImapModelOpenConnectionTest::init()
{
    reinit(TlsRequired::No);
}

void ImapModelOpenConnectionTest::reinit(const TlsRequired tlsRequired)
{
    m_fakeListCommand = false;
    m_fakeOpenTask = false;
    m_startTlsRequired = tlsRequired == TlsRequired::Yes;
    m_initialConnectionState = Imap::CONN_STATE_CONNECTED_PRETLS_PRECAPS;
    LibMailboxSync::init();
    model->setProperty("trojita-imap-id-no-versions", QVariant(true));
    connect(model, &Imap::Mailbox::Model::authRequested, this, &ImapModelOpenConnectionTest::provideAuthDetails, Qt::QueuedConnection);
    connect(model, &Imap::Mailbox::Model::needsSslDecision, this, &ImapModelOpenConnectionTest::acceptSsl, Qt::QueuedConnection);
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    QCoreApplication::processEvents();
    task = new Imap::Mailbox::OpenConnectionTask(model);
    completedSpy = new QSignalSpy(task, SIGNAL(completed(Imap::Mailbox::ImapTask*)));
    failedSpy = new QSignalSpy(task, SIGNAL(failed(QString)));
    authSpy = new QSignalSpy(model, SIGNAL(authRequested()));
    connErrorSpy = new QSignalSpy(model, SIGNAL(imapError(QString)));
    startTlsUpgradeSpy = new QSignalSpy(model, SIGNAL(requireStartTlsInFuture()));
    authErrorSpy = new QSignalSpy(model, SIGNAL(imapAuthErrorChanged(const QString&)));
    t.reset();
}

void ImapModelOpenConnectionTest::acceptSsl(const QList<QSslCertificate> &certificateChain, const QList<QSslError> &sslErrors)
{
    model->setSslPolicy(certificateChain, sslErrors, true);
}

/** @short Test for explicitly obtaining capability when greeted by PREAUTH */
void ImapModelOpenConnectionTest::testPreauth()
{
    cEmpty();
    cServer("* PREAUTH foo\r\n");
    QVERIFY(completedSpy->isEmpty());
    cClient(t.mk("CAPABILITY\r\n"));
    QVERIFY(completedSpy->isEmpty());
    cServer("* CAPABILITY IMAP4rev1\r\n"
            + t.last("OK capability completed\r\n"));
    cEmpty();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QVERIFY(authSpy->isEmpty());
    QVERIFY(startTlsUpgradeSpy->isEmpty());
}

/** @short Test that we can obtain capability when embedded in PREAUTH */
void ImapModelOpenConnectionTest::testPreauthWithCapability()
{
    cEmpty();
    cServer("* PREAUTH [CAPABILITY IMAP4rev1] foo\r\n");
    cEmpty();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QVERIFY(authSpy->isEmpty());
    QVERIFY(startTlsUpgradeSpy->isEmpty());
}

/** @short What happens when the server responds with PREAUTH and we want STARTTLS? */
void ImapModelOpenConnectionTest::testPreauthWithStartTlsWanted()
{
    reinit(TlsRequired::Yes);

    cEmpty();
    cServer("* PREAUTH hi there\r\n");
    QCOMPARE(failedSpy->size(), 1);
    QVERIFY(completedSpy->isEmpty());
    QVERIFY(authSpy->isEmpty());
    QVERIFY(startTlsUpgradeSpy->isEmpty());
    cClient(t.mk("LOGOUT\r\n"));
    cEmpty();
}

/** @short Test for obtaining capability and logging in without any STARTTLS */
void ImapModelOpenConnectionTest::testOk()
{
    cEmpty();
    cServer("* OK foo\r\n");
    QVERIFY(completedSpy->isEmpty());
    cClient(t.mk("CAPABILITY\r\n"));
    QVERIFY(completedSpy->isEmpty());
    cServer("* CAPABILITY IMAP4rev1\r\n"
            + t.last("OK capability completed\r\n"));
    QCOMPARE(authSpy->size(), 1);
    cClient(t.mk("LOGIN luzr sikrit\r\n"));
    cServer(t.last("OK [CAPABILITY IMAP4rev1] logged in\r\n"));
    cEmpty();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(startTlsUpgradeSpy->isEmpty());
    QCOMPARE(authErrorSpy->size(), 0);
    QCOMPARE(model->imapAuthError(), QString());
}

/** @short Test with capability inside the OK greetings, no STARTTLS */
void ImapModelOpenConnectionTest::testOkWithCapability()
{
    cEmpty();
    cServer("* OK [CAPABILITY IMAP4rev1] foo\r\n");
    QVERIFY(completedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    cClient(t.mk("LOGIN luzr sikrit\r\n"));
    cServer(t.last("OK [CAPABILITY IMAP4rev1] logged in\r\n"));
    cEmpty();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(startTlsUpgradeSpy->isEmpty());
    QCOMPARE(authErrorSpy->size(), 0);
    QCOMPARE(model->imapAuthError(), QString());
}

/** @short See what happens when the capability response doesn't contain IMAP4rev1 capability */
void ImapModelOpenConnectionTest::testOkMissingImap4rev1()
{
    {
        ExpectSingleErrorHere blocker(this);
        cServer("* OK [CAPABILITY pwn] foo\r\n");
    }
    cClient(t.mk("LOGOUT\r\n"));
    cEmpty();
    QVERIFY(authSpy->isEmpty());
    QVERIFY(startTlsUpgradeSpy->isEmpty());
}

/** @short Test to honor embedded LOGINDISABLED */
void ImapModelOpenConnectionTest::testOkLogindisabled()
{
    cEmpty();
    cServer("* OK [CAPABILITY IMAP4rev1 starttls LoginDisabled] foo\r\n");
    QVERIFY(completedSpy->isEmpty());
    QVERIFY(authSpy->isEmpty());
    cClient(t.mk("STARTTLS\r\n"));
    cServer(t.last("OK will establish secure layer immediately\r\n"));
    cClient("[*** STARTTLS ***]"
            + t.mk("CAPABILITY\r\n"));
    QVERIFY(authSpy->isEmpty());
    QVERIFY(completedSpy->isEmpty());
    QVERIFY(authSpy->isEmpty());
    cServer("* CAPABILITY IMAP4rev1\r\n"
            + t.last("OK capability completed\r\n"));
    cClient(t.mk("LOGIN luzr sikrit\r\n"));
    QCOMPARE(authSpy->size(), 1);
    cServer(t.last("OK [CAPABILITY IMAP4rev1] logged in\r\n"));
    cEmpty();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QCOMPARE(startTlsUpgradeSpy->size(), 1);
    QCOMPARE(authErrorSpy->size(), 0);
    QCOMPARE(model->imapAuthError(), QString());
}

/** @short Test how LOGINDISABLED without a corresponding STARTTLS in the capabilities end up */
void ImapModelOpenConnectionTest::testOkLogindisabledWithoutStarttls()
{
    cEmpty();
    cServer("* OK [CAPABILITY IMAP4rev1 LoginDisabled] foo\r\n");
    // The capabilities do not contain STARTTLS but LOGINDISABLED is in there
    cClient(t.mk("LOGOUT\r\n"))
    QVERIFY(completedSpy->isEmpty());
    QVERIFY(authSpy->isEmpty());
    QCOMPARE(failedSpy->size(), 1);
    QVERIFY(startTlsUpgradeSpy->isEmpty());
}

/** @short Test for an explicit CAPABILITY retrieval and automatic STARTTLS when LOGINDISABLED */
void ImapModelOpenConnectionTest::testOkLogindisabledLater()
{
    cEmpty();
    cServer("* OK foo\r\n");
    QVERIFY(completedSpy->isEmpty());
    cClient(t.mk("CAPABILITY\r\n"));
    QVERIFY(completedSpy->isEmpty() );
    cServer("* CAPABILITY IMAP4rev1 starttls LoGINDISABLED\r\n"
            + t.last("OK capability completed\r\n"));
    QVERIFY(authSpy->isEmpty());
    cClient(t.mk("STARTTLS\r\n"));
    cServer(t.last("OK will establish secure layer immediately\r\n"));
    QVERIFY(authSpy->isEmpty());
    cClient(QByteArray("[*** STARTTLS ***]")
            + t.mk("CAPABILITY\r\n"));
    QVERIFY(completedSpy->isEmpty());
    QVERIFY(authSpy->isEmpty());
    cServer("* CAPABILITY IMAP4rev1\r\n" + t.last("OK capability completed\r\n"));
    cClient(t.mk("LOGIN luzr sikrit\r\n"));
    QCOMPARE(authSpy->size(), 1);
    cServer(t.last("OK [CAPABILITY IMAP4rev1] logged in\r\n"));
    cEmpty();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QCOMPARE(startTlsUpgradeSpy->size(), 1);
    QCOMPARE(authErrorSpy->size(), 0);
    QCOMPARE(model->imapAuthError(), QString());
}

/** @short Test conf-requested STARTTLS when not faced with embedded capabilities in OK greetings */
void ImapModelOpenConnectionTest::testOkStartTls()
{
    reinit(TlsRequired::Yes);

    cEmpty();
    cServer("* OK foo\r\n");
    QVERIFY(completedSpy->isEmpty());
    cClient(t.mk("CAPABILITY\r\n"));
    cServer("* CAPABILITY imap4rev1 starttls\r\n"
            + t.last("ok cap\r\n"));
    QVERIFY(authSpy->isEmpty());
    cClient(t.mk("STARTTLS\r\n"));
    cServer(t.last("OK will establish secure layer immediately\r\n"));
    QVERIFY(authSpy->isEmpty());
    cClient("[*** STARTTLS ***]"
            + t.mk("CAPABILITY\r\n"));
    QVERIFY(completedSpy->isEmpty());
    QVERIFY(authSpy->isEmpty());
    cServer("* CAPABILITY IMAP4rev1\r\n"
            + t.last("OK capability completed\r\n"));
    cClient(t.mk("LOGIN luzr sikrit\r\n"));
    QCOMPARE(authSpy->size(), 1);
    cServer(t.last("OK [CAPABILITY IMAP4rev1] logged in\r\n"));
    cEmpty();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(startTlsUpgradeSpy->isEmpty());
    QCOMPARE(authErrorSpy->size(), 0);
    QCOMPARE(model->imapAuthError(), QString());
}

/** @short Test that an untagged CAPABILITY after LOGIN prevents an extra CAPABILITY command */
void ImapModelOpenConnectionTest::testCapabilityAfterLogin()
{
    reinit(TlsRequired::Yes);

    cEmpty();
    cServer("* OK foo\r\n");
    QVERIFY(completedSpy->isEmpty());
    cClient(t.mk("CAPABILITY\r\n"));
    cServer("* CAPABILITY imap4rev1 starttls\r\n"
            + t.last("ok cap\r\n"));
    QVERIFY(authSpy->isEmpty());
    cClient(t.mk("STARTTLS\r\n"));
    cServer(t.last("OK will establish secure layer immediately\r\n"));
    QVERIFY(authSpy->isEmpty());
    cClient("[*** STARTTLS ***]"
            + t.mk("CAPABILITY\r\n"));
    QVERIFY(completedSpy->isEmpty());
    QVERIFY(authSpy->isEmpty());
    cServer("* CAPABILITY IMAP4rev1\r\n"
            + t.last("OK capability completed\r\n"));
    cClient(t.mk("LOGIN luzr sikrit\r\n"));
    QCOMPARE(authSpy->size(), 1);
    cServer("* CAPABILITY imap4rev1\r\n" + t.last("OK logged in\r\n"));
    cEmpty();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(startTlsUpgradeSpy->isEmpty());
    QCOMPARE(authErrorSpy->size(), 0);
    QCOMPARE(model->imapAuthError(), QString());
}

/** @short Test conf-requested STARTTLS when the server doesn't support STARTTLS at all */
void ImapModelOpenConnectionTest::testOkStartTlsForbidden()
{
    reinit(TlsRequired::Yes);

    cEmpty();
    cServer("* OK foo\r\n");
    QVERIFY(completedSpy->isEmpty());
    cClient(t.mk("CAPABILITY\r\n"));
    cServer("* CAPABILITY imap4rev1\r\n"
            + t.last("ok cap\r\n"));
    cClient(t.mk("LOGOUT\r\n"));
    QVERIFY(authSpy->isEmpty());
    QCOMPARE(failedSpy->size(), 1);
    QVERIFY(authSpy->isEmpty());
    QVERIFY(startTlsUpgradeSpy->isEmpty());
}

/** @short Test to re-request formerly embedded capabilities when launching STARTTLS */
void ImapModelOpenConnectionTest::testOkStartTlsDiscardCaps()
{
    reinit(TlsRequired::Yes);

    cEmpty();
    cServer("* OK [Capability imap4rev1 starttls] foo\r\n");
    QVERIFY(completedSpy->isEmpty());
    QVERIFY(authSpy->isEmpty());
    cClient(t.mk("STARTTLS\r\n"));
    cServer(t.last("OK will establish secure layer immediately\r\n"));
    QVERIFY(authSpy->isEmpty());
    cClient("[*** STARTTLS ***]" + t.mk("CAPABILITY\r\n"));
    QVERIFY(completedSpy->isEmpty());
    QVERIFY(authSpy->isEmpty());
    cServer("* CAPABILITY IMAP4rev1\r\n" + t.last("OK capability completed\r\n"));
    cClient(t.mk("LOGIN luzr sikrit\r\n"));
    QCOMPARE(authSpy->size(), 1);
    cServer(t.last("OK [CAPABILITY IMAP4rev1] logged in\r\n"));
    cEmpty();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(startTlsUpgradeSpy->isEmpty());
    QCOMPARE(authErrorSpy->size(), 0);
    QCOMPARE(model->imapAuthError(), QString());
}

/** @short Test how COMPRESS=DEFLATE gets activated and its interaction with further tasks */
void ImapModelOpenConnectionTest::testCompressDeflateOk()
{
    cEmpty();
    cServer("* OK [capability imap4rev1] hi there\r\n");
    QVERIFY(completedSpy->isEmpty());
    cClient(t.mk("LOGIN luzr sikrit\r\n"));
    QCOMPARE(authSpy->size(), 1);
    cServer(t.last("OK [CAPABILITY IMAP4rev1 compress=deflate id] logged in\r\n"));
#if TROJITA_COMPRESS_DEFLATE
    cClient(t.mk("COMPRESS DEFLATE\r\n"));
    cServer(t.last("OK compressing\r\n"));
    cClient("[*** DEFLATE ***]" + t.mk("ID (\"name\" \"Trojita\")\r\n"));
    cServer("* ID nil\r\n" + t.last("OK you courious peer\r\n"));
#else
    cClient(t.mk("ID (\"name\" \"Trojita\")\r\n"));
    cServer("* ID nil\r\n" + t.last("OK you courious peer\r\n"));
#endif
    cEmpty();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(startTlsUpgradeSpy->isEmpty());
    QCOMPARE(authErrorSpy->size(), 0);
    QCOMPARE(model->imapAuthError(), QString());
}

/** @short Test that denied COMPRESS=DEFLATE doesn't result in compression being active */
void ImapModelOpenConnectionTest::testCompressDeflateNo()
{
    cEmpty();
    cServer("* OK [capability imap4rev1] hi there\r\n");
    QVERIFY(completedSpy->isEmpty());
    cClient(t.mk("LOGIN luzr sikrit\r\n"));
    QCOMPARE(authSpy->size(), 1);
    cServer(t.last("OK [CAPABILITY IMAP4rev1 compress=deflate id] logged in\r\n"));
#if TROJITA_COMPRESS_DEFLATE
    cClient(t.mk("COMPRESS DEFLATE\r\n"));
    cServer(t.last("NO I just don't want to\r\n"));
    cClient(t.mk("ID (\"name\" \"Trojita\")\r\n"));
    cServer("* ID nil\r\n"
            + t.last("OK you courious peer\r\n"));
#else
    cClient(t.mk("ID (\"name\" \"Trojita\")\r\n"));
    cServer("* ID nil\r\n"
            + t.last("OK you courious peer\r\n"));
#endif
    cEmpty();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(startTlsUpgradeSpy->isEmpty());
    QCOMPARE(authErrorSpy->size(), 0);
    QCOMPARE(model->imapAuthError(), QString());
}

/** @short Make sure that as long as the OpenConnectionTask has not finished its job, nothing else will get queued */
void ImapModelOpenConnectionTest::testOpenConnectionShallBlock()
{
    model->rowCount(QModelIndex());
    cEmpty();
    cServer("* OK [capability imap4rev1] hi there\r\n");
    QVERIFY(completedSpy->isEmpty());
    cClient(t.mk("LOGIN luzr sikrit\r\n"));
    QCOMPARE(authSpy->size(), 1);
    cServer(t.last("OK [CAPABILITY IMAP4rev1 compress=deflate id] logged in\r\n"));
#if TROJITA_COMPRESS_DEFLATE
    cClient(t.mk("COMPRESS DEFLATE\r\n"));
    cServer(t.last("NO I just don't want to\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
#endif
    auto c1 = t.mk("ID (\"name\" \"Trojita\")\r\n");
    auto r1 = t.last("OK you courious peer\r\n");
    auto c2 = t.mk("LIST \"\" \"%\"\r\n");
    auto r2 = t.last("OK listed, nothing like that in there\r\n");
    cClient(c1 + c2);
    cServer("* ID nil\r\n" + r2 + r1);
    cEmpty();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(startTlsUpgradeSpy->isEmpty());
    QCOMPARE(authErrorSpy->size(), 0);
    QCOMPARE(model->imapAuthError(), QString());
}

/** @short Test that no tasks can skip over a task which is blocking for login */
void ImapModelOpenConnectionTest::testLoginDelaysOtherTasks()
{
    using namespace Imap::Mailbox;
    MemoryCache *cache = dynamic_cast<MemoryCache *>(model->cache());
    Q_ASSERT(cache);
    cache->setChildMailboxes(QString(),
                             QList<MailboxMetadata>() << MailboxMetadata(QLatin1String("a"), QString(), QStringList())
                             << MailboxMetadata(QLatin1String("b"), QString(), QStringList())
                             );
    m_enableAutoLogin = false;
    // The cache is not queried synchronously
    QCOMPARE(model->rowCount(QModelIndex()), 1);
    QCoreApplication::processEvents();
    // The cache should have been consulted by now
    QCOMPARE(model->rowCount(QModelIndex()), 3);
    cEmpty();
    cServer("* OK [capability imap4rev1] hi there\r\n");
    QVERIFY(completedSpy->isEmpty());
    // The login must not be sent (as per m_enableAutoLogin being false)
    cEmpty();
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
    cEmpty();

    // Unblock the login
    m_enableAutoLogin = true;
    provideAuthDetails();
    cClient(t.mk("LOGIN luzr sikrit\r\n"));
    cServer(t.last("OK [CAPABILITY IMAP4rev1] logged in\r\n"));
    // FIXME: selecting the "b" shall definitely wait until "a" is completed
    auto c1 = t.mk("LIST \"\" \"%\"\r\n");
    auto r1 = t.last("OK listed, nothing like that in there\r\n");
    auto c2 = t.mk("SELECT a\r\n");
    auto c3 = t.mk("SELECT b\r\n");
    cClient(c1 + c2 + c3);
    cServer(r1);
    cEmpty();
    QCOMPARE(completedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    QVERIFY(startTlsUpgradeSpy->isEmpty());
    QCOMPARE(authErrorSpy->size(), 0);
    QCOMPARE(model->imapAuthError(), QString());
}

/** @short Test that we respect an initial BYE and don't proceed with login */
void ImapModelOpenConnectionTest::testInitialBye()
{
    model->rowCount(QModelIndex());
    cEmpty();
    cServer("* BYE Sorry, we're offline\r\n");
    cClient(t.mk("LOGOUT\r\n"));
    cEmpty();
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
    cEmpty();
    {
        ExpectSingleErrorHere blocker(this);
        cServer(greetings);
    }
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

void ImapModelOpenConnectionTest::testAuthFailure()
{
    cEmpty();
    cServer("* OK [capability imap4rev1] Serivce Ready\r\n");
    QVERIFY(completedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    cClient(t.mk("LOGIN luzr sikrit\r\n"));
    QCOMPARE(authSpy->size(), 1);
    cServer(t.last("NO [AUTHENTICATIONFAILED] foobar\r\n"));
    QCOMPARE(completedSpy->size(), 0);
    QCOMPARE(authSpy->size(), 2);
    QCOMPARE(authErrorSpy->size(), 1);
    QVERIFY(model->imapAuthError().contains("foobar"));
}

void ImapModelOpenConnectionTest::testAuthFailureNoRespCode()
{
    cEmpty();
    cServer("* OK [capability imap4rev1] Service Ready\r\n");
    QVERIFY(completedSpy->isEmpty());
    QCOMPARE(authSpy->size(), 1);
    cClient(t.mk("LOGIN luzr sikrit\r\n"));
    QCOMPARE(authSpy->size(), 1);
    cServer(t.last("NO Derp\r\n"));
    QCOMPARE(completedSpy->size(), 0);
    QCOMPARE(authSpy->size(), 2);
    QCOMPARE(authErrorSpy->size(), 1);
    QVERIFY(model->imapAuthError().contains("Derp"));
}

/** @short Ensure that network reconnects do not lead to a huge number of password prompts

https://bugs.kde.org/show_bug.cgi?id=362477
*/
void ImapModelOpenConnectionTest::testExcessivePasswordPrompts()
{
    cEmpty();
    m_enableAutoLogin = false;
    cServer("* OK [capability imap4rev1] Service Ready\r\n");
    cEmpty();
    QCOMPARE(authSpy->size(), 1);
    SOCK->fakeDisconnect(QStringLiteral("Fake network going down"));
    for (int i = 0; i < 10; ++i) {
        QCoreApplication::processEvents();
    }
    // SOCK is now unusable
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    model->rowCount(QModelIndex());
    QCoreApplication::processEvents();
    // SOCK is now back to a usable state
    cServer("* OK [capability imap4rev1] Service Ready\r\n");
    QCOMPARE(authSpy->size(), 1);
    model->setImapUser(QStringLiteral("a"));
    model->setImapPassword(QStringLiteral("b"));
    cClient(t.mk("LOGIN a b\r\n"));
    QCOMPARE(authSpy->size(), 1);
    cServer(t.last("OK [CAPABILITY imap4rev1] logged in\r\n"));
    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    cServer(t.last("OK listed\r\n"));
    cEmpty();
}

// FIXME: verify how LOGINDISABLED even after STARTLS ends up

void ImapModelOpenConnectionTest::provideAuthDetails()
{
    if (m_enableAutoLogin) {
        model->setImapUser(QStringLiteral("luzr"));
        model->setImapPassword(QStringLiteral("sikrit"));
    }
}


QTEST_GUILESS_MAIN( ImapModelOpenConnectionTest )
