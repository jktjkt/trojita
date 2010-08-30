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
#include "test_Imap_Model_OpenConnectionTask.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/MailboxModel.h"
#include "Imap/Model/OpenConnectionTask.h"

void ImapModelOpenConnectionTaskTest::initTestCase()
{
    model = 0;
    spy = 0;
}

void ImapModelOpenConnectionTaskTest::init()
{
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache( this, QString() );
    factory = new Imap::Mailbox::FakeSocketFactory();
    model = new Imap::Mailbox::Model( this, cache, Imap::Mailbox::SocketFactoryPtr( factory ), false );
    task = new Imap::Mailbox::OpenConnectionTask( model );
    spy = new QSignalSpy( task, SIGNAL(completed()) );
}

void ImapModelOpenConnectionTaskTest::cleanup()
{
    delete model;
    model = 0;
    task = 0;
    if ( spy ) {
        spy->deleteLater();
        spy = 0;
    }
}

#define SOCK static_cast<Imap::FakeSocket*>( factory->lastSocket() )

void ImapModelOpenConnectionTaskTest::testPreauth()
{
    SOCK->fakeReading( "* PREAUTH foo\r\n" );
    QVERIFY( spy->isEmpty() );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 CAPABILITY\r\n") );
    QVERIFY( spy->isEmpty() );
    SOCK->fakeReading( "* CAPABILITY IMAP4rev1\r\ny0 OK capability completed\r\n" );
    QCoreApplication::processEvents();
    QCOMPARE( spy->size(), 1 );
}

void ImapModelOpenConnectionTaskTest::testPreauthWithCapability()
{
    SOCK->fakeReading( "* PREAUTH [CAPABILITY IMAP4rev1] foo\r\n" );
    QVERIFY( spy->isEmpty() );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray() );
    QCOMPARE( spy->size(), 1 );
}

QTEST_MAIN( ImapModelOpenConnectionTaskTest )
