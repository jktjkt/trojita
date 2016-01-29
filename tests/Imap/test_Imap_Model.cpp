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
#include "Common/MetaTypes.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/MailboxModel.h"

void ImapModelTest::initTestCase()
{
    Common::registerMetaTypes();
    mboxModel = 0;
}

void ImapModelTest::init()
{
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache(this);
    factory = new Streams::FakeSocketFactory(Imap::CONN_STATE_CONNECTED_PRETLS_PRECAPS);
    Imap::Mailbox::TaskFactoryPtr taskFactory(new Imap::Mailbox::TaskFactory());
    model = new Imap::Mailbox::Model(this, cache, Imap::Mailbox::SocketFactoryPtr(factory), std::move(taskFactory));
    setupLogging();
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    QCoreApplication::processEvents();
}

void ImapModelTest::testSyncMailbox()
{
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    cServer("* PREAUTH [CAPABILITY Imap4Rev1] foo\r\n");
    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    cServer("* LIST (\\HasNoChildren) \".\" \"INBOX\"\r\n"
            "* CAPABILITY IMAP4rev1\r\n"
            + t.last("ok list completed\r\n"));
    QModelIndex inbox = model->index(1, 0, QModelIndex());
    QCOMPARE(model->data( inbox, Qt::DisplayRole), QVariant("INBOX"));
    cEmpty();

    // further tests are especially in the Imap_Task_ObtainSynchronizedMailboxTest
}

void ImapModelTest::testInboxCaseSensitivity()
{
    mboxModel = new Imap::Mailbox::MailboxModel(this, model);
    mboxModel->rowCount(QModelIndex());
    QCoreApplication::processEvents();
    cServer("* PREAUTH [Capability imap4rev1] foo\r\n");
    t.reset();
    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    cServer("* LIST (\\Noinferiors) \".\" \"Inbox\"\r\n"
            + t.last("ok list completed\r\n"));
    QCOMPARE(mboxModel->data(mboxModel->index(0, 0, QModelIndex()), Qt::DisplayRole), QVariant("INBOX"));
    cEmpty();
    mboxModel->deleteLater();
    mboxModel = 0;
}

void ImapModelTest::testCreationDeletionHandling()
{
    // Start the conversation
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    cServer("* PREAUTH [CAPABILITY imap4rev1] foo\r\n");

    // Ask for capabilities and list top-level mailboxes
    // These commands are interleaved with each other
    t.reset();
    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    cServer("* LIST (\\HasNoChildren) \".\" \"INBOX\"\r\n"
            "* LIST (\\HasChildren) \".\" \"SomeParent\"\r\n"
            "* LIST (\\HasNoChildren) \".\" \"one\"\r\n"
            "* LIST (\\HasNoChildren) \".\" two\r\n"
            + t.last("ok list completed\r\n"));

    // Note that the ordering is case-insensitive
    QModelIndex mbox_inbox = model->index(1, 0, QModelIndex());
    QModelIndex mbox_one = model->index(2, 0, QModelIndex());
    QModelIndex mbox_SomeParent = model->index(3, 0, QModelIndex());
    QModelIndex mbox_two = model->index(4, 0, QModelIndex());
    QCOMPARE( model->data(mbox_inbox, Qt::DisplayRole), QVariant("INBOX"));
    QCOMPARE( model->data(mbox_one, Qt::DisplayRole), QVariant("one"));
    QCOMPARE( model->data(mbox_two, Qt::DisplayRole), QVariant("two"));
    QCOMPARE( model->data(mbox_SomeParent, Qt::DisplayRole), QVariant("SomeParent"));

    // Try to create mailbox
    model->createMailbox(QStringLiteral("zzz_newly-Created"));
    cClient(t.mk("CREATE zzz_newly-Created\r\n"));

    // Sane invariants
    QSignalSpy creationFailed(model, SIGNAL(mailboxCreationFailed(QString,QString)));
    QVERIFY(creationFailed.isValid());
    QSignalSpy creationSucceded(model, SIGNAL(mailboxCreationSucceded(QString)));
    QVERIFY(creationSucceded.isValid());
    QSignalSpy deletionFailed(model, SIGNAL(mailboxDeletionFailed(QString,QString)));
    QVERIFY(deletionFailed.isValid());
    QSignalSpy deletionSucceded(model, SIGNAL(mailboxDeletionSucceded(QString)));
    QVERIFY(deletionSucceded.isValid());

    // Test that we handle failure of the CREATE command
    cServer(t.last("NO go away\r\n"));
    QCOMPARE(creationFailed.count(), 1);
    QList<QVariant> args = creationFailed.takeFirst();
    QCOMPARE(args.size(), 2);
    QCOMPARE(args[0], QVariant("zzz_newly-Created"));
    QCOMPARE(args[1], QVariant("go away"));
    QCOMPARE(creationSucceded.count(), 0);
    QCOMPARE(deletionFailed.count(), 0);
    QCOMPARE(deletionSucceded.count(), 0);

    // Now test its successful completion
    model->createMailbox(QStringLiteral("zzz_newly-Created2"));
    cClient(t.mk("CREATE zzz_newly-Created2\r\n"));
    cServer(t.last("OK mailbox created\r\n"));
    cClient(t.mk("LIST \"\" zzz_newly-Created2\r\n"));
    cServer("* LIST (\\HasNoChildren) \".\" zzz_newly-Created2\r\n"
            + t.last("OK x\r\n"));
    QCOMPARE(creationFailed.count(), 0);
    QCOMPARE(creationSucceded.count(), 1);
    args = creationSucceded.takeFirst();
    QCOMPARE(args.size(), 1);
    QCOMPARE(args[0], QVariant("zzz_newly-Created2"));
    QCOMPARE(deletionFailed.count(), 0);
    QCOMPARE(deletionSucceded.count(), 0);
    QModelIndex mbox_zzz = model->index(5, 0, QModelIndex());
    QCOMPARE(model->data(mbox_zzz, Qt::DisplayRole), QVariant("zzz_newly-Created2"));
    cEmpty();

    // Verify automated subscription
    model->createMailbox(QStringLiteral("zzz_newly-Created3"), Imap::Mailbox::AutoSubscription::SUBSCRIBE);
    cClient(t.mk("CREATE zzz_newly-Created3\r\n"));
    cServer(t.last("OK created\r\n"));
    cClient(t.mk("LIST \"\" zzz_newly-Created3\r\n"));
    cServer("* LIST (\\HasNoChildren) \".\" zzz_newly-Created3\r\n"
            + t.last("OK listed\r\n"));
    cClient(t.mk("SUBSCRIBE zzz_newly-Created3\r\n"));
    cServer(t.last("OK subscribed\r\n"));
    QModelIndex mbox_created3 = model->index(6, 0, QModelIndex());
    QVERIFY(mbox_created3.isValid());
    QCOMPARE(mbox_created3.data(Qt::DisplayRole), QVariant("zzz_newly-Created3"));
    QCOMPARE(mbox_created3.data(Imap::Mailbox::RoleMailboxIsSubscribed), QVariant(true));
    QCOMPARE(mbox_inbox.data(Imap::Mailbox::RoleMailboxIsSubscribed), QVariant(false));
    cEmpty();
}

QTEST_GUILESS_MAIN(ImapModelTest)
