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

    // Now add one more message, the D
    helperGenericTrafficArrive2(askForEnvelopes);

    // Remove B
    helperDeleteOneMessage(1, QStringList() << QLatin1String("A") << QLatin1String("C") << QLatin1String("D"));

    // Remove D
    helperDeleteOneMessage(2, QStringList() << QLatin1String("A") << QLatin1String("C"));

    // Remove A
    helperDeleteOneMessage(0, QStringList() << QLatin1String("C"));

    // Remove C
    helperDeleteOneMessage(0, QStringList());

    // Add completely different messages, A, B, C and D
    helperGenericTrafficArrive3(askForEnvelopes);

    // remove C and B
    helperDeleteOneMessage(2, QStringList() << QLatin1String("A") << QLatin1String("B") << QLatin1String("D"));
    helperDeleteOneMessage(1, QStringList() << QLatin1String("A") << QLatin1String("D"));

    // Add E, F and G
    helperGenericTrafficArrive4(askForEnvelopes);

    // Delete D and F
    helperDeleteTwoMessages(3, 1, QStringList() << QLatin1String("A") << QLatin1String("E") << QLatin1String("G"));

    // Delete G and A
    helperDeleteTwoMessages(2, 0, QStringList() << QLatin1String("E"));
}

/** @short Test an arrival of three brand new messages to an already synced mailbox

This function assumes that mailbox A is already openened and is active and synced. It will fake an arrival
of three brand new messages, A, B and C. This arrival will get noticed and processed.
 */
void ImapModelSelectedMailboxUpdatesTest::helperGenericTrafficFirstArrivals(bool askForEnvelopes)
{
    // Fake delivery of A, B and C
    SOCK->fakeReading(QByteArray("* 3 EXISTS\r\n* 3 RECENT\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // This should trigger a request for flags
    QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 12:* (FLAGS)\r\n"));
    // The messages sgould be there already
    QVERIFY(msgListA.child(0,0).isValid());
    QModelIndex msgB = msgListA.child(1, 0);
    QVERIFY(msgB.isValid());
    QVERIFY(msgListA.child(2,0).isValid());
    QVERIFY( ! msgListA.child(3,0).isValid());
    // We shouldn't have the UID yet
    QCOMPARE(msgB.data(Imap::Mailbox::RoleMessageUid).toUInt(), 0u);

    // Verify various message counts
    // This one is parsed from RECENT
    QCOMPARE(idxA.data(Imap::Mailbox::RoleRecentMessageCount).toInt(), 3);
    // This one is easy, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 3);
    // We have to wait for FLAGS for this one
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 0);

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
    // The UIDs shall be known at this point
    QCOMPARE(msgB.data(Imap::Mailbox::RoleMessageUid).toUInt(), 44u);
    // The unread message count should be correct now, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 3);
    QCoreApplication::processEvents();
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

    // Yes, we're counting to one more than what actually is here; that's because we want to prevent a possible out-of-bounds access
    for ( int i = 0; i < static_cast<int>(existsA) + 1; ++i )
        msgListA.child(i,0).data(Imap::Mailbox::RoleMessageSubject);
    QCoreApplication::processEvents();
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

/** @short Fake delivery of D to a mailbox which already contains A, B and C */
void  ImapModelSelectedMailboxUpdatesTest::helperGenericTrafficArrive2(bool askForEnvelopes)
{
    // Fake delivery of D
    SOCK->fakeReading(QByteArray("* 4 EXISTS\r\n* 4 RECENT\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // This should trigger a request for flags
    QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 46:* (FLAGS)\r\n"));
    // The messages sgould be there already
    QVERIFY(msgListA.child(0,0).isValid());
    QModelIndex msgD = msgListA.child(3, 0);
    QVERIFY(msgD.isValid());
    QVERIFY( ! msgListA.child(4,0).isValid());
    // We shouldn't have the UID yet
    QCOMPARE(msgD.data(Imap::Mailbox::RoleMessageUid).toUInt(), 0u);

    // Verify various message counts
    // This one is parsed from RECENT
    QCOMPARE(idxA.data(Imap::Mailbox::RoleRecentMessageCount).toInt(), 4);
    // This one is easy, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 4);
    // this one isn't available yet
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 3);

    if ( askForEnvelopes ) {
        // In the meanwhile, ask for ENVELOPE etc -- but it shouldn't be there yet
        QVERIFY( ! msgD.data(Imap::Mailbox::RoleMessageFrom).isValid() );
    }

    // FLAGS arrive, bringing the UID with them
    SOCK->fakeReading( QByteArray("* 4 FETCH (UID 46 FLAGS (\\Recent))\r\n") +
                       t.last("OK fetched\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // The UIDs shall be known at this point
    QCOMPARE(msgD.data(Imap::Mailbox::RoleMessageUid).toUInt(), 46u);
    // The unread message count should be correct now, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 4);
    QCoreApplication::processEvents();

    if ( askForEnvelopes ) {
        QCoreApplication::processEvents();
        // The ENVELOPE and related fields should be requested now
        QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 46 (ENVELOPE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }

    existsA = 4;
    uidNextA = 47;
    uidMapA << 46;
    helperCheckCache();
    helperVerifyUidMapA();

    // Request the subject once again
    msgListA.child(3,0).data(Imap::Mailbox::RoleMessageSubject);
    // In contrast to helperGenericTrafficFirstArrivals, we won't query "message #5",ie. one more than how many are
    // actually there. The main motivation is trying to behave in a different manner than before. Hope this helps.
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    if ( askForEnvelopes ) {
        // Envelope for D already got requested -> nothing to do here
    } else {
        // The D got requested by an explicit query above
        QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 46 (ENVELOPE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }
    SOCK->fakeReading( helperCreateTrivialEnvelope(4, 46, QLatin1String("D")) + t.last("OK fetched\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    helperCheckSubjects(QStringList() << QLatin1String("A") << QLatin1String("B") << QLatin1String("C") << QLatin1String("D"));

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QVERIFY( errorSpy->isEmpty() );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
}

/** @short Helper for creating a fake FETCH response with all usually fetched fields

This function will prepare a response mentioning a minimal set of ENVELOPE, UID, BODYSTRUCTURE etc. Please note that
the actual string won't be passed to the fake socket, but rather returned; this is needed because the fake socket can't accept
incremental data, but we have to feed it with stuff at once.
*/
QByteArray ImapModelSelectedMailboxUpdatesTest::helperCreateTrivialEnvelope(const uint seq, const uint uid, const QString &subject)
{
    return QString::fromAscii("* %1 FETCH (UID %2 RFC822.SIZE 89 ENVELOPE (NIL \"%3\" NIL NIL NIL NIL NIL NIL NIL NIL) "
                              "BODYSTRUCTURE (\"text\" \"plain\" () NIL NIL NIL 19 2 NIL NIL NIL NIL))\r\n").arg(
                                      QString::number(seq), QString::number(uid), subject ).toAscii();
}

/** @short Check subjects of all messages in a given mailbox

This function will check subjects of all mailboxes in the mailbox A against a list of subjects specified in @arg subjects.
*/
void ImapModelSelectedMailboxUpdatesTest::helperCheckSubjects(const QStringList &subjects)
{
    for ( int i = 0; i < subjects.size(); ++i ) {
        QModelIndex index = msgListA.child(i,0);
        QVERIFY(index.isValid());
        QCOMPARE(index.data(Imap::Mailbox::RoleMessageSubject).toString(), subjects[i]);
    }
    // Me sure thet there are no more messages
    QVERIFY( ! msgListA.child(subjects.size(),0).isValid() );
}

/** @short Test what happens when the server tells us that one message got deleted */
void ImapModelSelectedMailboxUpdatesTest::helperDeleteOneMessage(const uint seq, const QStringList &remainingSubjects)
{
    // Fake deleting one message
    --existsA;
    uidMapA.removeAt(seq);
    Q_ASSERT(remainingSubjects.size() == static_cast<int>(existsA));
    Q_ASSERT(uidMapA.size() == static_cast<int>(existsA));
    SOCK->fakeReading(QString::fromAscii("* %1 EXPUNGE\r\n* %2 RECENT\r\n").arg(QString::number(seq+1), QString::number(existsA)).toAscii());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Verify the model's idea about the current state
    helperCheckSubjects(remainingSubjects);
    helperCheckCache();
    helperVerifyUidMapA();
}

/** @short Test what happens when the server tells us that one message got deleted */
void ImapModelSelectedMailboxUpdatesTest::helperDeleteTwoMessages(const uint seq1, const uint seq2, const QStringList &remainingSubjects)
{
    // Fake deleting one message
    existsA -= 2;
    uidMapA.removeAt(seq1);
    uidMapA.removeAt(seq2);
    Q_ASSERT(remainingSubjects.size() == static_cast<int>(existsA));
    Q_ASSERT(uidMapA.size() == static_cast<int>(existsA));
    SOCK->fakeReading(QString::fromAscii("* %1 EXPUNGE\r\n* %2 EXPUNGE\r\n* %3 RECENT\r\n").arg(QString::number(seq1+1), QString::number(seq2+1), QString::number(existsA)).toAscii());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Verify the model's idea about the current state
    helperCheckSubjects(remainingSubjects);
    helperCheckCache();
    helperVerifyUidMapA();
}

/** @short Fake delivery of a completely new set of messages, the A, B, C and D */
void ImapModelSelectedMailboxUpdatesTest::helperGenericTrafficArrive3(bool askForEnvelopes)
{
    // Fake the announcement of the delivery
    SOCK->fakeReading(QByteArray("* 4 EXISTS\r\n* 4 RECENT\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // This should trigger a request for flags
    QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 47:* (FLAGS)\r\n"));

    // The messages should be there already
    QVERIFY(msgListA.child(0,0).isValid());
    QModelIndex msgD = msgListA.child(3, 0);
    QVERIFY(msgD.isValid());
    QVERIFY( ! msgListA.child(4,0).isValid());
    // We shouldn't have the UID yet
    QCOMPARE(msgD.data(Imap::Mailbox::RoleMessageUid).toUInt(), 0u);

    // Verify various message counts
    // This one is parsed from RECENT
    QCOMPARE(idxA.data(Imap::Mailbox::RoleRecentMessageCount).toInt(), 4);
    // This one is easy, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 4);
    // this one isn't available yet
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 0);

    if ( askForEnvelopes ) {
        // In the meanwhile, ask for ENVELOPE etc -- but it shouldn't be there yet
        QVERIFY( ! msgD.data(Imap::Mailbox::RoleMessageFrom).isValid() );
    }

    // FLAGS arrive, bringing the UID with them
    SOCK->fakeReading( QByteArray("* 1 FETCH (UID 47 FLAGS (\\Recent))\r\n"
                                  "* 2 FETCH (UID 48 FLAGS (\\Recent))\r\n") );
    // Try to insert a pause here; this could be used to properly play with preloading in future...
    for ( int i = 0; i < 10; ++i)
        QCoreApplication::processEvents();
    SOCK->fakeReading( QByteArray("* 3 FETCH (UID 49 FLAGS (\\Recent))\r\n"
                                  "* 4 FETCH (UID 50 FLAGS (\\Recent))\r\n") +
                       t.last("OK fetched\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // The UIDs shall be known at this point
    QCOMPARE(msgD.data(Imap::Mailbox::RoleMessageUid).toUInt(), 50u);
    // The unread message count should be correct now, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 4);
    QCoreApplication::processEvents();

    if ( askForEnvelopes ) {
        QCoreApplication::processEvents();
        // The ENVELOPE and related fields should be requested now
        QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 47:50 (ENVELOPE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }

    existsA = 4;
    uidNextA = 51;
    uidMapA.clear();
    uidMapA << 47 << 48 << 49 << 50;
    helperCheckCache();
    helperVerifyUidMapA();

    if ( askForEnvelopes ) {
        // Envelopes already requested -> nothing to do here
    } else {
        // Not requested yet -> do it now
        QVERIFY( ! msgD.data(Imap::Mailbox::RoleMessageFrom).isValid() );
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 47:50 (ENVELOPE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }
    // This is common for both cases; the data should finally arrive
    SOCK->fakeReading( helperCreateTrivialEnvelope(1, 47, QLatin1String("A")) +
                       helperCreateTrivialEnvelope(2, 48, QLatin1String("B")) +
                       helperCreateTrivialEnvelope(3, 49, QLatin1String("C")) +
                       helperCreateTrivialEnvelope(4, 50, QLatin1String("D")) +
                       t.last("OK fetched\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    helperCheckSubjects(QStringList() << QLatin1String("A") << QLatin1String("B") << QLatin1String("C") << QLatin1String("D"));

    // Verify UIDd and cache stuff once again to make sure reading data doesn't mess anything up
    helperCheckCache();
    helperVerifyUidMapA();

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QVERIFY( errorSpy->isEmpty() );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
}

/** @short Fake delivery of three new messages, E, F and G, to a mailbox with A and D */
void ImapModelSelectedMailboxUpdatesTest::helperGenericTrafficArrive4(bool askForEnvelopes)
{
    // Fake the announcement of the delivery
    SOCK->fakeReading(QByteArray("* 5 EXISTS\r\n* 5 RECENT\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // This should trigger a request for flags
    QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 51:* (FLAGS)\r\n"));

    QModelIndex msgE = msgListA.child(2, 0);
    QVERIFY(msgE.isValid());
    // We shouldn't have the UID yet
    QCOMPARE(msgE.data(Imap::Mailbox::RoleMessageUid).toUInt(), 0u);

    // Verify various message counts
    // This one is parsed from RECENT
    QCOMPARE(idxA.data(Imap::Mailbox::RoleRecentMessageCount).toInt(), 5);
    // This one is easy, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 5);
    // this one isn't available yet
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 2);

    if ( askForEnvelopes ) {
        // In the meanwhile, ask for ENVELOPE etc -- but it shouldn't be there yet
        QVERIFY( ! msgE.data(Imap::Mailbox::RoleMessageFrom).isValid() );
    }

    // FLAGS arrive, bringing the UID with them
    SOCK->fakeReading( QByteArray("* 3 FETCH (UID 52 FLAGS (\\Recent))\r\n"
                                  "* 4 FETCH (UID 53 FLAGS (\\Recent))\r\n"
                                  "* 5 FETCH (UID 63 FLAGS (\\Recent))\r\n") +
                       t.last("OK fetched\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // The UIDs shall be known at this point
    QCOMPARE(msgE.data(Imap::Mailbox::RoleMessageUid).toUInt(), 52u);
    // The unread message count should be correct now, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 5);
    QCoreApplication::processEvents();

    if ( askForEnvelopes ) {
        QCoreApplication::processEvents();
        // The ENVELOPE and related fields should be requested now
        QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 52 (ENVELOPE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }

    existsA = 5;
    uidNextA = 64;
    uidMapA << 52 << 53 << 63;
    helperCheckCache();
    helperVerifyUidMapA();

    if ( askForEnvelopes ) {
        // Envelopes already requested -> nothing to do here
    } else {
        // Not requested yet -> do it now
        QVERIFY( ! msgE.data(Imap::Mailbox::RoleMessageFrom).isValid() );
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QCOMPARE(SOCK->writtenStuff(), t.mk("UID FETCH 52:53,63 (ENVELOPE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }
    // This is common for both cases; the data should finally arrive
    SOCK->fakeReading( helperCreateTrivialEnvelope(3, 52, QLatin1String("E")) +
                       helperCreateTrivialEnvelope(4, 53, QLatin1String("F")) +
                       helperCreateTrivialEnvelope(5, 63, QLatin1String("G")) +
                       t.last("OK fetched\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    helperCheckSubjects(QStringList() << QLatin1String("A") << QLatin1String("D") << QLatin1String("E") << QLatin1String("F") << QLatin1String("G"));

    // Verify UIDd and cache stuff once again to make sure reading data doesn't mess anything up
    helperCheckCache();
    helperVerifyUidMapA();

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QVERIFY( errorSpy->isEmpty() );
    QVERIFY( SOCK->writtenStuff().isEmpty() );
}


TROJITA_HEADLESS_TEST( ImapModelSelectedMailboxUpdatesTest )
