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
#include "test_Imap_Tasks_ObtainSynchronizedMailbox.h"
#include "../headless_test.h"
#include "test_LibMailboxSync/FakeCapabilitiesInjector.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Tasks/ObtainSynchronizedMailboxTask.h"

namespace Imap {
namespace Mailbox {
/** @short Operator for QCOMPARE which acts on all data stored in the SyncState

This operator compares *everything*, including the hidden members.
*/
bool operator==(const SyncState &a, const SyncState &b)
{
    return a.completelyEqualTo(b);
}

}
}

namespace QTest {

/** @short Debug data dumper for unit tests

Could be a bit confusing as it doesn't print out the hidden members. Therefore a simple x.setFlags(QStringList()) -- which merely
sets a value same as the default one -- will result in comparison failure, but this function wouldn't print the original cause.
*/
template<>
char *toString(const Imap::Mailbox::SyncState &syncState)
{
    QString buf;
    QDebug d(&buf);
    d << syncState;
    return qstrdup(buf.toAscii().constData());
}

/** @short Debug data dumper for the MessageDataBundle */
template<>
char *toString(const Imap::Mailbox::AbstractCache::MessageDataBundle &bundle)
{
    QString buf;
    QDebug d(&buf);
    d << "UID:" << bundle.uid << "Envelope:" << bundle.envelope << "size:" << bundle.size <<
         "bodystruct:" << bundle.serializedBodyStructure;
    return qstrdup(buf.toAscii().constData());
}

/** @short Debug data dumper for QList<uint> */
template<>
char *toString(const QList<uint> &list)
{
    QString buf;
    QDebug d(&buf);
    d << "QList<uint> (" << list.size() << "items):";
    Q_FOREACH(const uint item, list) {
        d << item;
    }
    return qstrdup(buf.toAscii().constData());
}


}

/** Verify syncing of an empty mailbox with just the EXISTS response

Verify that we can synchronize a mailbox which is empty even if the server
ommits most of the required replies and sends us just the EXISTS one. Also
check the cache for valid state.
*/
void ImapModelObtainSynchronizedMailboxTest::testSyncEmptyMinimal()
{
    model->setProperty( "trojita-imap-noop-period", 10 );

    // Ask the model to sync stuff
    QCOMPARE( model->rowCount( msgListA ), 0 );
    cClient(t.mk("SELECT a\r\n"));

    // Try to feed it with absolute minimum data
    cServer(QByteArray("* 0 exists\r\n") + t.last("OK done\r\n"));

    // Verify that we indeed received what we wanted
    QCOMPARE( model->rowCount( msgListA ), 0 );
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->fetched() );
    //QTest::qWait( 100 );
    QCoreApplication::processEvents();
    cEmpty();

    // Now, let's try to re-sync it once again; the difference is that our cache now has "something"
    model->resyncMailbox( idxA );
    QCoreApplication::processEvents();

    // Verify that it indeed caused a re-synchronization
    list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->loading() );
    cClient(t.mk("SELECT a\r\n"));
    cServer(QByteArray("* 0 exists\r\n") + t.last("OK done\r\n"));
    cEmpty();

    // Check the cache
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QString::fromAscii("a") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.flags(), QStringList() );
    QCOMPARE( syncState.isUsableForNumbers(), false );
    QCOMPARE( syncState.isUsableForSyncing(), false );
    QCOMPARE( syncState.permanentFlags(), QStringList() );
    QCOMPARE( syncState.recent(), 0u );
    QCOMPARE( syncState.uidNext(), 0u );
    QCOMPARE( syncState.uidValidity(), 0u );
    QCOMPARE( syncState.unSeenCount(), 0u );
    QCOMPARE( syncState.unSeenOffset(), 0u );

    // No errors
    QVERIFY( errorSpy->isEmpty() );
}

/** @short Verify synchronization of an empty mailbox against a compliant IMAP4rev1 server

This test verifies that we handle a compliant server's responses when we sync an empty mailbox.
A check of the state of the cache after is completed, too.
*/
void ImapModelObtainSynchronizedMailboxTest::testSyncEmptyNormal()
{
    // Ask the model to sync stuff
    QCOMPARE( model->rowCount( msgListA ), 0 );
    cClient(t.mk("SELECT a\r\n"));

    // Try to feed it with absolute minimum data
    cServer(QByteArray("* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n"
                       "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft \\*)] Flags permitted.\r\n"
                       "* 0 EXISTS\r\n"
                       "* 0 RECENT\r\n"
                       "* OK [UIDVALIDITY 666] UIDs valid\r\n"
                       "* OK [UIDNEXT 3] Predicted next UID\r\n")
            + t.last("OK [READ-WRITE] Select completed.\r\n"));

    // Verify that we indeed received what we wanted
    QCOMPARE( model->rowCount( msgListA ), 0 );
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->fetched() );
    cEmpty();

    // Check the cache
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QString::fromAscii("a") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.flags(), QStringList() << QString::fromAscii("\\Answered") <<
              QString::fromAscii("\\Flagged") << QString::fromAscii("\\Deleted") <<
              QString::fromAscii("\\Seen") << QString::fromAscii("\\Draft") );
    QCOMPARE( syncState.isUsableForNumbers(), false );
    QCOMPARE( syncState.isUsableForSyncing(), true );
    QCOMPARE( syncState.permanentFlags(), QStringList() << QString::fromAscii("\\Answered") <<
              QString::fromAscii("\\Flagged") << QString::fromAscii("\\Deleted") <<
              QString::fromAscii("\\Seen") << QString::fromAscii("\\Draft") << QString::fromAscii("\\*") );
    QCOMPARE( syncState.recent(), 0u );
    QCOMPARE( syncState.uidNext(), 3u );
    QCOMPARE( syncState.uidValidity(), 666u );
    QCOMPARE( syncState.unSeenCount(), 0u );
    QCOMPARE(syncState.unSeenOffset(), 0u);

    // Now, let's try to re-sync it once again; the difference is that our cache now has "something"
    // and that we feed it with a rather limited set of responses
    model->resyncMailbox( idxA );
    QCoreApplication::processEvents();

    // Verify that it indeed caused a re-synchronization
    list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->loading() );
    cClient(t.mk("SELECT a\r\n"));
    cServer(QByteArray("* 0 exists\r\n* NO a random no in inserted here\r\n") + t.last("OK done\r\n"));
    cEmpty();

    // Check the cache; now it should be almost empty
    syncState = model->cache()->mailboxSyncState( QString::fromAscii("a") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.flags(), QStringList() );
    QCOMPARE( syncState.isUsableForNumbers(), false );
    QCOMPARE( syncState.isUsableForSyncing(), false );
    QCOMPARE( syncState.permanentFlags(), QStringList() );
    QCOMPARE( syncState.recent(), 0u );

    // No errors
    QVERIFY( errorSpy->isEmpty() );
}

/** @short Initial sync of a mailbox that contains some messages */
void ImapModelObtainSynchronizedMailboxTest::testSyncWithMessages()
{
    existsA = 33;
    uidValidityA = 333666;
    for ( uint i = 1; i <= existsA; ++i ) {
        uidMapA.append( i * 1.3 );
    }
    uidNextA = qMax( 666u, uidMapA.last() );
    helperSyncAWithMessagesEmptyState();
    helperVerifyUidMapA();
}

/** @short Go back to a selected mailbox after some time, the mailbox doesn't have any modifications */
void ImapModelObtainSynchronizedMailboxTest::testResyncNoArrivals()
{
    existsA = 42;
    uidValidityA = 1337;
    for ( uint i = 1; i <= existsA; ++i ) {
        uidMapA.append( 6 + i * 3.6 );
    }
    uidNextA = qMax( 666u, uidMapA.last() );
    helperSyncAWithMessagesEmptyState();
    helperSyncBNoMessages();
    helperSyncAWithMessagesNoArrivals();
    helperSyncBNoMessages();
    helperSyncAWithMessagesNoArrivals();
    helperVerifyUidMapA();
    helperOneFlagUpdate( idxA.child( 0,0 ).child( 10, 0 ) );
}

/** @short Test new message arrivals happening on each resync */
void ImapModelObtainSynchronizedMailboxTest::testResyncOneNew()
{
    existsA = 17;
    uidValidityA = 800500;
    for ( uint i = 1; i <= existsA; ++i ) {
        uidMapA.append( 3 + i * 1.3 );
    }
    uidNextA = qMax( 666u, uidMapA.last() );
    helperSyncAWithMessagesEmptyState();
    helperSyncBNoMessages();
    helperSyncASomeNew( 1 );
    helperVerifyUidMapA();
    helperSyncBNoMessages();
    helperSyncASomeNew( 3 );
    helperVerifyUidMapA();
}

/** @short Test inconsistency in the local cache where UIDNEXT got decreased without UIDVALIDITY change */
void ImapModelObtainSynchronizedMailboxTest::testDecreasedUidNext()
{
    // Initial state
    existsA = 3;
    uidValidityA = 333666;
    for ( uint i = 1; i <= existsA; ++i ) {
        uidMapA.append(i);
    }
    // Make sure the UID really gets decreeased
    Q_ASSERT(uidNextA < uidMapA.last() + 1);
    uidNextA = uidMapA.last() + 1;
    helperSyncAWithMessagesEmptyState();
    helperVerifyUidMapA();
    helperSyncBNoMessages();

    // Now perform the nasty change...
    --existsA;
    uidMapA.removeLast();
    --uidNextA;

    // ...and resync again, this should scream loud, but not crash
    QCOMPARE( model->rowCount( msgListA ), 3 );
    model->switchToMailbox( idxA );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    helperSyncAFullSync();
}

/**
Test that going from an empty mailbox to a bigger one works correctly, especially that the untagged
EXISTS response which belongs to the SELECT gets picked up by the new mailbox and not the old one
*/
void ImapModelObtainSynchronizedMailboxTest::testSyncTwoLikeCyrus()
{
    // Ask the model to sync stuff
    QCOMPARE( model->rowCount( msgListB ), 0 );
    cClient(t.mk("SELECT b\r\n"));

    cServer(QByteArray("* 0 EXISTS\r\n"
                       "* 0 RECENT\r\n"
                       "* FLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen)\r\n"
                       "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen \\*)] Ok\r\n"
                       "* OK [UIDVALIDITY 1290594339] Ok\r\n"
                       "* OK [UIDNEXT 1] Ok\r\n"
                       "* OK [HIGHESTMODSEQ 1] Ok\r\n"
                       "* OK [URLMECH INTERNAL] Ok\r\n")
            + t.last("OK [READ-WRITE] Completed\r\n"));

    // Verify that we indeed received what we wanted
    Imap::Mailbox::TreeItemMsgList* listB = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListB.internalPointer() ) );
    Q_ASSERT( listB );
    QVERIFY( listB->fetched() );

    QCOMPARE( model->rowCount( msgListB ), 0 );
    cEmpty();
    QVERIFY( errorSpy->isEmpty() );

    QCOMPARE( model->rowCount( msgListA ), 0 );
    cClient(t.mk("SELECT a\r\n"));

    cServer(QByteArray("* 1 EXISTS\r\n"
                       "* 0 RECENT\r\n"
                       "* FLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen hasatt hasnotd)\r\n"
                       "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen hasatt hasnotd \\*)] Ok\r\n"
                       "* OK [UIDVALIDITY 1290593745] Ok\r\n"
                       "* OK [UIDNEXT 2] Ok\r\n"
                       "* OK [HIGHESTMODSEQ 9] Ok\r\n"
                       "* OK [URLMECH INTERNAL] Ok\r\n")
            + t.last("OK [READ-WRITE] Completed"));
    Imap::Mailbox::TreeItemMsgList* listA = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( listA );
    QVERIFY( ! listA->fetched() );
    cEmpty();
    QVERIFY( errorSpy->isEmpty() );
}

void ImapModelObtainSynchronizedMailboxTest::testSyncTwoInParallel()
{
    // This will select both mailboxes, one after another
    QCOMPARE( model->rowCount( msgListA ), 0 );
    QCOMPARE( model->rowCount( msgListB ), 0 );
    cClient(t.mk("SELECT a\r\n"));
    cServer(QByteArray("* 1 EXISTS\r\n"
                       "* 0 RECENT\r\n"
                       "* FLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen hasatt hasnotd)\r\n"
                       "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen hasatt hasnotd \\*)] Ok\r\n"
                       "* OK [UIDVALIDITY 1290593745] Ok\r\n"
                       "* OK [UIDNEXT 2] Ok\r\n"
                       "* OK [HIGHESTMODSEQ 9] Ok\r\n"
                       "* OK [URLMECH INTERNAL] Ok\r\n")
            + t.last("OK [READ-WRITE] Completed\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    QCOMPARE( model->rowCount( msgListA ), 1 );
    QCOMPARE( model->rowCount( msgListB ), 0 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( msgListA ), 1 );
    QCOMPARE( model->rowCount( msgListB ), 0 );
    // ...none of them are synced yet

    cServer(QByteArray("* SEARCH 1\r\n") + t.last("OK Completed (1 msgs in 0.000 secs)\r\n"));
    QCOMPARE( model->rowCount( msgListA ), 1 );
    // the first one should contain a message now
    cClient(t.mk("FETCH 1 (FLAGS)\r\n"));
    // without the tagged OK -- se below
    cServer(QByteArray("* 1 FETCH (FLAGS (\\Seen hasatt hasnotd))\r\n"));
    QModelIndex msg1A = model->index( 0, 0, msgListA );

    // Requesting message data should delay entering the second mailbox.
    // That's why we had to delay the tagged OK for FLAGS.
    QCOMPARE( model->data( msg1A, Imap::Mailbox::RoleMessageMessageId ), QVariant() );
    cServer(t.last("OK Completed (0.000 sec)\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( msgListA ), 1 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    cClient(t.mk(QByteArray("UID FETCH 1 (ENVELOPE BODYSTRUCTURE RFC822.SIZE)\r\n")));
    // let's try to get around without specifying ENVELOPE and BODYSTRUCTURE
    cServer(QByteArray("* 1 FETCH (UID 1 RFC822.SIZE 13021)\r\n") + t.last("OK Completed\r\n"));
    cClient(t.mk(QByteArray("SELECT b\r\n")));
    cEmpty();
    QVERIFY( errorSpy->isEmpty() );
}

/** @short Test whether a change in the UIDVALIDITY results in a complete resync */
void ImapModelObtainSynchronizedMailboxTest::testResyncUidValidity()
{
    existsA = 42;
    uidValidityA = 1337;
    for ( uint i = 1; i <= existsA; ++i ) {
        uidMapA.append( 6 + i * 3.6 );
    }
    uidNextA = qMax( 666u, uidMapA.last() );
    helperSyncAWithMessagesEmptyState();

    // Change UIDVALIDITY
    uidValidityA = 333666;
    helperSyncBNoMessages();

    QCOMPARE( model->rowCount( msgListA ), 42 );
    model->switchToMailbox( idxA );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    helperSyncAFullSync();
}

void ImapModelObtainSynchronizedMailboxTest::testFlagReSyncBenchmark()
{
    existsA = 100000;
    uidValidityA = 333;
    for (uint i = 1; i <= existsA; ++i) {
        uidMapA << i;
    }
    uidNextA = existsA + 2;
    helperSyncAWithMessagesEmptyState();

    QBENCHMARK {
        helperSyncBNoMessages();
        helperSyncAWithMessagesNoArrivals();
    }
}

/** @short Make sure that calling Model::resyncMailbox() preloads data from the cache */
void ImapModelObtainSynchronizedMailboxTest::testReloadReadsFromCache()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 3 FETCH (FLAGS (z))\r\n");
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    justKeepTask();
}

/** @short Test synchronization of a mailbox with on-disk cache being up-to-date, but no data in memory */
void ImapModelObtainSynchronizedMailboxTest::testCacheNoChange()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 3 FETCH (FLAGS (z))\r\n");
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    justKeepTask();
}

/** @short Test UIDVALIDITY changes since the last cached state */
void ImapModelObtainSynchronizedMailboxTest::testCacheUidValidity()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(333);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;

    // Fill the cache with some values which shall make sense in the "previous state"
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    // Don't forget about the flags
    model->cache()->setMsgFlags("a", 1, QStringList() << "f1");
    model->cache()->setMsgFlags("a", 6, QStringList() << "f6");
    // And even message metadata
    Imap::Mailbox::AbstractCache::MessageDataBundle bundle;
    bundle.envelope = Imap::Message::Envelope(QDateTime::currentDateTime(), QString::fromAscii("subj"),
                                              QList<Imap::Message::MailAddress>(), QList<Imap::Message::MailAddress>(),
                                              QList<Imap::Message::MailAddress>(), QList<Imap::Message::MailAddress>(),
                                              QList<Imap::Message::MailAddress>(), QList<Imap::Message::MailAddress>(),
                                              QByteArray(), QByteArray());
    bundle.uid = 1;
    model->cache()->setMessageMetadata("a", 1, bundle);
    bundle.uid = 6;
    model->cache()->setMessageMetadata("a", 6, bundle);
    // And of course also message parts
    model->cache()->setMsgPart("a", 1, "1", "blah");
    model->cache()->setMsgPart("a", 6, "1", "blah");

    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n");
    cServer(t.last("OK selected\r\n"));

    // The UIDVALIDTY change should be already discovered
    QCOMPARE(model->cache()->msgFlags("a", 1), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 3), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList());
    QCOMPARE(model->cache()->messageMetadata("a", 1), Imap::Mailbox::AbstractCache::MessageDataBundle());
    QCOMPARE(model->cache()->messageMetadata("a", 3), Imap::Mailbox::AbstractCache::MessageDataBundle());
    QCOMPARE(model->cache()->messageMetadata("a", 6), Imap::Mailbox::AbstractCache::MessageDataBundle());
    QCOMPARE(model->cache()->messagePart("a", 1, "1"), QByteArray());
    QCOMPARE(model->cache()->messagePart("a", 3, "1"), QByteArray());
    QCOMPARE(model->cache()->messagePart("a", 6, "1"), QByteArray());

    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer(QByteArray("* SEARCH 6 9 10\r\n") + t.last("OK uid search\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 3 FETCH (FLAGS (z))\r\n");
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    sync.setUidValidity(666);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    justKeepTask();
}

/** @short Test synchronization of a mailbox with on-disk cache when one new message arrives */
void ImapModelObtainSynchronizedMailboxTest::testCacheArrivals()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 4 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 16] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH UID 15:*\r\n"));
    cServer("* SEARCH 42\r\n");
    cServer(t.last("OK uids\r\n"));
    uidMap << 42;
    sync.setUidNext(43);
    sync.setExists(4);
    cClient(t.mk("FETCH 1:4 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 3 FETCH (FLAGS (z))\r\n"
            "* 4 FETCH (FLAGS (fn))\r\n");
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    QCOMPARE(model->cache()->msgFlags("a", 42), QStringList() << "fn");
    justKeepTask();
}

/** @short Number of messages grows twice

The first increment is when compared to the original state, the second one after the UID SEARCH is issued,
but before its response arrives.
*/
void ImapModelObtainSynchronizedMailboxTest::testCacheArrivalRaceDuringUid()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 4 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 16] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH UID 15:*\r\n"));
    cServer("* 5 EXISTS\r\n* SEARCH 42 43\r\n");
    cServer(t.last("OK uids\r\n"));
    uidMap << 42 << 43;
    sync.setUidNext(44);
    sync.setExists(5);
    cClient(t.mk("FETCH 1:5 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 3 FETCH (FLAGS (z))\r\n"
            "* 4 FETCH (FLAGS (fn))\r\n"
            "* 5 FETCH (FLAGS (a))\r\n");
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    QCOMPARE(model->cache()->msgFlags("a", 42), QStringList() << "fn");
    QCOMPARE(model->cache()->msgFlags("a", 43), QStringList() << "a");
    justKeepTask();
}

/** @short Number of messages grows twice

The first increment is when compared to the original state, the second one after the UID SEARCH is issued, after its untagged
response arrives, but before the tagged OK.
*/
void ImapModelObtainSynchronizedMailboxTest::testCacheArrivalRaceDuringUid2()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 4 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 16] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH UID 15:*\r\n"));
    cServer("* SEARCH 42\r\n* 5 EXISTS\r\n");
    cServer(t.last("OK uids\r\n"));
    uidMap << 42;
    sync.setUidNext(43);
    sync.setExists(5);
    QByteArray newArrivalsQuery = t.mk("UID FETCH 43:* (FLAGS)\r\n");
    QByteArray newArrivalsResponse = t.last("OK uid fetch\r\n");
    QByteArray preexistingFlagsQuery = t.mk("FETCH 1:5 (FLAGS)\r\n");
    QByteArray preexistingFlagsResponse = t.last("OK fetch\r\n");
    cClient(newArrivalsQuery + preexistingFlagsQuery);
    cServer("* 5 FETCH (UID 66 FLAGS (a))\r\n");
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 3 FETCH (FLAGS (z))\r\n"
            "* 4 FETCH (FLAGS (fn))\r\n"
            "* 5 FETCH (FLAGS (a))\r\n");
    cServer(newArrivalsResponse + preexistingFlagsResponse);
    uidMap << 66;
    sync.setUidNext(67);
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    QCOMPARE(model->cache()->msgFlags("a", 42), QStringList() << "fn");
    // UID 43 did not exist at any time after all
    QCOMPARE(model->cache()->msgFlags("a", 43), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 66), QStringList() << "a");
    justKeepTask();
}
/** @short New message arrives when syncing flags */
void ImapModelObtainSynchronizedMailboxTest::testCacheArrivalRaceDuringFlags()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 4 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 16] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH UID 15:*\r\n"));
    cServer("* SEARCH 42\r\n");
    cServer(t.last("OK uids\r\n"));
    uidMap << 42;
    sync.setUidNext(43);
    sync.setExists(4);
    cClient(t.mk("FETCH 1:4 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 5 EXISTS\r\n"
            "* 3 FETCH (FLAGS (z))\r\n"
            "* 4 FETCH (FLAGS (fn))\r\n"
            // notice that we're adding unsolicited data for a new message here
            "* 5 FETCH (FLAGS (blah))\r\n");
    // The new arrival shall not be present in the *persistent cache* at this point -- that's not an atomic update (yet)
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QByteArray fetchTermination = t.last("OK fetch\r\n");
    cClient(t.mk("UID FETCH 43:* (FLAGS)\r\n"));
    cServer(fetchTermination);
    cServer("* 5 FETCH (FLAGS (gah) UID 60)\r\n" + t.last("OK new discovery\r\n"));
    sync.setExists(5);
    sync.setUidNext(61);
    uidMap << 60;
    cEmpty();
    // At this point, the cache shall be up-to-speed again
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    QCOMPARE(model->cache()->msgFlags("a", 42), QStringList() << "fn");
    QCOMPARE(model->cache()->msgFlags("a", 60), QStringList() << "gah");
    justKeepTask();
}

/** @short Make sure that the only existing task is the KeepMailboxOpenTask and nothing else */
void ImapModelObtainSynchronizedMailboxTest::justKeepTask()
{
    QCOMPARE(model->taskModel()->rowCount(), 1);
    QModelIndex parser1 = model->taskModel()->index(0, 0);
    QVERIFY(parser1.isValid());
    QCOMPARE(model->taskModel()->rowCount(parser1), 1);
    QModelIndex firstTask = parser1.child(0, 0);
    QVERIFY(firstTask.isValid());
    QVERIFY(!firstTask.child(0, 0).isValid());
}

/** @short Test synchronization of a mailbox with on-disk cache when one message got deleted */
void ImapModelObtainSynchronizedMailboxTest::testCacheExpunges()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(6);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10 << 11 << 12 << 14;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 9, QStringList() << "foo");
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 5 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 6 10 11 12 14\r\n");
    cServer(t.last("OK uids\r\n"));
    uidMap.removeAt(1);
    sync.setExists(5);
    cClient(t.mk("FETCH 1:5 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (z))\r\n"
            "* 3 FETCH (FLAGS (a))\r\n"
            "* 4 FETCH (FLAGS (b))\r\n"
            "* 5 FETCH (FLAGS (c))\r\n"
            );
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    QCOMPARE(model->cache()->msgFlags("a", 11), QStringList() << "a");
    QCOMPARE(model->cache()->msgFlags("a", 12), QStringList() << "b");
    QCOMPARE(model->cache()->msgFlags("a", 14), QStringList() << "c");
    // Flags for the deleted message shall be gone
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList());
    justKeepTask();
}

/** @short Test two expunges, once during normal sync and then once again during the UID syncing */
void ImapModelObtainSynchronizedMailboxTest::testCacheExpungesDuringUid()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(6);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10 << 11 << 12 << 14;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 9, QStringList() << "foo");
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 5 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* 4 EXPUNGE\r\n* SEARCH 6 10 11 14\r\n");
    cServer(t.last("OK uids\r\n"));
    uidMap.removeAt(1);
    uidMap.removeAt(3);
    sync.setExists(4);
    cClient(t.mk("FETCH 1:4 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (z))\r\n"
            "* 3 FETCH (FLAGS (a))\r\n"
            "* 4 FETCH (FLAGS (c))\r\n"
            );
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    QCOMPARE(model->cache()->msgFlags("a", 11), QStringList() << "a");
    QCOMPARE(model->cache()->msgFlags("a", 14), QStringList() << "c");
    // Flags for the deleted messages shall be gone
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 12), QStringList());
    justKeepTask();
}

/** @short Test two expunges, once during normal sync and then once again immediately after the UID syncing */
void ImapModelObtainSynchronizedMailboxTest::testCacheExpungesDuringUid2()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(6);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10 << 11 << 12 << 14;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 9, QStringList() << "foo");
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 5 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 6 10 11 12 14\r\n* 4 EXPUNGE\r\n");
    cServer(t.last("OK uids\r\n"));
    uidMap.removeAt(1);
    uidMap.removeAt(3);
    sync.setExists(4);
    cClient(t.mk("FETCH 1:4 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (z))\r\n"
            "* 3 FETCH (FLAGS (a))\r\n"
            "* 4 FETCH (FLAGS (c))\r\n"
            );
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    QCOMPARE(model->cache()->msgFlags("a", 11), QStringList() << "a");
    QCOMPARE(model->cache()->msgFlags("a", 14), QStringList() << "c");
    // Flags for the deleted messages shall be gone
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 12), QStringList());
    justKeepTask();
}

/** @short Test two expunges, once during normal sync and then once again before the UID syncing */
void ImapModelObtainSynchronizedMailboxTest::testCacheExpungesDuringSelect()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(6);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10 << 11 << 12 << 14;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 9, QStringList() << "foo");
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 5 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* 4 EXPUNGE\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 6 10 11 14\r\n");
    cServer(t.last("OK uids\r\n"));
    uidMap.removeAt(1);
    uidMap.removeAt(3);
    sync.setExists(4);
    cClient(t.mk("FETCH 1:4 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (z))\r\n"
            "* 3 FETCH (FLAGS (a))\r\n"
            "* 4 FETCH (FLAGS (c))\r\n"
            );
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    QCOMPARE(model->cache()->msgFlags("a", 11), QStringList() << "a");
    QCOMPARE(model->cache()->msgFlags("a", 14), QStringList() << "c");
    // Flags for the deleted messages shall be gone
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 12), QStringList());
    justKeepTask();
}

/** @short Test two expunges, once during normal sync and then once again when syncing flags */
void ImapModelObtainSynchronizedMailboxTest::testCacheExpungesDuringFlags()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(6);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10 << 11 << 12 << 14;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 9, QStringList() << "foo");
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 5 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 6 10 11 12 14\r\n");
    cServer(t.last("OK uids\r\n"));
    uidMap.removeAt(1);
    sync.setExists(5);
    cClient(t.mk("FETCH 1:5 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (z))\r\n"
            "* 4 EXPUNGE\r\n"
            "* 3 FETCH (FLAGS (a))\r\n"
            "* 4 FETCH (FLAGS (c))\r\n"
            );
    cServer(t.last("OK fetch\r\n"));
    uidMap.removeAt(3);
    sync.setExists(4);
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    QCOMPARE(model->cache()->msgFlags("a", 11), QStringList() << "a");
    QCOMPARE(model->cache()->msgFlags("a", 14), QStringList() << "c");
    // Flags for the deleted messages shall be gone
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 12), QStringList());
    justKeepTask();
}

/** @short The mailbox contains one new message which gets deleted before we got a chance to ask for its UID */
void ImapModelObtainSynchronizedMailboxTest::testCacheArrivalsImmediatelyDeleted()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 4 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 16] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH UID 15:*\r\n"));
    cServer("* 4 EXPUNGE\r\n* SEARCH \r\n");
    cServer(t.last("OK uids\r\n"));
    // We know that at least one message has arrived, so the UIDNEXT *must* have changed.
    sync.setUidNext(16);
    sync.setExists(3);
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 3 FETCH (FLAGS (z))\r\n");
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    QCOMPARE(model->cache()->msgFlags("a", 15), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 16), QStringList());
    justKeepTask();
}

/** @short The mailbox contains one new message.  One of the old ones gets deleted while fetching UIDs. */
void ImapModelObtainSynchronizedMailboxTest::testCacheArrivalsOldDeleted()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 4 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 16] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH UID 15:*\r\n"));
    cServer("* 3 EXPUNGE\r\n* SEARCH 33\r\n");
    cServer(t.last("OK uids\r\n"));
    sync.setUidNext(34);
    uidMap.removeAt(2);
    uidMap << 33;
    sync.setExists(3);
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 3 FETCH (FLAGS (blah))\r\n");
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 33), QStringList() << "blah");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 15), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 16), QStringList());
    justKeepTask();
}

/** @short Mailbox is said to contain some new messages. While performing the sync, many events occur. */
void ImapModelObtainSynchronizedMailboxTest::testCacheArrivalsThenDynamic()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(10);
    sync.setUidValidity(666);
    sync.setUidNext(100);
    QList<uint> uidMap;
    for (uint i = 1; i <= sync.exists(); ++i)
        uidMap << i;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    // The sync indicates that there are five more messages and nothing else
    cServer("* 15 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 105] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cServer(
        // One more message arrives
        "* 16 EXISTS\r\n"
        // The last of the previously known messages gets deleted
        "* 10 EXPUNGE\r\n"
        // The very first arrival (which happened between the disconnect and this sync) goes away
        "* 10 EXPUNGE\r\n"
            );
    cClient(t.mk("UID SEARCH UID 100:*\r\n"));
    // One of the old messages get deleted
    cServer("* 3 EXPUNGE\r\n");
    cServer("* SEARCH 102 103 104 105 106\r\n");
    cServer(t.last("OK uids\r\n"));
    // That's the last of the previously known, the first "10 EXPUNGE"
    uidMap.removeAt(9);
    // the second "10 EXPUNGE" is not reflected here at all, as it's for the new arrivals
    // This one is for the "3 EXPUNGE"
    uidMap.removeAt(2);
    uidMap << 102 << 103 << 104 << 105 << 106;
    sync.setUidNext(107);
    sync.setExists(13);
    cClient(t.mk("FETCH 1:13 (FLAGS)\r\n"));
    cServer("* 2 EXPUNGE\r\n");
    uidMap.removeAt(1);
    cServer("* 12 EXPUNGE\r\n");
    uidMap.removeAt(11);
    sync.setExists(11);
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 3 FETCH (FLAGS (z))\r\n"
            "* 4 FETCH (FLAGS (a))\r\n"
            "* 5 FETCH (FLAGS (b))\r\n"
            "* 6 FETCH (FLAGS (c))\r\n"
            "* 7 FETCH (FLAGS (d))\r\n"
            "* 8 FETCH (FLAGS (x102))\r\n"
            "* 9 FETCH (FLAGS (x103))\r\n"
            "* 10 FETCH (FLAGS (x104))\r\n"
            "* 11 FETCH (FLAGS (x105))\r\n");
    // Add two, delete the last of them
    cServer("* 13 EXISTS\r\n* 13 EXPUNGE\r\n");
    sync.setExists(12);
    cServer(t.last("OK fetch\r\n"));
    cClient(t.mk("UID FETCH 107:* (FLAGS)\r\n"));
    cServer("* 12 FETCH (UID 109 FLAGS (last))\r\n");
    uidMap << 109;
    cServer(t.last("OK uid fetch flags done\r\n"));
    cEmpty();
    sync.setUidNext(110);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    // Known UIDs at this point: 1, 4, 5, 6, 7, 8, 9, 102, 103, 104, 105, 109
    QCOMPARE(model->cache()->msgFlags("a", 1), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 4), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 5), QStringList() << "z");
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "a");
    QCOMPARE(model->cache()->msgFlags("a", 7), QStringList() << "b");
    QCOMPARE(model->cache()->msgFlags("a", 8), QStringList() << "c");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "d");
    QCOMPARE(model->cache()->msgFlags("a", 102), QStringList() << "x102");
    QCOMPARE(model->cache()->msgFlags("a", 103), QStringList() << "x103");
    QCOMPARE(model->cache()->msgFlags("a", 104), QStringList() << "x104");
    QCOMPARE(model->cache()->msgFlags("a", 105), QStringList() << "x105");
    QCOMPARE(model->cache()->msgFlags("a", 109), QStringList() << "last");
    QCOMPARE(model->cache()->msgFlags("a", 2), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 3), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 100), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 101), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 106), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 107), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 108), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 110), QStringList());
    justKeepTask();
}


TROJITA_HEADLESS_TEST( ImapModelObtainSynchronizedMailboxTest )
