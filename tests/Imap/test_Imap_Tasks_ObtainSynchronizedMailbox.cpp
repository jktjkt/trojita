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

#include "test_Imap_Tasks_ObtainSynchronizedMailbox.h"
#include "Utils/headless_test.h"
#include "Utils/FakeCapabilitiesInjector.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Imap/Tasks/ObtainSynchronizedMailboxTask.h"


void ImapModelObtainSynchronizedMailboxTest::init()
{
    LibMailboxSync::init();

    using namespace Imap::Mailbox;
    MsgListModel *msgListModelA = new MsgListModel(model, model);
    ThreadingMsgListModel *threadingA = new ThreadingMsgListModel(msgListModelA);
    threadingA->setSourceModel(msgListModelA);
    MsgListModel *msgListModelB = new MsgListModel(model, model);
    ThreadingMsgListModel *threadingB = new ThreadingMsgListModel(msgListModelB);
    threadingB->setSourceModel(msgListModelB);
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
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QLatin1String("a") );
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
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QLatin1String("a") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.flags(), QStringList() << QLatin1String("\\Answered") <<
              QLatin1String("\\Flagged") << QLatin1String("\\Deleted") <<
              QLatin1String("\\Seen") << QLatin1String("\\Draft") );
    QCOMPARE( syncState.isUsableForNumbers(), false );
    QCOMPARE( syncState.isUsableForSyncing(), true );
    QCOMPARE( syncState.permanentFlags(), QStringList() << QLatin1String("\\Answered") <<
              QLatin1String("\\Flagged") << QLatin1String("\\Deleted") <<
              QLatin1String("\\Seen") << QLatin1String("\\Draft") << QLatin1String("\\*") );
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
    syncState = model->cache()->mailboxSyncState( QLatin1String("a") );
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

    cClient(t.mk("UID FETCH 1 (" FETCH_METADATA_ITEMS ")\r\n"));
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
    bundle.envelope = Imap::Message::Envelope(QDateTime::currentDateTime(), QLatin1String("subj"),
                                              QList<Imap::Message::MailAddress>(), QList<Imap::Message::MailAddress>(),
                                              QList<Imap::Message::MailAddress>(), QList<Imap::Message::MailAddress>(),
                                              QList<Imap::Message::MailAddress>(), QList<Imap::Message::MailAddress>(),
                                              QList<QByteArray>(), QByteArray());
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

void ImapModelObtainSynchronizedMailboxTest::testCacheArrivalRaceDuringUid()
{
    helperCacheArrivalRaceDuringUid(WITHOUT_ESEARCH);
}

void ImapModelObtainSynchronizedMailboxTest::testCacheArrivalRaceDuringUid_ESearch()
{
    helperCacheArrivalRaceDuringUid(WITH_ESEARCH);
}

/** @short Number of messages grows twice

The first increment is when compared to the original state, the second one after the UID SEARCH is issued,
but before its response arrives.
*/
void ImapModelObtainSynchronizedMailboxTest::helperCacheArrivalRaceDuringUid(const ESearchMode esearch)
{
    if (esearch == WITH_ESEARCH) {
        FakeCapabilitiesInjector injector(model);
        injector.injectCapability("ESEARCH");
    }
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
    if (esearch == WITH_ESEARCH) {
        cClient(t.mk("UID SEARCH RETURN (ALL) UID 15:*\r\n"));
        cServer(QByteArray("* 5 EXISTS\r\n* ESEARCH (TAG ") + t.last() + ") UID ALL 42:43\r\n");
    } else {
        cClient(t.mk("UID SEARCH UID 15:*\r\n"));
        cServer("* 5 EXISTS\r\n* SEARCH 42 43\r\n");
    }
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
    uidMap << 42 << 60;
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



void ImapModelObtainSynchronizedMailboxTest::testCacheExpunges()
{
    helperCacheExpunges(WITHOUT_ESEARCH);
}

void ImapModelObtainSynchronizedMailboxTest::testCacheExpunges_ESearch()
{
    helperCacheExpunges(WITH_ESEARCH);
}

/** @short Test synchronization of a mailbox with on-disk cache when one message got deleted */
void ImapModelObtainSynchronizedMailboxTest::helperCacheExpunges(const ESearchMode esearch)
{
    if (esearch == WITH_ESEARCH) {
        FakeCapabilitiesInjector injector(model);
        injector.injectCapability("ESEARCH");
    }
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
    if (esearch == WITH_ESEARCH) {
        cClient(t.mk("UID SEARCH RETURN (ALL) ALL\r\n"));
        cServer(QByteArray("* ESEARCH (TAG ") + t.last() + ") UID ALL 6,10:12,14\r\n");
    } else {
        cClient(t.mk("UID SEARCH ALL\r\n"));
        cServer("* SEARCH 6 10 11 12 14\r\n");
    }
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

/** @short Mailbox is said to have lost some messages. While performing the sync, many events occur. */
void ImapModelObtainSynchronizedMailboxTest::testCacheDeletionsThenDynamic()
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
    cServer("* 5 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 100] .\r\n");
    cServer(t.last("OK selected\r\n"));
    // Let's say that the server's idea about the UIDs is now (1 3 4 6 9)
    cServer(
        // Five more messages arrives; that means that the UIDNEXT is now at least on 105
        "* 10 EXISTS\r\n"
        // The last of the previously known messages gets deleted
        "* 5 EXPUNGE\r\n"
        // Right now, the UIDs are (1 3 4 6 ? ? ? ? ?)
        // And the first of the new arrivals will be gone, too.
        "* 5 EXPUNGE\r\n"
        // That makes four new messages when compared to the original state.
        // Right now, the UIDs are (1 3 4 6 ? ? ? ?)
            );
    cClient(t.mk("UID SEARCH ALL\r\n"));
    // One of the old messages get deleted (UID 4)
    cServer("* 3 EXPUNGE\r\n");
    // Right now, the UIDs are (1 3 6 ? ? ? ?)
    cServer("* SEARCH 1 3 6 101 102 103 104\r\n");
    cServer(t.last("OK uids\r\n"));

    uidMap.clear();
    uidMap << 1 << 3 << 6 << 101 << 102 << 103 << 104;
    sync.setUidNext(105);
    sync.setExists(7);
    cClient(t.mk("FETCH 1:7 (FLAGS)\r\n"));
    cServer("* 2 EXPUNGE\r\n");
    uidMap.removeAt(1);
    sync.setExists(6);
    cServer("* 1 FETCH (FLAGS (f1))\r\n"
            "* 2 FETCH (FLAGS (f6))\r\n"
            "* 3 FETCH (FLAGS (f101))\r\n"
            "* 4 FETCH (FLAGS (f102))\r\n"
            "* 5 FETCH (FLAGS (f103))\r\n"
            "* 6 FETCH (FLAGS (f104))\r\n");
    // Add two, delete the last of them
    cServer("* 8 EXISTS\r\n* 8 EXPUNGE\r\n");
    sync.setExists(7);
    cServer(t.last("OK fetch\r\n"));
    cClient(t.mk("UID FETCH 105:* (FLAGS)\r\n"));
    cServer("* 7 FETCH (UID 109 FLAGS (last))\r\n");
    uidMap << 109;
    cServer(t.last("OK uid fetch flags done\r\n"));
    cEmpty();
    sync.setUidNext(110);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    // UIDs: 1 6 101 102 103 104 109
    QCOMPARE(model->cache()->msgFlags("a", 1), QStringList() << "f1");
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "f6");
    QCOMPARE(model->cache()->msgFlags("a", 101), QStringList() << "f101");
    QCOMPARE(model->cache()->msgFlags("a", 102), QStringList() << "f102");
    QCOMPARE(model->cache()->msgFlags("a", 103), QStringList() << "f103");
    QCOMPARE(model->cache()->msgFlags("a", 104), QStringList() << "f104");
    QCOMPARE(model->cache()->msgFlags("a", 109), QStringList() << "last");
    for (int i=2; i < 6; ++i)
        QCOMPARE(model->cache()->msgFlags("a", i), QStringList());
    for (int i=7; i < 101; ++i)
        QCOMPARE(model->cache()->msgFlags("a", i), QStringList());
    for (int i=105; i < 109; ++i)
        QCOMPARE(model->cache()->msgFlags("a", i), QStringList());
    for (int i=110; i < 120; ++i)
        QCOMPARE(model->cache()->msgFlags("a", i), QStringList());
    justKeepTask();
}

/** @short Test what happens when HIGHESTMODSEQ says there are no changes */
void ImapModelObtainSynchronizedMailboxTest::testCondstoreNoChanges()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("CONDSTORE");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (CONDSTORE)\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 33] .\r\n"
            );
    cServer(t.last("OK selected\r\n"));
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    justKeepTask();
}

/** @short Test changed HIGHESTMODSEQ */
void ImapModelObtainSynchronizedMailboxTest::testCondstoreChangedFlags()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("CONDSTORE");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (CONDSTORE)\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 666] .\r\n"
            );
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS) (CHANGEDSINCE 33)\r\n"));
    cServer("* 3 FETCH (FLAGS (f101))\r\n");
    cServer(t.last("OK fetched\r\n"));
    cEmpty();
    sync.setHighestModSeq(666);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "f101");
    justKeepTask();
}

/** @short Test constant HIGHESTMODSEQ and more EXISTS */
void ImapModelObtainSynchronizedMailboxTest::testCondstoreErrorExists()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("CONDSTORE");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (CONDSTORE)\r\n"));
    cServer("* 4 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 33] .\r\n"
            );
    // yes, it's buggy. The goal here is to make sure that even an increased EXISTS is enough
    // to disable CHANGEDSINCE
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer(QByteArray("* SEARCH 6 9 10 15\r\n") + t.last("OK uids\r\n"));
    cClient(t.mk("FETCH 1:4 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 3 FETCH (FLAGS (z))\r\n"
            "* 4 FETCH (FLAGS (blah))\r\n");
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    uidMap << 15;
    sync.setUidNext(16);
    sync.setExists(4);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    QCOMPARE(model->cache()->msgFlags("a", 15), QStringList() << "blah");
    justKeepTask();
}

/** @short Test constant HIGHESTMODSEQ but changed UIDNEXT */
void ImapModelObtainSynchronizedMailboxTest::testCondstoreErrorUidNext()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("CONDSTORE");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (CONDSTORE)\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 16] .\r\n"
            "* OK [HIGHESTMODSEQ 33] .\r\n"
            );
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer(QByteArray("* SEARCH 6 9 10\r\n") + t.last("OK uids\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 3 FETCH (FLAGS (z))\r\n");
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    sync.setUidNext(16);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    justKeepTask();
}

/** @short Changed UIDVALIDITY shall always lead to full sync, no matter what HIGHESTMODSEQ says */
void ImapModelObtainSynchronizedMailboxTest::testCondstoreUidValidity()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("CONDSTORE");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (CONDSTORE)\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 333] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 33] .\r\n"
            );
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer(QByteArray("* SEARCH 6 9 10\r\n") + t.last("OK uids\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 3 FETCH (FLAGS (z))\r\n");
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    sync.setUidValidity(333);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    justKeepTask();
}

/** @short Test decreased HIGHESTMODSEQ */
void ImapModelObtainSynchronizedMailboxTest::testCondstoreDecreasedHighestModSeq()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("CONDSTORE");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (CONDSTORE)\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 1] .\r\n"
            );
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x1))\r\n"
            "* 2 FETCH (FLAGS (x2))\r\n"
            "* 3 FETCH (FLAGS (x3))\r\n");
    cServer(t.last("OK fetched\r\n"));
    cEmpty();
    sync.setHighestModSeq(1);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x1");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "x2");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "x3");
    justKeepTask();
}


/** @short Test that we deal with discrepancy between the EXISTS and the number of UIDs in the cache
and that FLAGS are completely re-fetched even in presence of the HIGHESTMODSEQ */
void ImapModelObtainSynchronizedMailboxTest::testCacheDiscrepancyExistsUidsConstantHMS()
{
    helperCacheDiscrepancyExistsUids(true);
}

/** @short Test that we deal with discrepancy between the EXISTS and the number of UIDs in the cache
and that FLAGS are fetched even if the HIGHESTMODSEQ remains the same */
void ImapModelObtainSynchronizedMailboxTest::testCacheDiscrepancyExistsUidsDifferentHMS()
{
    helperCacheDiscrepancyExistsUids(false);
}

void ImapModelObtainSynchronizedMailboxTest::helperCacheDiscrepancyExistsUids(bool constantHighestModSeq)
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("QRESYNC");

    uidMapA << 5 << 6;
    existsA = 2;
    uidNextA = 10;
    uidValidityA = 123;

    helperSyncAWithMessagesEmptyState();
    helperCheckCache();

    helperSyncBNoMessages();

    injector.injectCapability("ESEARCH");
    model->switchToMailbox(idxA);

    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(10);
    sync.setHighestModSeq(111);
    QList<uint> uidMap;
    uidMap << 5 << 6 << 7 << 8;
    model->cache()->setMailboxSyncState(QLatin1String("a"), sync);
    model->cache()->setUidMapping(QLatin1String("a"), uidMap);
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 111 (3 7)))\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* 1 RECENT\r\n"
            "* OK [UNSEEN 5] x\r\n"
            "* OK [UIDVALIDITY 666] x\r\n"
            "* OK [UIDNEXT 10] x\r\n"
            "* OK [HIGHESTMODSEQ " + QByteArray::number(constantHighestModSeq ? 111 : 112) + "] x\r\n"
            "* VANISHED (EARLIER) 8\r\n" +
            t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH RETURN (ALL) ALL\r\n"));
    cServer("* ESEARCH (TAG \"" + t.last() + "\") UID ALL 5:7\r\n");
    cServer(t.last("OK fetched\r\n"));
    // This test makes sure that the flags are synced after the UID mapping vs. EXISTS discrepancy is detected.
    // Otherwise we might get some nonsense like all message marked as read, etc.
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (a))\r\n"
            "* 2 FETCH (FLAGS (\\Seen))\r\n"
            "* 3 FETCH (FLAGS (c))\r\n" +
            t.last("OK fetched\r\n"));
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 2);
    cEmpty();
    justKeepTask();
}

void ImapModelObtainSynchronizedMailboxTest::helperTestQresyncNoChanges(ModeForHelperTestQresyncNoChanges mode)
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("QRESYNC");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
    if (mode == EXTRA_ENABLED) {
        cServer("* ENABLED CONDSTORE QRESYNC\r\n");
    }
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 33] .\r\n"
            );
    cServer(t.last("OK selected\r\n"));
    cEmpty();
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    // deactivate envelope preloading
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_EXPENSIVE);
    requestAndCheckSubject(0, "subject 6");
    justKeepTask();
}

/** @short Test QRESYNC when there are no changes */
void ImapModelObtainSynchronizedMailboxTest::testQresyncNoChanges()
{
    helperTestQresyncNoChanges(JUST_QRESYNC);
}

/** @short Test QRESYNC reporting changed flags */
void ImapModelObtainSynchronizedMailboxTest::testQresyncChangedFlags()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("QRESYNC");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 36] .\r\n"
            "* 2 FETCH (UID 9 FLAGS (x2 \\Seen))\r\n"
            );
    cServer(t.last("OK selected\r\n"));
    cEmpty();
    sync.setHighestModSeq(36);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "\\Seen" << "x2");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");

    // make sure that we've picked up possible flag change about unread messages
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 2);

    // deactivate envelope preloading
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_EXPENSIVE);
    requestAndCheckSubject(0, "subject 6");
    justKeepTask();
}

/** @short Test QRESYNC using VANISHED EARLIER */
void ImapModelObtainSynchronizedMailboxTest::testQresyncVanishedEarlier()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("QRESYNC");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
    cServer("* 2 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 36] .\r\n"
            "* VANISHED (EARLIER) 1:5,9,11:13\r\n"
            );
    cServer(t.last("OK selected\r\n"));
    cEmpty();
    sync.setHighestModSeq(36);
    sync.setExists(2);
    uidMap.removeOne(9);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    justKeepTask();
}

/** @short Test QRESYNC when UIDVALIDITY changes */
void ImapModelObtainSynchronizedMailboxTest::testQresyncUidValidity()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("QRESYNC");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 333] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 33] .\r\n"
            );
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer(QByteArray("* SEARCH 6 9 10\r\n") + t.last("OK uids\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (\\Seen))\r\n"
            "* 3 FETCH (FLAGS (z))\r\n");
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    sync.setUidValidity(333);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "\\Seen");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 2);
    justKeepTask();
}

/** @short Test QRESYNC reporting changed flags */
void ImapModelObtainSynchronizedMailboxTest::testQresyncNoModseqChangedFlags()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("QRESYNC");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [NOMODSEQ] .\r\n"
            );
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x1))\r\n"
            "* 2 FETCH (FLAGS (x2))\r\n"
            "* 3 FETCH (FLAGS (x3))\r\n"
            + t.last("OK flags\r\n"));
    cEmpty();
    sync.setHighestModSeq(0);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x1");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "x2");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "x3");
    justKeepTask();
}

/** @short Test that EXISTS change with unchanged HIGHESTMODSEQ cancels QRESYNC */
void ImapModelObtainSynchronizedMailboxTest::testQresyncErrorExists()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("QRESYNC");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
    cServer("* 4 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 33] .\r\n"
            );
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 6 9 10 12\r\n" + t.last("OK search\r\n"));
    cClient(t.mk("FETCH 1:4 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x1))\r\n"
            "* 2 FETCH (FLAGS (x2))\r\n"
            "* 3 FETCH (FLAGS (x3))\r\n"
            "* 4 FETCH (FLAGS (x4))\r\n"
            + t.last("OK flags\r\n"));
    cEmpty();
    sync.setExists(4);
    sync.setHighestModSeq(0);
    uidMap << 12;
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x1");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "x2");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "x3");
    QCOMPARE(model->cache()->msgFlags("a", 12), QStringList() << "x4");
    QCOMPARE(model->cache()->msgFlags("a", 15), QStringList());
    justKeepTask();
}

/** @short Test that UIDNEXT change with unchanged HIGHESTMODSEQ cancels QRESYNC */
void ImapModelObtainSynchronizedMailboxTest::testQresyncErrorUidNext()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("QRESYNC");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 20] .\r\n"
            "* OK [HIGHESTMODSEQ 33] .\r\n"
            );
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 6 9 10\r\n" + t.last("OK search\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x1))\r\n"
            "* 2 FETCH (FLAGS (x2))\r\n"
            "* 3 FETCH (FLAGS (x3))\r\n"
            + t.last("OK flags\r\n"));
    cEmpty();
    sync.setUidNext(20);
    sync.setHighestModSeq(0);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x1");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "x2");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "x3");
    justKeepTask();
}

/** @short Test QRESYNC when something has arrived and the server haven't told us about that */
void ImapModelObtainSynchronizedMailboxTest::testQresyncUnreportedNewArrivals()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("QRESYNC");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
    cServer("* 4 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 20] .\r\n"
            "* OK [HIGHESTMODSEQ 34] .\r\n"
            );
    QCOMPARE(model->rowCount(msgListA), 4);
    QCOMPARE(msgListA.child(3, 0).data(Imap::Mailbox::RoleMessageUid).toUInt(), 0u);
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID FETCH 15:* (FLAGS)\r\n"));
    cServer("* 4 FETCH (FLAGS (x4) UID 16)\r\n" + t.last("OK uid fetch flags\r\n"));
    cEmpty();
    sync.setExists(4);
    sync.setUidNext(20);
    uidMap << 16;
    sync.setHighestModSeq(34);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    justKeepTask();
}

/** @short Test QRESYNC when something has arrived and the server reported UID of that stuff on its own */
void ImapModelObtainSynchronizedMailboxTest::testQresyncReportedNewArrivals()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("QRESYNC");
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    QList<uint> uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 6, QStringList() << "x");
    model->cache()->setMsgFlags("a", 9, QStringList() << "y");
    model->cache()->setMsgFlags("a", 10, QStringList() << "z");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
    cServer("* 4 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 20] .\r\n"
            "* OK [HIGHESTMODSEQ 34] .\r\n"
            "* 4 FETCH (FLAGS (x4) UID 16)\r\n"
            );
    QCOMPARE(model->rowCount(msgListA), 4);
    QCOMPARE(msgListA.child(3, 0).data(Imap::Mailbox::RoleMessageUid).toUInt(), 16u);
    cServer(t.last("OK selected\r\n"));
    cEmpty();
    sync.setExists(4);
    sync.setUidNext(20);
    uidMap << 16;
    sync.setHighestModSeq(34);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "z");
    // deactivate envelope preloading
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_EXPENSIVE);
    requestAndCheckSubject(0, "subject 6");
    justKeepTask();
}

/** @short QRESYNC where the number of messaged actually removed by the VANISHED EARLIER is equal to number of new arrivals */
void ImapModelObtainSynchronizedMailboxTest::testQresyncDeletionsNewArrivals()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("QRESYNC");
    Imap::Mailbox::SyncState sync;
    sync.setExists(5);
    sync.setUidValidity(666);
    sync.setUidNext(6);
    sync.setHighestModSeq(10);
    QList<uint> uidMap;
    uidMap << 1 << 2 << 3 << 4 << 5;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->cache()->setMsgFlags("a", 1, QStringList() << "1");
    model->cache()->setMsgFlags("a", 2, QStringList() << "2");
    model->cache()->setMsgFlags("a", 3, QStringList() << "3");
    model->cache()->setMsgFlags("a", 4, QStringList() << "4");
    model->cache()->setMsgFlags("a", 5, QStringList() << "5");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 10 (3,5 3,5)))\r\n"));
    cServer("* 5 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 10] .\r\n"
            "* OK [HIGHESTMODSEQ 34] .\r\n"
            "* VANISHED (EARLIER) 1:3\r\n"
            "* 3 FETCH (UID 6 FLAGS (6))\r\n"
            "* 4 FETCH (UID 7 FLAGS (7))\r\n"
            "* 5 FETCH (UID 8 FLAGS (8))\r\n"
            );
    uidMap.removeOne(1);
    uidMap.removeOne(2);
    uidMap.removeOne(3);
    uidMap << 6 << 7 << 8;
    QCOMPARE(model->rowCount(msgListA), 5);
    cServer(t.last("OK selected\r\n"));
    cEmpty();
    sync.setUidNext(10);
    sync.setHighestModSeq(34);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    // these were removed
    QCOMPARE(model->cache()->msgFlags("a", 1), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 2), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 3), QStringList());
    // these are still present
    QCOMPARE(model->cache()->msgFlags("a", 4), QStringList() << "4");
    QCOMPARE(model->cache()->msgFlags("a", 5), QStringList() << "5");
    // and these are the new arrivals
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "6");
    QCOMPARE(model->cache()->msgFlags("a", 7), QStringList() << "7");
    QCOMPARE(model->cache()->msgFlags("a", 8), QStringList() << "8");
    justKeepTask();
}

/** @short Test that extra SEARCH responses don't cause asserts */
void ImapModelObtainSynchronizedMailboxTest::testSpuriousSearch()
{
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer(QByteArray("* 0 exists\r\n"));

    {
        ExpectSingleErrorHere blocker(this);
        cServer("* SEARCH \r\n");
    }
}

/** @short Test how unexpected ESEARCH operates */
void ImapModelObtainSynchronizedMailboxTest::testSpuriousESearch()
{
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer(QByteArray("* 0 exists\r\n"));

    {
        ExpectSingleErrorHere blocker(this);
        cServer("* ESEARCH (TAG \"\") UID \r\n");
    }
}

/** @short Mailbox synchronization without the UIDNEXT -- this is what Courier 4.5.0 is happy to return */
void ImapModelObtainSynchronizedMailboxTest::testSyncNoUidnext()
{
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* 0 RECENT\r\n"
            "* OK [UIDVALIDITY 1336643053] Ok\r\n"
            "* OK [MYRIGHTS \"acdilrsw\"] ACL\r\n"
            + t.last("OK [READ-WRITE] Ok\r\n"));
    QCOMPARE(model->rowCount(msgListA), 3);
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 1212 1214 1215\r\n");
    cServer(t.last("OK search\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (uid1212))\r\n"
            "* 2 FETCH (FLAGS (uid1214))\r\n"
            "* 3 FETCH (FLAGS (uid1215))\r\n"
            + t.last("OK fetch\r\n"));
    cEmpty();
    justKeepTask();

    // Verify the cache
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(1336643053);
    sync.setRecent(0);
    // The UIDNEXT shall be updated automatically
    sync.setUidNext(1216);
    QList<uint> uidMap;
    uidMap << 1212 << 1214 << 1215;

    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 1212), QStringList() << "uid1212");
    QCOMPARE(model->cache()->msgFlags("a", 1214), QStringList() << "uid1214");
    QCOMPARE(model->cache()->msgFlags("a", 1215), QStringList() << "uid1215");

    // Switch away from this mailbox
    helperSyncBNoMessages();

    // Make sure that we catch UIDNEXT missing by purging the cache
    model->cache()->setMsgPart("a", 1212, QString(), "foo");

    // Now go back to mailbox A
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* 0 RECENT\r\n"
            "* OK [UIDVALIDITY 1336643053] .\r\n"
            "* OK [MYRIGHTS \"acdilrsw\"] ACL\r\n"
            );
    QCOMPARE(model->rowCount(msgListA), 3);
    cServer(t.last("OK selected\r\n"));
    // The UIDNEXT is missing -> resyncing the UIDs again
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 1212 1214 1215\r\n");
    cServer(t.last("OK search\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (uid1212))\r\n"
            "* 2 FETCH (FLAGS (uid1214))\r\n"
            "* 3 FETCH (FLAGS (uid1215))\r\n"
            + t.last("OK fetch\r\n"));
    cEmpty();
    justKeepTask();

    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);

    // Missing UIDNEXT is a violation of the IMAP protocol specification, we treat that like a severe error and fall back to
    // a full synchronization which means that any cached data is discarded
    QCOMPARE(model->cache()->messagePart("a", 1212, QString()), QByteArray());
}

/** @short Test that we can open a mailbox using just the cached data when offline */
void ImapModelObtainSynchronizedMailboxTest::testOfflineOpening()
{
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_OFFLINE);
    cClient(t.mk("LOGOUT\r\n"));
    cServer(t.last("OK logged out\r\n"));

    // prepare the cache
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(333);
    sync.setRecent(0);
    sync.setUidNext(666);
    QList<uint> uidMap;
    uidMap << 10 << 20 << 30;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    Imap::Mailbox::AbstractCache::MessageDataBundle msg10, msg20;
    msg10.uid = 10;
    msg10.envelope.subject = "msg10";
    msg20.uid = 20;
    msg20.envelope.subject = "msg20";

    // Prepare the body structure for this message
    int start = 0;
    Imap::Responses::Fetch fetchResponse(666, QByteArray(" (BODYSTRUCTURE (\"text\" \"plain\" (\"chaRset\" \"UTF-8\" "
                                                         "\"format\" \"flowed\") NIL NIL \"8bit\" 362 15 NIL NIL NIL))\r\n"),
                                         start);
    msg10.serializedBodyStructure = dynamic_cast<const Imap::Responses::RespData<QByteArray>&>(*(fetchResponse.data["x-trojita-bodystructure"])).data;
    msg20.serializedBodyStructure = msg10.serializedBodyStructure;

    model->cache()->setMessageMetadata("a", 10, msg10);
    model->cache()->setMessageMetadata("a", 20, msg20);

    // Check that stuff works
    QCOMPARE(model->rowCount(msgListA), 0);
    QCoreApplication::processEvents();
    QCOMPARE(model->rowCount(msgListA), 3);
    checkCachedSubject(0, "msg10");
    checkCachedSubject(1, "msg20");
    checkCachedSubject(2, "");
    QCOMPARE(msgListA.child(2, 0).data(Imap::Mailbox::RoleIsFetched).toBool(), false);

    QCOMPARE(model->taskModel()->rowCount(), 0);

    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
}

/** @short Check that ENABLE QRESYNC always gets sent prior to SELECT QRESYNC

See Redmine #611 for details.
*/
void ImapModelObtainSynchronizedMailboxTest::testQresyncEnabling()
{
    using namespace Imap::Mailbox;

    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_OFFLINE);
    cClient(t.mk("LOGOUT\r\n"));
    cServer(t.last("OK logged out\r\n"));

    // this one is important, otherwise the index would get invalidated "too fast"
    taskFactoryUnsafe->fakeListChildMailboxes = false;
    // we need to tap into the whole connection establishing process; otherwise KeepMailboxOpenTask asserts on line 80
    taskFactoryUnsafe->fakeOpenConnectionTask = false;
    factory->setInitialState(Imap::CONN_STATE_CONNECTED_PRETLS_PRECAPS);
    t.reset();

    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    QCoreApplication::processEvents();
    cServer("* OK [CAPABILITY IMAP4rev1] hi there\r\n");
    QCOMPARE(model->rowCount(QModelIndex()), 26);
    idxA = model->index(1, 0, QModelIndex());
    QVERIFY(idxA.isValid());
    QCOMPARE(idxA.data(RoleMailboxName).toString(), QString("a"));
    msgListA = idxA.child(0, 0);
    QVERIFY(idxA.isValid());
    QCOMPARE(model->rowCount(msgListA), 0);
    cEmpty();
    model->setImapUser(QLatin1String("user"));
    model->setImapPassword(QLatin1String("pw"));
    cClient(t.mk("LOGIN user pw\r\n"));
    cServer(t.last("OK [CAPABILITY IMAP4rev1 ENABLE QRESYNC UNSELECT] logged in\r\n"));

    QByteArray idCmd = t.mk("ENABLE QRESYNC\r\n");
    QByteArray idRes = t.last("OK enabled\r\n");
    QByteArray listCmd = t.mk("LIST \"\" \"%\"\r\n");
    QByteArray listResp = t.last("OK listed\r\n");
    QByteArray selectCmd = t.mk("SELECT a\r\n");
    QByteArray selectResp = t.last("OK selected\r\n");

    cClient(idCmd + listCmd + selectCmd);
    cServer(idRes + "* LIST (\\HasNoChildren) \".\" \"a\"\r\n" + listResp);
    cServer(selectResp);
    cClient(t.mk("UNSELECT\r\n"));
    cServer(t.last("OK whatever\r\n"));
    cEmpty();
}

/** @short VANISHED EARLIER which refers to meanwhile-arrived-and-deleted messages on an empty mailbox */
void ImapModelObtainSynchronizedMailboxTest::testQresyncSpuriousVanishedEarlier()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("ESEARCH");
    injector.injectCapability("QRESYNC");
    Imap::Mailbox::SyncState sync;
    sync.setExists(0);
    sync.setUidValidity(1309542826);
    sync.setUidNext(252);
    sync.setHighestModSeq(10);
    QList<uint> uidMap;
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", uidMap);
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (1309542826 10))\r\n"));
    cServer("* 0 EXISTS\r\n"
            "* OK [UIDVALIDITY 1309542826] UIDs valid\r\n"
            "* OK [UIDNEXT 256] Predicted next UID\r\n"
            "* OK [HIGHESTMODSEQ 22] Highest\r\n"
            "* VANISHED (EARLIER) 252:255\r\n"
            );
    QCOMPARE(model->rowCount(msgListA), 0);
    cServer(t.last("OK selected\r\n"));
    cEmpty();
    sync.setUidNext(256);
    sync.setHighestModSeq(22);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), 0);
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    justKeepTask();
}

/** @short QRESYNC synchronization of a formerly empty mailbox which now contains some messages

This was reported by paalsteek as being broken.
*/
void ImapModelObtainSynchronizedMailboxTest::testQresyncAfterEmpty()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("ESEARCH");
    injector.injectCapability("QRESYNC");
    Imap::Mailbox::SyncState sync;
    sync.setUidValidity(1336686200);
    sync.setExists(0);
    sync.setUidNext(1);
    sync.setHighestModSeq(1);
    model->cache()->setMailboxSyncState("a", sync);
    model->cache()->setUidMapping("a", QList<uint>());
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (1336686200 1))\r\n"));
    cServer("* 6 EXISTS\r\n"
            "* OK [UIDVALIDITY 1336686200] UIDs valid\r\n"
            "* OK [UIDNEXT 7] Predicted next UID\r\n"
            "* OK [HIGHESTMODSEQ 3] Highest\r\n"
            "* 1 FETCH (MODSEQ (2) UID 1 FLAGS (\\Seen \\Recent))\r\n"
            "* 2 FETCH (MODSEQ (2) UID 2 FLAGS (\\Seen \\Recent))\r\n"
            "* 3 FETCH (MODSEQ (2) UID 3 FLAGS (\\Seen \\Recent))\r\n"
            "* 4 FETCH (MODSEQ (2) UID 4 FLAGS (\\Seen \\Recent))\r\n"
            "* 5 FETCH (MODSEQ (2) UID 5 FLAGS (\\Seen \\Recent))\r\n"
            "* 6 FETCH (MODSEQ (2) UID 6 FLAGS (\\Seen \\Recent))\r\n"
            + t.last("OK [READ-WRITE] Select completed.\r\n")
            );
    existsA = 6;
    uidNextA = 7;
    uidValidityA = 1336686200;
    for (uint i = 1; i <= existsA; ++i)
        uidMapA << i;
    helperCheckCache();
    cEmpty();
    justKeepTask();
}

/** @short Test QRESYNC/CONDSTORE initial sync on Devocot which reports NOMODSEQ followed by HIGHESTMODSEQ 1

This is apparently a real-world issue.
*/
void ImapModelObtainSynchronizedMailboxTest::testCondstoreQresyncNomodseqHighestmodseq()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability("ESEARCH");
    injector.injectCapability("CONDSTORE");
    injector.injectCapability("QRESYNC");
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (CONDSTORE)\r\n"));
    cServer("* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft Junk NonJunk $Forwarded)\r\n"
            "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft Junk NonJunk $Forwarded \\*)] Flags permitted.\r\n"
            "* 3 EXISTS\r\n"
            "* 0 RECENT\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [NOMODSEQ] .\r\n"
            );
    cServer(t.last("OK [READ-WRITE] selected\r\n"));
    cClient(t.mk("UID SEARCH RETURN (ALL) ALL\r\n"));
    cServer("* ESEARCH (TAG \"" + t.last() + "\") UID ALL 1:3\r\n");
    cServer(t.last("OK [HIGHESTMODSEQ 1] Searched\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (MODSEQ (1) UID 1 FLAGS (\\Seen))\r\n"
            "* 2 FETCH (MODSEQ (1) UID 2 FLAGS ())\r\n"
            "* 3 FETCH (MODSEQ (1) UID 3 FLAGS (\\Answered))\r\n"
            + t.last("OK flags fetched\r\n"));

    existsA = 3;
    uidNextA = 15;
    uidValidityA = 666;
    Imap::Mailbox::SyncState state;
    state.setExists(existsA);
    state.setUidNext(uidNextA);
    state.setUidValidity(uidValidityA);
    state.setRecent(0);
    state.setFlags(QString::fromUtf8("\\Answered \\Flagged \\Deleted \\Seen \\Draft Junk NonJunk $Forwarded").split(QLatin1Char(' ')));
    state.setPermanentFlags(QString::fromUtf8("\\Answered \\Flagged \\Deleted \\Seen \\Draft Junk NonJunk $Forwarded \\*").split(QLatin1Char(' ')));
    state.setHighestModSeq(1);
    uidMapA << 1 << 2 << 3;

    helperCheckCache();
    helperVerifyUidMapA();
    QCOMPARE(model->cache()->mailboxSyncState("a"), state);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMapA.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMapA);
    QCOMPARE(model->cache()->msgFlags("a", 1), QStringList() << QLatin1String("\\Seen"));
    QCOMPARE(model->cache()->msgFlags("a", 2), QStringList());
    QCOMPARE(model->cache()->msgFlags("a", 3), QStringList() << QLatin1String("\\Answered"));

    cEmpty();
    justKeepTask();
}

/** @short Bug #329204 -- spurious * ENABLED untagged responses from Kolab's IMAP servers */
void ImapModelObtainSynchronizedMailboxTest::testQresyncExtraEnabled()
{
    helperTestQresyncNoChanges(EXTRA_ENABLED);
}

TROJITA_HEADLESS_TEST( ImapModelObtainSynchronizedMailboxTest )
