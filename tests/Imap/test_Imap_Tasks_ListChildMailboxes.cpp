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
#include "test_Imap_Tasks_ListChildMailboxes.h"
#include "Common/MetaTypes.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxFinder.h"
#include "Imap/Model/MailboxModel.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/Model.h"
#include "Imap/Tasks/Fake_ListChildMailboxesTask.h"

void ImapModelListChildMailboxesTest::init()
{
    m_fakeListCommand = false;
    LibMailboxSync::init();

    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    QCoreApplication::processEvents();
    t.reset();
}

void ImapModelListChildMailboxesTest::testSimpleListing()
{
    QCOMPARE(model->rowCount(QModelIndex()), 1);
    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    cServer("* LIST (\\HasNoChildren) \".\" \"b\"\r\n"
            "* LIST (\\HasChildren) \".\" \"a\"\r\n"
            "* LIST (\\Noselect \\HasChildren) \".\" \"xyz\"\r\n"
            "* LIST (\\HasNoChildren) \".\" \"INBOX\"\r\n"
            + t.last("OK List done.\r\n"));
    QCOMPARE(model->rowCount( QModelIndex() ), 5);
    // the first one will be "list of messages"
    QModelIndex idxInbox = model->index(1, 0, QModelIndex());
    QModelIndex idxA = model->index(2, 0, QModelIndex());
    QModelIndex idxB = model->index(3, 0, QModelIndex());
    QModelIndex idxXyz = model->index(4, 0, QModelIndex());
    QCOMPARE(model->data(idxInbox, Qt::DisplayRole), QVariant(QLatin1String("INBOX")));
    QCOMPARE(model->data(idxA, Qt::DisplayRole), QVariant(QLatin1String("a")));
    QCOMPARE(model->data(idxB, Qt::DisplayRole), QVariant(QLatin1String("b")));
    QCOMPARE(model->data(idxXyz, Qt::DisplayRole), QVariant(QLatin1String("xyz")));
    cEmpty();
    QCOMPARE(model->rowCount(idxInbox), 1); // just the "list of messages"
    QCOMPARE(model->rowCount(idxB), 1); // just the "list of messages"
    cEmpty();
    model->rowCount(idxA);
    model->rowCount(idxXyz);
    QByteArray c1 = t.mk("LIST \"\" \"a.%\"\r\n");
    QByteArray r1 = t.last("OK listed\r\n");
    QByteArray c2 = t.mk("LIST \"\" \"xyz.%\"\r\n");
    QByteArray r2 = t.last("OK listed\r\n");
    cClient(c1 + c2);
    cServer("* LIST (\\HasNoChildren) \".\" \"a.aa\"\r\n"
            "* LIST (\\HasNoChildren) \".\" \"a.ab\"\r\n"
            + r1 +
            "* LIST (\\HasNoChildren) \".\" \"xyz.a\"\r\n"
            "* LIST (\\HasNoChildren) \".\" \"xyz.c\"\r\n"
            + r2);
    cEmpty();
    QCOMPARE(model->rowCount(idxA), 3);
    QCOMPARE(model->rowCount(idxXyz), 3);
    cEmpty();
}

void ImapModelListChildMailboxesTest::testFailingList()
{
    QCOMPARE(model->rowCount(QModelIndex()), 1);
    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    QCOMPARE(model->data(QModelIndex(), Imap::Mailbox::RoleIsFetched).toBool(), false);
    QCOMPARE(model->data(QModelIndex(), Imap::Mailbox::RoleIsUnavailable).toBool(), false);
    cServer(t.last("NO no joy today\r\n"));
    QCOMPARE(model->data(QModelIndex(), Imap::Mailbox::RoleIsFetched).toBool(), false);
    QCOMPARE(model->data(QModelIndex(), Imap::Mailbox::RoleIsUnavailable).toBool(), true);
    QCOMPARE(model->rowCount(QModelIndex()), 1);
    cEmpty();
    model->reloadMailboxList();
    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    QCOMPARE(model->data(QModelIndex(), Imap::Mailbox::RoleIsFetched).toBool(), false);
    QCOMPARE(model->data(QModelIndex(), Imap::Mailbox::RoleIsUnavailable).toBool(), false);
    cServer(t.last("OK listed\r\n"));
    QCOMPARE(model->data(QModelIndex(), Imap::Mailbox::RoleIsFetched).toBool(), true);
    QCOMPARE(model->data(QModelIndex(), Imap::Mailbox::RoleIsUnavailable).toBool(), false);
    QCOMPARE(model->rowCount(QModelIndex()), 1);
    cEmpty();
}

void ImapModelListChildMailboxesTest::testFakeListing()
{
    taskFactoryUnsafe->fakeListChildMailboxes = true;
    taskFactoryUnsafe->fakeListChildMailboxesMap[ QLatin1String("") ] = QStringList() << QStringLiteral("a") << QStringLiteral("b");
    taskFactoryUnsafe->fakeListChildMailboxesMap[ QStringLiteral("a") ] = QStringList() << QStringLiteral("aa") << QStringLiteral("ab");
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 3 );
    QModelIndex idxA = model->index( 1, 0, QModelIndex() );
    QModelIndex idxB = model->index( 2, 0, QModelIndex() );
    QCOMPARE( model->data( idxA, Qt::DisplayRole ), QVariant(QLatin1String("a")) );
    QCOMPARE( model->data( idxB, Qt::DisplayRole ), QVariant(QLatin1String("b")) );
    model->rowCount( idxA );
    model->rowCount( idxB );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( idxA ), 3 );
    QCOMPARE( model->rowCount( idxB ), 1 );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
    cleanup(); init();
}

void ImapModelListChildMailboxesTest::testBackslashes()
{
    model->rowCount(QModelIndex());
    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    cServer("* LIST (\\HasNoChildren) \"/\" Drafts\r\n"
            "* LIST (\\HasNoChildren) \"/\" {13}\r\nMail\\Papelera\r\n"
            "* LIST () \"/\" INBOX\r\n"
            + t.last("OK LIST completed\r\n"));
    QCOMPARE(model->rowCount(QModelIndex()), 4);
    QModelIndex withBackSlash = model->index(3, 0, QModelIndex());
    QVERIFY(withBackSlash.isValid());
    QCOMPARE(withBackSlash.data(Imap::Mailbox::RoleMailboxName).toString(), QString::fromUtf8("Mail\\Papelera"));
    QCOMPARE(withBackSlash.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 0);
    cClient(t.mk("STATUS \"Mail\\\\Papelera\" (MESSAGES UNSEEN RECENT)\r\n"));
    cServer("* STATUS {13}\r\nMail\\Papelera (MESSAGES 1 RECENT 1 UNSEEN 1)\r\n" + t.last("OK status done\r\n"));
    QCOMPARE(withBackSlash.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 1);
    cEmpty();
}

/** @short Check that cached mailboxes do not send away STATUS when they are going to be immediately replaced anyway */
void ImapModelListChildMailboxesTest::testNoStatusForCachedItems()
{
    using namespace Imap::Mailbox;

    // Remember two mailboxes
    model->cache()->setChildMailboxes(QString(), QList<MailboxMetadata>()
                                      << MailboxMetadata(QStringLiteral("a"), QStringLiteral("."), QStringList())
                                      << MailboxMetadata(QStringLiteral("b"), QStringLiteral("."), QStringList())
                                      );
    // ... and the numbers for the first of them
    SyncState s;
    s.setExists(10);
    s.setRecent(1);
    s.setUnSeenCount(2);
    QVERIFY(s.isUsableForNumbers());
    model->cache()->setMailboxSyncState(QStringLiteral("b"), s);

    // touch the network
    QCOMPARE(model->rowCount(QModelIndex()), 1);
    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    // ...but the data gets refreshed from cache immediately
    QCOMPARE(model->rowCount(QModelIndex()), 3);

    // just settings up indexes
    idxA = model->index(1, 0, QModelIndex());
    QVERIFY(idxA.isValid());
    QCOMPARE(idxA.data(RoleMailboxName).toString(), QString::fromUtf8("a"));
    idxB = model->index(2, 0, QModelIndex());
    QVERIFY(idxB.isValid());
    QCOMPARE(idxB.data(RoleMailboxName).toString(), QString::fromUtf8("b"));

    // Request message counts; this should not lead to network activity even though the actual number
    // is not stored in the cache for mailbox "a", simply because the whole mailbox will get replaced
    // after the LIST finishes anyway
    QCOMPARE(idxA.data(RoleTotalMessageCount), QVariant());
    QCOMPARE(idxA.data(RoleMailboxNumbersFetched).toBool(), false);
    QCOMPARE(idxB.data(RoleTotalMessageCount).toInt(), 10);
    QCOMPARE(idxB.data(RoleMailboxNumbersFetched).toBool(), false);
    cEmpty();

    cServer("* LIST (\\HasNoChildren) \".\" a\r\n"
            "* LIST (\\HasNoChildren) \".\" b\r\n"
            + t.last("OK listed\r\n"));
    cEmpty();

    // The mailboxes are replaced now -> rebuild the indexes
    QVERIFY(!idxA.isValid());
    QVERIFY(!idxB.isValid());
    idxA = model->index(1, 0, QModelIndex());
    QVERIFY(idxA.isValid());
    QCOMPARE(idxA.data(RoleMailboxName).toString(), QString::fromUtf8("a"));
    idxB = model->index(2, 0, QModelIndex());
    QVERIFY(idxB.isValid());
    QCOMPARE(idxB.data(RoleMailboxName).toString(), QString::fromUtf8("b"));

    // Ask for the numbers again
    QCOMPARE(idxA.data(RoleTotalMessageCount), QVariant());
    QCOMPARE(idxB.data(RoleTotalMessageCount), QVariant());
    QByteArray c1 = t.mk("STATUS a (MESSAGES UNSEEN RECENT)\r\n");
    QByteArray r1 = t.last("OK status\r\n");
    QByteArray c2 = t.mk("STATUS b (MESSAGES UNSEEN RECENT)\r\n");
    QByteArray r2 = t.last("OK status\r\n");
    cClient(c1 + c2);
    cServer("* STATUS a (MESSAGES 1 RECENT 2 UNSEEN 3)\r\n"
            "* STATUS b (MESSAGES 666 RECENT 33 UNSEEN 2)\r\n");
    QCOMPARE(idxA.data(RoleTotalMessageCount).toInt(), 1);
    QCOMPARE(idxA.data(RoleMailboxNumbersFetched).toBool(), true);
    QCOMPARE(idxB.data(RoleTotalMessageCount).toInt(), 666);
    QCOMPARE(idxB.data(RoleMailboxNumbersFetched).toBool(), true);
    QCOMPARE(model->cache()->mailboxSyncState(QLatin1String("a")).unSeenCount(), 3u);
    QCOMPARE(model->cache()->mailboxSyncState(QLatin1String("a")).recent(), 2u);
    // the "EXISTS" is missing, though
    QVERIFY(!model->cache()->mailboxSyncState(QLatin1String("a")).isUsableForNumbers());
    QCOMPARE(model->cache()->mailboxSyncState(QLatin1String("b")).unSeenCount(), 2u);
    QCOMPARE(model->cache()->mailboxSyncState(QLatin1String("b")).recent(), 33u);
    // The mailbox state for mailbox "b" must be preserved
    QCOMPARE(model->cache()->mailboxSyncState(QLatin1String("b")).exists(), 10u);
    QVERIFY(model->cache()->mailboxSyncState(QLatin1String("b")).isUsableForNumbers());
    cServer(r2 + r1);
    cEmpty();
}

/** @short Concurrent listing at several levels of the nesting

https://bugs.kde.org/show_bug.cgi?id=364314
*/
void ImapModelListChildMailboxesTest::testAutoExpanding()
{
    Imap::Mailbox::MailboxModel mm(nullptr, model);
    Imap::Mailbox::MailboxFinder finder(nullptr, &mm);

    auto resetWatchedMailboxes = [&finder]() {
        finder.addMailbox(QStringLiteral("a"));
        finder.addMailbox(QStringLiteral("a.A"));
        finder.addMailbox(QStringLiteral("a.A.1"));
    };

    resetWatchedMailboxes();
    connect(&mm, &QAbstractItemModel::layoutChanged, this, resetWatchedMailboxes);
    connect(&mm, &QAbstractItemModel::rowsRemoved, this, resetWatchedMailboxes);
    connect(&mm, &QAbstractItemModel::modelReset, this, resetWatchedMailboxes);

    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    cServer("* LIST (\\HasChildren) \".\" a\r\n"
            + t.last("OK listed\r\n"));
    cClient(t.mk("LIST \"\" \"a.%\"\r\n"));
    cServer("* LIST (\\HasChildren) \".\" a.A\r\n"
            + t.last("OK listed\r\n"));
    cClient(t.mk("LIST \"\" \"a.A.%\"\r\n"));
    cServer("* LIST (\\HasNoChildren) \".\" a.A.1\r\n"
            + t.last("OK listed\r\n"));

    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_OFFLINE);
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    t.reset();

    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    cServer("* LIST (\\HasChildren) \".\" a\r\n"
            + t.last("OK listed\r\n"));
    auto q1 = t.mk("LIST \"\" \"a.%\"\r\n");
    auto r1 = t.last("OK listed\r\n");
    auto q2 = t.mk("LIST \"\" \"a.A.%\"\r\n");
    auto r2 = t.last("OK listed\r\n");
    cClient(q1 + q2);
    cServer("* LIST (\\HasChildren) \".\" a.A\r\n"
            "* LIST (\\HasNoChildren) \".\" a.A.1\r\n"
            + r1 + r2);

    auto idx_a = mm.index(0, 0, QModelIndex());
    QVERIFY(idx_a.isValid());
    QCOMPARE(idx_a.data(Imap::Mailbox::RoleMailboxName).toString(), QString::fromUtf8("a"));
    QCOMPARE(mm.rowCount(idx_a), 1); // KDE bug 364314, "a.A.1" must not show up beneath "a"
    auto idx_a_A = mm.index(0, 0, idx_a);
    QVERIFY(idx_a_A.isValid());
    QCOMPARE(idx_a_A.data(Imap::Mailbox::RoleMailboxName).toString(), QString::fromUtf8("a.A"));
    QCOMPARE(mm.rowCount(idx_a_A), 1);

    // ...and because the q1's response invalidated q2's root, it gets re-requested
    cClient(t.mk("LIST \"\" \"a.A.%\"\r\n"));
    cServer("* LIST (\\HasNoChildren) \".\" a.A.1\r\n"
            + t.last("OK listed\r\n"));
    QCOMPARE(mm.rowCount(idx_a), 1);
    QCOMPARE(mm.rowCount(idx_a_A), 1);
    cEmpty();
}


QTEST_GUILESS_MAIN( ImapModelListChildMailboxesTest )
