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

/** @short Test that we survive a new message arrival and its subsequent removal in rapid sequence

The code won't notice that the message got expunged immediately (simply because it has no idea of a
next response when processing the first EXISTS), will ask for UID and FLAGS, but the message gets
deleted (and EXPUNGE sent) before the server receives the UID FETCH command. This leads to us having
a flawed idea of UIDNEXT, unless the server provides a hint via the * OK [UIDNEXT xyz] response.

It's a question whether or not to at least increment the UIDNEXT by one when the response doesn't
arrive. Doing that would probably break when the server employs an Outlook-workaround for IDLE with
a fake EXISTS/EXPUNGE responses.
*/
void ImapModelSelectedMailboxUpdatesTest::helperTestExpungeImmediatelyAfterArrival(bool sendUidNext)
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
    QCOMPARE(SOCK->writtenStuff(), QString(t.mk("UID FETCH %1:* (FLAGS)\r\n")).arg(QString::number( uidMapA.last() + 1 )).toAscii());

    // Add message with this UID to our internal list
    uint addedUid = 33;
    uidNextA = addedUid + 1;

    QByteArray uidUpdateResponse = sendUidNext ? QString("* OK [UIDNEXT %1] courtesy of the server\r\n").arg(
            QString::number(uidNextA)).toAscii() : QByteArray();

    // ...but because it got deleted, here we go
    SOCK->fakeReading(t.last("OK empty fetch\r\n") + uidUpdateResponse);

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QVERIFY(errorSpy->isEmpty());

    helperCheckCache( ! sendUidNext );
    helperVerifyUidMapA();
}

void ImapModelSelectedMailboxUpdatesTest::testUnsolicitedFetch()
{
    existsA = 2;
    uidValidityA = 666;
    uidMapA << 3 << 9;
    uidNextA = 33;
    helperSyncAWithMessagesEmptyState();
    QVERIFY(SOCK->writtenStuff().isEmpty());

    SOCK->fakeReading(QString::fromAscii("* %1 EXISTS\r\n* %1 FETCH (FLAGS (\\Seen \\Recent $NotJunk NotJunk))\r\n").arg(QString::number(existsA + 1)).toAscii());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(SOCK->writtenStuff(), QString(t.mk("UID FETCH %1:* (FLAGS)\r\n")).arg(QString::number( uidMapA.last() + 1 )).toAscii());

    // Add message with this UID to our internal list
    uint addedUid = 42;
    ++existsA;
    uidMapA << addedUid;
    uidNextA = addedUid + 1;

    SOCK->fakeReading( QString("* %1 FETCH (FLAGS (\\Seen \\Recent $NotJunk NotJunk) UID %2)\r\n").arg(
            QString::number(existsA), QString::number(addedUid)).toAscii() +
                       t.last("OK flags returned\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QVERIFY(errorSpy->isEmpty());

    helperCheckCache();
    helperVerifyUidMapA();
}

/** @short Test a rapid EXISTS/EXPUNGE sequence

@see helperTestExpungeImmediatelyAfterArrival for details
*/
void ImapModelSelectedMailboxUpdatesTest::testExpungeImmediatelyAfterArrival()
{
    helperTestExpungeImmediatelyAfterArrival(false);
}

/** @short Test a rapid EXISTS/EXPUNGE sequence with an added UIDNEXT response

@see helperTestExpungeImmediatelyAfterArrival for details
*/
void ImapModelSelectedMailboxUpdatesTest::testExpungeImmediatelyAfterArrivalWithUidNext()
{
    helperTestExpungeImmediatelyAfterArrival(true);
}

/** @short Test generic traffic to an opened mailbox without asking for message data too soon */
void ImapModelSelectedMailboxUpdatesTest::testGenericTraffic()
{
    helperGenericTraffic(false);
}

/** @short Test generic traffic to an opened mailbox when we ask for message metadata as soon as they arrive */
void ImapModelSelectedMailboxUpdatesTest::testGenericTrafficWithEnvelopes()
{
    helperGenericTraffic(true);
}

void ImapModelSelectedMailboxUpdatesTest::helperGenericTraffic(bool askForEnvelopes)
{
    // At first, sync to an empty state
    uidNextA = 12;
    uidValidityA = 333;
    helperSyncANoMessagesCompleteState();

    // Fake delivery of A, B and C
    helperGenericTrafficFirstArrivals(askForEnvelopes);
}

void ImapModelSelectedMailboxUpdatesTest::helperGenericTrafficFirstArrivals(bool askForEnvelopes)
{
    // Fake delivery of A, B and C
    SOCK->fakeReading(QByteArray("* 3 EXISTS\r\n* 3 RECENT\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // This should trigger a request for flags
    QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 1:* (FLAGS)\r\n"));
    // The messages sgould be there already
    QVERIFY(msgListA.child(0,0).isValid());
    QModelIndex msgB = msgListA.child(1, 0);
    QVERIFY(msgB.isValid());
    QVERIFY(msgListA.child(2,0).isValid());
    QVERIFY( ! msgListA.child(3,0).isValid());
    // We shouldn't have the UID yet
    QCOMPARE(msgB.data(Imap::Mailbox::RoleMessageUid).toUInt(), 0u);

    if ( askForEnvelopes ) {
        // In the meanwhile, ask for ENVELOPE etc -- but it shouldn't be there yet
        QVERIFY( ! msgB.data(Imap::Mailbox::RoleMessageFrom).isValid() );
    }

    // FLAGS arrive, bringing the UID with them
    SOCK->fakeReading( QByteArray("* 1 FETCH (UID 43 FLAGS (\\Recent))\r\n"
                                  "* 2 FETCH (UID 44 FLAGS (\\Recent))\r\n"
                                  "* 3 FETCH (UID 45 FLAGS (\\Recent))\r\n") +
                       t.last("OK fetched\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // The UIDs shall be known at thsi point
    QCOMPARE(msgB.data(Imap::Mailbox::RoleMessageUid).toUInt(), 44u);
    QCoreApplication::processEvents();

    if ( askForEnvelopes ) {
        // The ENVELOPE and related fields should be requested now
        QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 43:44 (ENVELOPE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }

    existsA = 3;
    uidNextA = 46;
    uidMapA << 43 << 44 << 45;
    helperCheckCache();
    helperVerifyUidMapA();

    for ( int i = 0; i < 3; ++i )
        msgListA.child(i,0).data(Imap::Mailbox::RoleMessageSubject);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    if ( askForEnvelopes ) {
        // Envelopes for A and B already got requested
        // We have to preserve the previous command, too
        QByteArray completionForFirstFetch = t.last("OK fetched\r\n");
        QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 45 (ENVELOPE BODYSTRUCTURE RFC822.SIZE)\r\n"));
        // Simulate out-of-order execution here -- the completion responses for both commands arrive only
        // after the actual data for both of them got pushed
        SOCK->fakeReading( helperCreateTrivialEnvelope(1, 43, QLatin1String("A")) +
                           helperCreateTrivialEnvelope(2, 44, QLatin1String("B")) +
                           helperCreateTrivialEnvelope(3, 45, QLatin1String("C")) +
                           completionForFirstFetch +
                           t.last("OK fetched\r\n"));
    } else {
        // Requesting all envelopes at once
        QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 43:45 (ENVELOPE BODYSTRUCTURE RFC822.SIZE)\r\n"));
        SOCK->fakeReading( helperCreateTrivialEnvelope(1, 43, QLatin1String("A")) +
                           helperCreateTrivialEnvelope(2, 44, QLatin1String("B")) +
                           helperCreateTrivialEnvelope(3, 45, QLatin1String("C")) +
                           t.last("OK fetched\r\n"));
    }
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    helperCheckSubjects(QStringList() << QLatin1String("A") << QLatin1String("B") << QLatin1String("C"));

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QVERIFY( errorSpy->isEmpty() );
    QVERIFY( SOCK->writtenStuff().isEmpty() );

}

QByteArray ImapModelSelectedMailboxUpdatesTest::helperCreateTrivialEnvelope(const uint seq, const uint uid, const QString &subject)
{
    return QString::fromAscii("* %1 FETCH (UID %2 RFC822.SIZE 89 ENVELOPE (NIL \"%3\" NIL NIL NIL NIL NIL NIL NIL NIL) "
                              "BODYSTRUCTURE (\"text\" \"plain\" () NIL NIL NIL 19 2 NIL NIL NIL NIL))\r\n").arg(
                                      QString::number(seq), QString::number(uid), subject ).toAscii();
}

void ImapModelSelectedMailboxUpdatesTest::helperCheckSubjects(const QStringList &subjects)
{
    for ( int i = 0; i < subjects.size(); ++i ) {
        QModelIndex index = msgListA.child(i,0);
        QVERIFY(index.isValid());
        QCOMPARE(index.data(Imap::Mailbox::RoleMessageSubject).toString(), subjects[i]);
    }
}

TROJITA_HEADLESS_TEST( ImapModelSelectedMailboxUpdatesTest )
