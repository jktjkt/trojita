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
#include "test_Imap_DisappearingMailboxes.h"
#include "../headless_test.h"
#include "Streams/FakeSocket.h"

/** @short This test verifies that we don't segfault during offline -> online transition */
void ImapModelDisappearingMailboxTest::testGoingOfflineOnline()
{
    helperSyncBNoMessages();
    model->setNetworkOffline();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), t.mk("LOGOUT\r\n"));
    model->setNetworkOnline();

    // We can't call helperSyncBNoMessages() here, it relies on msgListB's validity,
    // but that index is not neccessary valid because of our kludgy fake listing...

    QCOMPARE( model->rowCount( msgListB ), 0 );
    model->switchToMailbox( idxB );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), t.mk("SELECT b\r\n") );
    SOCK->fakeReading( QByteArray("* 0 exists\r\n")
                                  + t.last("ok completed\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

/** @short Simulate what happens when user goes offline with views attached

This is intended to be very similar to how real application behaves, reacting to events etc.
 */
void ImapModelDisappearingMailboxTest::testGoingReallyOfflineOnline()
{
    helperSyncBNoMessages();

    // Make sure the socket is present
    QPointer<Imap::Socket> socketPtr(factory->lastSocket());
    Q_ASSERT(!socketPtr.isNull());

    model->setNetworkOffline();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QCOMPARE(SOCK->writtenStuff(), t.mk("LOGOUT\r\n"));
    SOCK->fakeReading(QByteArray("* BYE see ya\r\n")
                      + t.last("ok logged out\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // It should be gone by now
    QVERIFY(socketPtr.isNull());

    // Try a reconnect
    t.reset();
    model->setNetworkOnline();
    helperInitialListing();
    helperSyncBNoMessages();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
}

TROJITA_HEADLESS_TEST( ImapModelDisappearingMailboxTest )
