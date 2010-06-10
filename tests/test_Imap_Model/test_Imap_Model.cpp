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

//#include <QTreeView>

void ImapModelTest::initTestCase()
{
    model = 0;
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
    QTest::qWait( 100 );
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 CAPABILITY\r\ny0 LIST \"\" \"%\"\r\n") );
    SOCK->fakeReading( "* LIST (\\HasNoChildren) \".\" \"INBOX\"\r\n"
                       "* CAPABILITY IMAP4rev1\r\n"
                       "y1 OK capability completed\r\n"
                       "y0 ok list completed\r\n" );
    QTest::qWait( 100 );
    QModelIndex inbox = model->index( 1, 0, QModelIndex() );
    QCOMPARE( model->data( inbox, Qt::DisplayRole ), QVariant("INBOX") );
    QTest::qWait( 100 );

    /*QTreeView* w = new QTreeView();
    w->setModel( model );
    w->show();
    QTest::qWait( 5000 );*/
}

QTEST_MAIN( ImapModelTest )
