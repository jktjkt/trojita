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
#include "Utils/headless_test.h"
#include "Common/MetaTypes.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"
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

void ImapModelListChildMailboxesTest::testFakeListing()
{
    taskFactoryUnsafe->fakeListChildMailboxes = true;
    taskFactoryUnsafe->fakeListChildMailboxesMap[ QLatin1String("") ] = QStringList() << QLatin1String("a") << QLatin1String("b");
    taskFactoryUnsafe->fakeListChildMailboxesMap[ QLatin1String("a") ] = QStringList() << QLatin1String("aa") << QLatin1String("ab");
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

TROJITA_HEADLESS_TEST( ImapModelListChildMailboxesTest )
