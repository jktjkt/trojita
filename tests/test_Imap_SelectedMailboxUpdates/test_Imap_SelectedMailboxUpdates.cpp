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
#include "test_Imap_SelectedMailboxUpdates.h"
#include "../headless_test.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"

/** @short Test that we survive a new message arrival and its subsequent removal in rapid sequence */
void ImapModelSelectedMailboxUpdatesTest::testExpungeImmediatelyAfterArrival()
{
    existsA = 3;
    uidValidityA = 6;
    uidMapA << 1 << 7 << 9;
    uidNextA = 16;
    helperSyncAWithMessagesEmptyState();
    QVERIFY(SOCK->writtenStuff().isEmpty());

    SOCK->fakeReading(QString::fromAscii("* %1 EXISTS\r\n* %1 EXPUNGE\r\n").arg(QString::number(existsA + 1)).toAscii());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    //QCOMPARE(SOCK->writtenStuff(), QString(t.mk("UID SEARCH UID %1:*\r\n")).arg(QString::number(uidNextA)).toAscii());

    uidMapA << uidNextA;
    ++uidNextA;
    ++existsA;

    //SOCK->fakeReading(QString::fromAscii("* %2 EXPUNGE\r\n").arg(QString::number(existsA)).toAscii());

    QVERIFY(errorSpy->isEmpty());
}

TROJITA_HEADLESS_TEST( ImapModelSelectedMailboxUpdatesTest )
