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
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache(this);
    factory = new Streams::FakeSocketFactory(Imap::CONN_STATE_AUTHENTICATED);
    Imap::Mailbox::TaskFactoryPtr taskFactory(new Imap::Mailbox::TestingTaskFactory());
    taskFactoryUnsafe = static_cast<Imap::Mailbox::TestingTaskFactory*>(taskFactory.get());
    taskFactoryUnsafe->fakeOpenConnectionTask = true;
    model = new Imap::Mailbox::Model(this, cache, Imap::Mailbox::SocketFactoryPtr(factory), std::move(taskFactory));
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    QCoreApplication::processEvents();
    task = 0;
}

void ImapModelListChildMailboxesTest::cleanup()
{
    delete model;
    model = 0;
    taskFactoryUnsafe = 0;
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
}

void ImapModelListChildMailboxesTest::initTestCase()
{
    Common::registerMetaTypes();
    model = 0;
    task = 0;
}

#define SOCK static_cast<Streams::FakeSocket*>( factory->lastSocket() )

void ImapModelListChildMailboxesTest::testSimpleListing()
{
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 LIST \"\" \"%\"\r\n") );
    SOCK->fakeReading( "* LIST (\\HasNoChildren) \".\" \"b\"\r\n"
                       "* LIST (\\HasChildren) \".\" \"a\"\r\n"
                       "* LIST (\\Noselect \\HasChildren) \".\" \"xyz\"\r\n"
                       "* LIST (\\HasNoChildren) \".\" \"INBOX\"\r\n"
                       "y0 OK List done.\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 5 );
    // the first one will be "list of messages"
    QModelIndex idxInbox = model->index( 1, 0, QModelIndex() );
    QModelIndex idxA = model->index( 2, 0, QModelIndex() );
    QModelIndex idxB = model->index( 3, 0, QModelIndex() );
    QModelIndex idxXyz = model->index( 4, 0, QModelIndex() );
    QCOMPARE( model->data( idxInbox, Qt::DisplayRole ), QVariant(QLatin1String("INBOX")) );
    QCOMPARE( model->data( idxA, Qt::DisplayRole ), QVariant(QLatin1String("a")) );
    QCOMPARE( model->data( idxB, Qt::DisplayRole ), QVariant(QLatin1String("b")) );
    QCOMPARE( model->data( idxXyz, Qt::DisplayRole ), QVariant(QLatin1String("xyz")) );
    QCoreApplication::processEvents();
    QVERIFY( SOCK->writtenStuff().isEmpty() );
    QCOMPARE( model->rowCount( idxInbox ), 1 ); // just the "list of messages"
    QCOMPARE( model->rowCount( idxB ), 1 ); // just the "list of messages"
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray() );
    model->rowCount( idxA );
    model->rowCount( idxXyz );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 LIST \"\" \"a.%\"\r\n" "y2 LIST \"\" \"xyz.%\"\r\n") );
    SOCK->fakeReading( "* LIST (\\HasNoChildren) \".\" \"a.aa\"\r\n"
                       "* LIST (\\HasNoChildren) \".\" \"a.ab\"\r\n"
                       "y1 OK List completed.\r\n"
                       "* LIST (\\HasNoChildren) \".\" \"xyz.a\"\r\n"
                       "* LIST (\\HasNoChildren) \".\" \"xyz.c\"\r\n"
                       "y2 OK List completed.\r\n" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( idxA ), 3 );
    QCOMPARE( model->rowCount( idxXyz ), 3 );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
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
