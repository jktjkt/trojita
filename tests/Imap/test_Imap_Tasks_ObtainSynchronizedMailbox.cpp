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
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QStringLiteral("a") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.flags(), QStringList() );
    // No messages, and hence we know that none of them could possibly be recent or unseen or whatever
    QCOMPARE( syncState.isUsableForNumbers(), true );
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

/** Verify syncing of a non-empty mailbox with just the EXISTS response

The interesting bits are what happens when the server cheats and returns just a very limited subset of mailbox metadata,
just the EXISTS response in particular.
*/
void ImapModelObtainSynchronizedMailboxTest::testSyncEmptyMinimalNonEmpty()
{
    model->setProperty("trojita-imap-noop-period", 10);

    // Ask the model to sync stuff
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));

    // Try to feed it with absolute minimum data
    cServer(QByteArray("* 1 exists\r\n") + t.last("OK done\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 666\r\n" + t.last("OK searched\r\n"));
    cClient(t.mk("FETCH 1 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    cEmpty();

    // Verify that we indeed received what we wanted
    QCOMPARE(model->rowCount(msgListA), 1);
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>(static_cast<Imap::Mailbox::TreeItem*>(msgListA.internalPointer()));
    Q_ASSERT(list);
    QVERIFY(list->fetched());
    QCoreApplication::processEvents();
    cEmpty();

    // Now, let's try to re-sync it once again; the difference is that our cache now has "something"
    model->resyncMailbox(idxA);
    QCoreApplication::processEvents();

    // Verify that it indeed caused a re-synchronization
    list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>(static_cast<Imap::Mailbox::TreeItem*>(msgListA.internalPointer()));
    Q_ASSERT(list);
    QVERIFY(list->loading());
    cClient(t.mk("SELECT a\r\n"));
    cServer(QByteArray("* 1 exists\r\n") + t.last("OK done\r\n"));
    // We still don't know anything else but EXISTS, so this is a full sync again
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 666\r\n" + t.last("OK searched\r\n"));
    cClient(t.mk("FETCH 1 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    cEmpty();

    // Check the cache
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState(QStringLiteral("a"));
    QCOMPARE(syncState.exists(), 1u);
    QCOMPARE(syncState.flags(), QStringList());
    // The flags are already synced, though
    QCOMPARE(syncState.isUsableForNumbers(), true);
    QCOMPARE(syncState.isUsableForSyncing(), false);
    QCOMPARE(syncState.permanentFlags(), QStringList());
    QCOMPARE(syncState.recent(), 0u);
    QCOMPARE(syncState.uidNext(), 667u);
    QCOMPARE(syncState.uidValidity(), 0u);
    QCOMPARE(syncState.unSeenCount(), 1u);
    QCOMPARE(syncState.unSeenOffset(), 0u);

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
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QStringLiteral("a") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.flags(), QStringList() << QLatin1String("\\Answered") <<
              QLatin1String("\\Flagged") << QLatin1String("\\Deleted") <<
              QLatin1String("\\Seen") << QLatin1String("\\Draft") );
    QCOMPARE( syncState.isUsableForNumbers(), true );
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
    syncState = model->cache()->mailboxSyncState( QStringLiteral("a") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.flags(), QStringList() );
    QCOMPARE( syncState.isUsableForNumbers(), true );
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
    uidMapA.pop_back();
    --uidNextA;

    // ...and resync again, this should scream loud, but not crash
    QCOMPARE( model->rowCount( msgListA ), 3 );
    model->switchToMailbox( idxA );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    helperSyncAFullSync();
}

/** @short Synchronization when the server doesn't report UIDNEXT at all */
void ImapModelObtainSynchronizedMailboxTest::helperMissingUidNext(const MessageNumberChange mode)
{
    Imap::Mailbox::SyncState oldState;
    oldState.setExists(3);
    oldState.setUidValidity(666);
    oldState.setUidNext(10); // because Trojita's code computes this during points in time the view is fully synced
    oldState.setUnSeenCount(0);
    oldState.setRecent(0);
    model->cache()->setMailboxSyncState(QStringLiteral("a"), oldState);
    model->cache()->setUidMapping(QStringLiteral("a"), Imap::Uids() << 4 << 6 << 7);

    QSignalSpy rowsInserted(model, SIGNAL(rowsInserted(QModelIndex,int,int)));
    QSignalSpy rowsRemoved(model, SIGNAL(rowsRemoved(QModelIndex,int,int)));

    model->switchToMailbox(idxA);
    cClient(t.mk("SELECT a\r\n"));
    // prepopulation from cache should be ignored
    QCOMPARE(rowsInserted.size(), 1);
    QCOMPARE(rowsInserted[0], QVariantList() << QModelIndex(msgListA) << 0 << 2);
    rowsInserted.clear();
    QVERIFY(rowsRemoved.isEmpty());
    switch (mode) {
    case MessageNumberChange::LESS:
        cServer("* 2 EXISTS\r\n");
        break;
    case MessageNumberChange::MORE:
        cServer("* 4 EXISTS\r\n");
        break;
    case MessageNumberChange::SAME:
        cServer("* 3 EXISTS\r\n");
        break;
    }
    cServer("* 0 RECENT\r\n"
            "* OK [UIDVALIDITY 666] UIDs valid\r\n"
            + t.last("OK selected\r\n"));
    QVERIFY(rowsRemoved.isEmpty());
    QVERIFY(rowsInserted.isEmpty());
    cClient(t.mk("UID SEARCH ALL\r\n"));
    // the actual UID data is garbage and not needed
    switch (mode) {
    case MessageNumberChange::LESS:
        cServer("* SEARCH 5 9\r\n");
        break;
    case MessageNumberChange::MORE:
        cServer("* SEARCH 5 8 7 9\r\n");
        break;
    case MessageNumberChange::SAME:
        cServer("* SEARCH 5 8 9\r\n");
        break;
    }
    cServer(t.last("OK search\r\n"));
    switch (mode) {
    case MessageNumberChange::LESS:
        cClient(t.mk("FETCH 1:2 (FLAGS)\r\n"));
        break;
    case MessageNumberChange::MORE:
        cClient(t.mk("FETCH 1:4 (FLAGS)\r\n"));
        break;
    case MessageNumberChange::SAME:
        cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
        break;
    }
    cServer("* 1 FETCH (FLAGS (\\Seen))\r\n"
            "* 2 FETCH (FLAGS (\\Seen))\r\n");
    switch (mode) {
    case MessageNumberChange::LESS:
        oldState.setExists(2);
        break;
    case MessageNumberChange::MORE:
        cServer("* 3 FETCH (FLAGS (\\Seen))\r\n"
                "* 4 FETCH (FLAGS (\\Seen))\r\n");
        oldState.setExists(4);
        break;
    case MessageNumberChange::SAME:
        cServer("* 3 FETCH (FLAGS (\\Seen))\r\n");
        break;
    }
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    justKeepTask();
    QCOMPARE(model->cache()->mailboxSyncState(QLatin1String("a")), oldState);
}

void ImapModelObtainSynchronizedMailboxTest::testMisingUidNextLess()
{
    helperMissingUidNext(MessageNumberChange::LESS);
}

void ImapModelObtainSynchronizedMailboxTest::testMisingUidNextMore()
{
    helperMissingUidNext(MessageNumberChange::MORE);
}

void ImapModelObtainSynchronizedMailboxTest::testMisingUidNextSame()
{
    helperMissingUidNext(MessageNumberChange::SAME);
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
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (\\SEEN x))\r\n"
            "* 2 FETCH (FLAGS (y))\r\n"
            "* 3 FETCH (FLAGS (\\seen z))\r\n");
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    sync.setUnSeenCount(1);
    sync.setRecent(0);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "\\Seen" << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "\\Seen" << "z");
    justKeepTask();
}

/** @short Test synchronization of a mailbox with on-disk cache being up-to-date, but no data in memory */
void ImapModelObtainSynchronizedMailboxTest::testCacheNoChange()
{
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
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
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;

    // Fill the cache with some values which shall make sense in the "previous state"
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    // Don't forget about the flags
    model->cache()->setMsgFlags(QStringLiteral("a"), 1, QStringList() << QStringLiteral("f1"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("f6"));
    // And even message metadata
    Imap::Mailbox::AbstractCache::MessageDataBundle bundle;
    bundle.envelope = Imap::Message::Envelope(QDateTime::currentDateTime(), QStringLiteral("subj"),
                                              QList<Imap::Message::MailAddress>(), QList<Imap::Message::MailAddress>(),
                                              QList<Imap::Message::MailAddress>(), QList<Imap::Message::MailAddress>(),
                                              QList<Imap::Message::MailAddress>(), QList<Imap::Message::MailAddress>(),
                                              QList<QByteArray>(), QByteArray());
    bundle.uid = 1;
    model->cache()->setMessageMetadata(QStringLiteral("a"), 1, bundle);
    bundle.uid = 6;
    model->cache()->setMessageMetadata(QStringLiteral("a"), 6, bundle);
    // And of course also message parts
    model->cache()->setMsgPart(QStringLiteral("a"), 1, "1", "blah");
    model->cache()->setMsgPart(QStringLiteral("a"), 6, "1", "blah");

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
    sync.setUnSeenCount(3);
    sync.setRecent(0);
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
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
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
    sync.setUnSeenCount(4);
    sync.setRecent(0);
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
        injector.injectCapability(QStringLiteral("ESEARCH"));
    }
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
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
    sync.setUnSeenCount(5);
    sync.setRecent(0);
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
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
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
    sync.setUnSeenCount(5);
    sync.setRecent(0);
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
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
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
    sync.setUnSeenCount(5);
    sync.setRecent(0);
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
        injector.injectCapability(QStringLiteral("ESEARCH"));
    }
    Imap::Mailbox::SyncState sync;
    sync.setExists(6);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10 << 11 << 12 << 14;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("foo"));
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
    uidMap.remove(1);
    sync.setExists(5);
    sync.setUnSeenCount(5);
    sync.setRecent(0);
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
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10 << 11 << 12 << 14;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("foo"));
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 5 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* 4 EXPUNGE\r\n* SEARCH 6 10 11 14\r\n");
    cServer(t.last("OK uids\r\n"));
    uidMap.remove(1);
    uidMap.remove(3);
    sync.setExists(4);
    sync.setUnSeenCount(4);
    sync.setRecent(0);
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
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10 << 11 << 12 << 14;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("foo"));
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 5 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 6 10 11 12 14\r\n* 4 EXPUNGE\r\n");
    cServer(t.last("OK uids\r\n"));
    uidMap.remove(1);
    uidMap.remove(3);
    sync.setExists(4);
    sync.setUnSeenCount(4);
    sync.setRecent(0);
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
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10 << 11 << 12 << 14;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("foo"));
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
    uidMap.remove(1);
    uidMap.remove(3);
    sync.setExists(4);
    cClient(t.mk("FETCH 1:4 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (z))\r\n"
            "* 3 FETCH (FLAGS (a))\r\n"
            "* 4 FETCH (FLAGS (c))\r\n"
            );
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    sync.setUnSeenCount(4);
    sync.setRecent(0);
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
    sync.setUnSeenCount(6);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10 << 11 << 12 << 14;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("foo"));
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 5 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 6 10 11 12 14\r\n");
    cServer(t.last("OK uids\r\n"));
    uidMap.remove(1);
    sync.setExists(5);
    cClient(t.mk("FETCH 1:5 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (x))\r\n"
            "* 2 FETCH (FLAGS (z))\r\n"
            "* 4 EXPUNGE\r\n"
            "* 3 FETCH (FLAGS (a))\r\n"
            "* 4 FETCH (FLAGS (c))\r\n"
            );
    cServer(t.last("OK fetch\r\n"));
    uidMap.remove(3);
    sync.setExists(4);
    sync.setUnSeenCount(4);
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
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
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
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
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
    uidMap.remove(2);
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
    Imap::Uids uidMap;
    for (uint i = 1; i <= sync.exists(); ++i)
        uidMap << i;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
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
    uidMap.remove(9);
    // the second "10 EXPUNGE" is not reflected here at all, as it's for the new arrivals
    // This one is for the "3 EXPUNGE"
    uidMap.remove(2);
    uidMap << 102 << 103 << 104 << 105 << 106;
    sync.setUidNext(107);
    sync.setExists(13);
    cClient(t.mk("FETCH 1:13 (FLAGS)\r\n"));
    cServer("* 2 EXPUNGE\r\n");
    uidMap.remove(1);
    cServer("* 12 EXPUNGE\r\n");
    uidMap.remove(11);
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
    sync.setUnSeenCount(12);
    sync.setRecent(0);
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
    Imap::Uids uidMap;
    for (uint i = 1; i <= sync.exists(); ++i)
        uidMap << i;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
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
    uidMap.remove(1);
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
    sync.setUnSeenCount(7);
    sync.setRecent(0);
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
    injector.injectCapability(QStringLiteral("CONDSTORE"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
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
    injector.injectCapability(QStringLiteral("CONDSTORE"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (CONDSTORE)\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 666] .\r\n"
            );
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS) (CHANGEDSINCE 33)\r\n"));
    cServer("* 3 FETCH (FLAGS (f101 \\seen))\r\n");
    cServer(t.last("OK fetched\r\n"));
    cEmpty();
    sync.setHighestModSeq(666);
    sync.setUnSeenCount(2);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "\\Seen" << "f101");
    justKeepTask();
}

/** @short Test constant HIGHESTMODSEQ and more EXISTS */
void ImapModelObtainSynchronizedMailboxTest::testCondstoreErrorExists()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("CONDSTORE"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
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
    sync.setUnSeenCount(4);
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
    injector.injectCapability(QStringLiteral("CONDSTORE"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
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
            "* 3 FETCH (FLAGS (z \\Recent))\r\n");
    cServer(t.last("OK fetch\r\n"));
    cEmpty();
    sync.setUidNext(16);
    sync.setRecent(1);
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "y");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "\\Recent" << "z");
    justKeepTask();
}

/** @short Changed UIDVALIDITY shall always lead to full sync, no matter what HIGHESTMODSEQ says */
void ImapModelObtainSynchronizedMailboxTest::testCondstoreUidValidity()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("CONDSTORE"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
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
    injector.injectCapability(QStringLiteral("CONDSTORE"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
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
    injector.injectCapability(QStringLiteral("QRESYNC"));

    uidMapA << 5 << 6;
    existsA = 2;
    uidNextA = 10;
    uidValidityA = 123;

    helperSyncAWithMessagesEmptyState();
    helperCheckCache();

    helperSyncBNoMessages();

    injector.injectCapability(QStringLiteral("ESEARCH"));
    model->switchToMailbox(idxA);

    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(10);
    sync.setHighestModSeq(111);
    Imap::Uids uidMap;
    uidMap << 5 << 6 << 7 << 8;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 111 (3 7)))\r\n"));
    cServer("* OK [CLOSED] Previous mailbox closed\r\n"
            "* 3 EXISTS\r\n"
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
    injector.injectCapability(QStringLiteral("QRESYNC"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
    switch (mode) {
    case JUST_QRESYNC:
        // do nothing
        break;
    case EXTRA_ENABLED:
        cServer("* ENABLED CONDSTORE QRESYNC\r\n");
        break;
    case EXTRA_ENABLED_EMPTY:
        cServer("* ENABLED\r\n");
        break;
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
    injector.injectCapability(QStringLiteral("QRESYNC"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
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
    sync.setUnSeenCount(2);
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
    injector.injectCapability(QStringLiteral("QRESYNC"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
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
    sync.setUnSeenCount(2);
    uidMap.remove(uidMap.indexOf(9));
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
    injector.injectCapability(QStringLiteral("QRESYNC"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
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
    sync.setUnSeenCount(2);
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
    injector.injectCapability(QStringLiteral("QRESYNC"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
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
    injector.injectCapability(QStringLiteral("QRESYNC"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
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
            "* 2 FETCH (FLAGS (\\seen x2))\r\n"
            "* 3 FETCH (FLAGS (x3 \\seen))\r\n"
            "* 4 FETCH (FLAGS (x4))\r\n"
            + t.last("OK flags\r\n"));
    cEmpty();
    sync.setExists(4);
    sync.setUnSeenCount(2);
    sync.setHighestModSeq(0);
    uidMap << 12;
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 6), QStringList() << "x1");
    QCOMPARE(model->cache()->msgFlags("a", 9), QStringList() << "\\Seen" << "x2");
    QCOMPARE(model->cache()->msgFlags("a", 10), QStringList() << "\\Seen" << "x3");
    QCOMPARE(model->cache()->msgFlags("a", 12), QStringList() << "x4");
    QCOMPARE(model->cache()->msgFlags("a", 15), QStringList());
    justKeepTask();
}

/** @short Test that UIDNEXT change with unchanged HIGHESTMODSEQ cancels QRESYNC */
void ImapModelObtainSynchronizedMailboxTest::testQresyncErrorUidNext()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("QRESYNC"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
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
    injector.injectCapability(QStringLiteral("QRESYNC"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
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
    cServer("* 4 FETCH (FLAGS (x4 \\seen) UID 16)\r\n" + t.last("OK uid fetch flags\r\n"));
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
    QCOMPARE(model->cache()->msgFlags("a", 16), QStringList() << "\\Seen" << "x4");
    justKeepTask();
}

/** @short Test QRESYNC when something has arrived and the server reported UID of that stuff on its own */
void ImapModelObtainSynchronizedMailboxTest::testQresyncReportedNewArrivals()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("QRESYNC"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 6 << 9 << 10;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList() << QStringLiteral("x"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList() << QStringLiteral("y"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList() << QStringLiteral("z"));
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
    sync.setUnSeenCount(4);
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
    injector.injectCapability(QStringLiteral("QRESYNC"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(5);
    sync.setUidValidity(666);
    sync.setUidNext(6);
    sync.setHighestModSeq(10);
    sync.setUnSeenCount(5);
    sync.setRecent(0);
    Imap::Uids uidMap;
    uidMap << 1 << 2 << 3 << 4 << 5;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    model->cache()->setMsgFlags(QStringLiteral("a"), 1, QStringList() << QStringLiteral("1"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 2, QStringList() << QStringLiteral("2"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 3, QStringList() << QStringLiteral("3"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 4, QStringList() << QStringLiteral("4"));
    model->cache()->setMsgFlags(QStringLiteral("a"), 5, QStringList() << QStringLiteral("5"));
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
    uidMap.remove(uidMap.indexOf(1));
    uidMap.remove(uidMap.indexOf(2));
    uidMap.remove(uidMap.indexOf(3));
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
            "* 2 FETCH (FLAGS (uid1214 \\seen))\r\n"
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
    // unseen count is computed, too
    sync.setUnSeenCount(2);
    Imap::Uids uidMap;
    uidMap << 1212 << 1214 << 1215;

    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);
    QCOMPARE(model->cache()->msgFlags("a", 1212), QStringList() << "uid1212");
    QCOMPARE(model->cache()->msgFlags("a", 1214), QStringList() << "\\Seen" << "uid1214");
    QCOMPARE(model->cache()->msgFlags("a", 1215), QStringList() << "uid1215");

    // Switch away from this mailbox
    helperSyncBNoMessages();

    // Make sure that we catch UIDNEXT missing by purging the cache
    model->cache()->setMsgPart(QStringLiteral("a"), 1212, QByteArray(), "foo");

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
    // At this point, there's no need for nuking the cache (yet).
    QCOMPARE(model->cache()->messagePart("a", 1212, QByteArray()), QByteArray("foo"));
    // The UIDNEXT is missing -> resyncing the UIDs again
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 1212 1214 1215\r\n");
    cServer(t.last("OK search\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS (uid1212))\r\n"
            "* 2 FETCH (FLAGS (\\sEEN uid1214))\r\n"
            "* 3 FETCH (FLAGS (uid1215))\r\n"
            + t.last("OK fetch\r\n"));
    cEmpty();
    justKeepTask();

    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMap.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMap);

    // While a missing UIDNEXT is an evil thing to do, the actual language in the RFC leaves some unfortunate
    // leeway in its interpretation. As much as I would love to throw away everything, Courier's users would
    // not be thrilled by that, so the UIDNEXT actually has to be preserved in this context.
    //
    // FIXME: It would be interesting to perturb the UIDs and detect *that*. However, it's outside of scope for now.
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
    Imap::Uids uidMap;
    uidMap << 10 << 20 << 30;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
    Imap::Mailbox::AbstractCache::MessageDataBundle msg10, msg20;
    msg10.uid = 10;
    msg10.envelope.subject = QLatin1String("msg10");
    msg20.uid = 20;
    msg20.envelope.subject = QLatin1String("msg20");

    // Prepare the body structure for this message
    int start = 0;
    Imap::Responses::Fetch fetchResponse(666, QByteArray(" (BODYSTRUCTURE (\"text\" \"plain\" (\"chaRset\" \"UTF-8\" "
                                                         "\"format\" \"flowed\") NIL NIL \"8bit\" 362 15 NIL NIL NIL))\r\n"),
                                         start);
    msg10.serializedBodyStructure = dynamic_cast<const Imap::Responses::RespData<QByteArray>&>(*(fetchResponse.data["x-trojita-bodystructure"])).data;
    msg20.serializedBodyStructure = msg10.serializedBodyStructure;

    model->cache()->setMessageMetadata(QStringLiteral("a"), 10, msg10);
    model->cache()->setMessageMetadata(QStringLiteral("a"), 20, msg20);

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
    model->setImapUser(QStringLiteral("user"));
    model->setImapPassword(QStringLiteral("pw"));
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
    injector.injectCapability(QStringLiteral("ESEARCH"));
    injector.injectCapability(QStringLiteral("QRESYNC"));
    Imap::Mailbox::SyncState sync;
    sync.setExists(0);
    sync.setUidValidity(1309542826);
    sync.setUidNext(252);
    sync.setHighestModSeq(10);
    // and just for fun: introduce garbage to the cache, muhehe
    sync.setUnSeenCount(10);
    sync.setRecent(10);
    Imap::Uids uidMap;
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), uidMap);
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
    sync.setUnSeenCount(0);
    sync.setRecent(0);
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
    injector.injectCapability(QStringLiteral("ESEARCH"));
    injector.injectCapability(QStringLiteral("QRESYNC"));
    Imap::Mailbox::SyncState sync;
    sync.setUidValidity(1336686200);
    sync.setExists(0);
    sync.setUidNext(1);
    sync.setHighestModSeq(1);
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), Imap::Uids());
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
    injector.injectCapability(QStringLiteral("ESEARCH"));
    injector.injectCapability(QStringLiteral("CONDSTORE"));
    injector.injectCapability(QStringLiteral("QRESYNC"));
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
    state.setUnSeenCount(2);
    state.setFlags(QStringLiteral("\\Answered \\Flagged \\Deleted \\Seen \\Draft Junk NonJunk $Forwarded").split(QLatin1Char(' ')));
    state.setPermanentFlags(QStringLiteral("\\Answered \\Flagged \\Deleted \\Seen \\Draft Junk NonJunk $Forwarded \\*").split(QLatin1Char(' ')));
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

/** @short Bug #350006 -- spurious and empty * ENABLED untagged responses from Cyrus on mailbox switchover */
void ImapModelObtainSynchronizedMailboxTest::testQresyncExtraEnabledEmptySwitchover()
{
    helperTestQresyncNoChanges(EXTRA_ENABLED_EMPTY);
}

/** @short Check that we can recover when a SELECT ends up in a BAD or NO response */
void ImapModelObtainSynchronizedMailboxTest::testSelectRetryNoBad()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("ESEARCH"));
    injector.injectCapability(QStringLiteral("CONDSTORE"));
    injector.injectCapability(QStringLiteral("QRESYNC"));

    // Our first attempt fails with a NO response
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (CONDSTORE)\r\n"));
    cServer(t.last("NO go away\r\n"));
    cEmpty();
    checkNoTasks();

    // The server is even more creative now and returns a tagged BAD for increased fun factor
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (CONDSTORE)\r\n"));
    cServer(t.last("BAD pwned\r\n"));
    cEmpty();
    checkNoTasks();

    // But we're very persistent and never give up
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (CONDSTORE)\r\n"));
    cServer("* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft Junk NonJunk $Forwarded)\r\n"
            "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft Junk NonJunk $Forwarded \\*)] Flags permitted.\r\n"
            "* 0 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 333] .\r\n"
            + t.last("OK [READ-WRITE] feels better, doesn't it\r\n"));
    cEmpty();
    justKeepTask();

    // Now this is strange -- reselecting fails.
    // The whole point why we're doing this is to test failed-A -> B transtitions.
    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 333))\r\n"));
    cServer(t.last("BAD pwned\r\n"));
    cEmpty();
    checkNoTasks();

    // Let's see if we will have any luck with the other mailbox
    model->resyncMailbox(idxB);
    cClient(t.mk("SELECT b (CONDSTORE)\r\n"));
    cServer("* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft Junk NonJunk $Forwarded)\r\n"
            "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft Junk NonJunk $Forwarded \\*)] Flags permitted.\r\n"
            "* 0 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 333] .\r\n"
            + t.last("OK [READ-WRITE] feels better, doesn't it\r\n"));
    cEmpty();
    justKeepTask();
}

/** @short Check that mailbox switchover which never completes and subsequent model destruction does not lead to segfault

https://bugs.kde.org/show_bug.cgi?id=336090
*/
void ImapModelObtainSynchronizedMailboxTest::testDanglingSelect()
{
    helperTestQresyncNoChanges(JUST_QRESYNC);
    model->resyncMailbox(idxB);
    cClient(t.mk("SELECT b\r\n"));
    cEmpty();
}

/** @short Test that we can detect broken servers which do not issue an untagged OK with the [CLOSED] response code */
void ImapModelObtainSynchronizedMailboxTest::testQresyncNoClosed()
{
    helperTestQresyncNoChanges(JUST_QRESYNC);
    model->resyncMailbox(idxB);
    cClient(t.mk("SELECT b\r\n"));
    {
        // The IMAP code should complain about a lack of the [CLOSED] response code
        ExpectSingleErrorHere blocker(this);
        cServer(t.last("OK selected\r\n"));
    }
}

/** @short Check that everything gets delivered to the older mailbox in absence of QRESYNC */
void ImapModelObtainSynchronizedMailboxTest::testNoQresyncOutOfBounds()
{
    existsA = 3;
    uidValidityA = 6;
    uidMapA << 1 << 7 << 9;
    uidNextA = 16;
    helperSyncAWithMessagesEmptyState();
    model->resyncMailbox(idxB);
    QCOMPARE(msgListA.model()->rowCount(msgListA), 3);
    cClient(t.mk("SELECT b\r\n"));
    {
        // This refers to message #4 in a mailbox which only has three messages, hence a failure.
        ExpectSingleErrorHere blocker(this);
        cServer("* 4 FETCH (UID 666 FLAGS())\r\n")
    }
}

/** @short Check that the responses are consumed by the older mailbox */
void ImapModelObtainSynchronizedMailboxTest::testQresyncClosedHandover()
{
    Imap::Mailbox::SyncState sync;
    helperQresyncAInitial(sync);
    QStringList okFlags = QStringList() << QStringLiteral("z");
    QCOMPARE(model->cache()->msgFlags("a", 10), okFlags);

    // OK, we're done. Now the actual test -- open another mailbox
    model->resyncMailbox(idxB);
    cClient(t.mk("SELECT b\r\n"));
    // this one should be eaten, but ignored
    cServer("* 3 FETCH (FLAGS ())\r\n");
    QCOMPARE(msgListA.child(2, 0).data(Imap::Mailbox::RoleMessageFlags).toStringList(), okFlags);
    QCOMPARE(model->cache()->msgFlags("a", 10), okFlags);
    cServer("* 4 EXISTS\r\n");
    cServer("* 4 FETCH (UID 333666333 FLAGS (PWNED))\r\n");
    cEmpty();
    // "4" shouldn't be in there either, of course
    QCOMPARE(model->cache()->mailboxSyncState("a"), sync);
    QCOMPARE(model->rowCount(msgListA), 3);
    cEmpty();
    cServer("* OK [CLOSED] Previous mailbox closed\r\n"
            "* 0 EXISTS\r\n"
            + t.last("OK selected\r\n"));
    justKeepTask();
    cEmpty();
}

/** @short In absence of QRESYNC, all responses are delivered directly to the new ObtainSynchronizedMailboxTask */
void ImapModelObtainSynchronizedMailboxTest::testNoClosedRouting()
{
    existsA = 3;
    uidValidityA = 6;
    uidMapA << 1 << 7 << 9;
    uidNextA = 16;
    helperSyncAWithMessagesEmptyState();
    model->resyncMailbox(idxB);
    cClient(t.mk("SELECT b\r\n"));
    cServer("* 1 EXISTS\r\n" + t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 123\r\n" + t.last("OK uids\r\n"));
    cClient(t.mk("FETCH 1 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS ())\r\n" + t.last("OK flags\r\n"));
    cEmpty();
    justKeepTask();
}

#define REINIT_INDEXES_AFTER_LIST_CYCLE \
    model->rowCount(QModelIndex()); \
    QCoreApplication::processEvents(); \
    QCoreApplication::processEvents(); \
    QCOMPARE(model->rowCount(QModelIndex()), 26); \
    idxA = model->index(1, 0, QModelIndex()); \
    idxB = model->index(2, 0, QModelIndex()); \
    QCOMPARE(model->data(idxA, Qt::DisplayRole), QVariant(QLatin1String("a"))); \
    QCOMPARE(model->data(idxB, Qt::DisplayRole), QVariant(QLatin1String("b"))); \
    msgListA = model->index(0, 0, idxA); \
    msgListB = model->index(0, 0, idxB);

/** @short Check that an UNSELECT resets that flag which expects a [CLOSED] */
void ImapModelObtainSynchronizedMailboxTest::testUnselectClosed()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("UNSELECT"));
    Imap::Mailbox::SyncState sync;
    helperQresyncAInitial(sync);

    model->reloadMailboxList();
    REINIT_INDEXES_AFTER_LIST_CYCLE
    cClient(t.mk("UNSELECT\r\n"));
    cServer(t.last("OK unselected\r\n"));
    cEmpty();

    model->resyncMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 33] .\r\n"
            );
    cServer(t.last("OK selected\r\n"));

    justKeepTask();
    cEmpty();
}

/** @short Similar to testUnselectClosed, but mailbox invalidation happens during the initial sync */
void ImapModelObtainSynchronizedMailboxTest::testUnselectClosedDuringSelecting()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("UNSELECT"));
    injector.injectCapability(QStringLiteral("QRESYNC"));
    Imap::Mailbox::SyncState sync;
    helperQresyncAInitial(sync);

    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_OFFLINE);
    cClient(t.mk("LOGOUT\r\n"));
    cServer(t.last("OK loggedeout\r\n") + "* BYE see ya\r\n");

    taskFactoryUnsafe->fakeListChildMailboxes = false;
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    t.reset();
    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    model->switchToMailbox(idxA);
    injector.injectCapability(QStringLiteral("UNSELECT"));
    injector.injectCapability(QStringLiteral("QRESYNC"));
    cServer("* LIST () \".\" a\r\n" + t.last("OK listed\r\n"));
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));

    // This simulates the first sync after a reconnect.
    // There's no [CLOSED] because it's a new connection.
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 33] .\r\n"
            + t.last("OK selected\r\n"));
    cClient(t.mk("UNSELECT\r\n"));
    cServer(t.last("OK unselected\r\n"));
    cEmpty();

    idxA = model->index(1, 0, QModelIndex());
    QCOMPARE(idxA.data(Imap::Mailbox::RoleMailboxName), QVariant(QLatin1String("a")));

    // Now perform a sync after the failed one.
    // There will again be no [CLOSED], bug Trojita had a bug and expected to see a [CLOSED].
    model->switchToMailbox(idxA);
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 33] .\r\n"
            + t.last("OK selected\r\n"));

    justKeepTask();
    cEmpty();
}

/** @short Servers speaking about UID 0 are buggy, full stop */
void ImapModelObtainSynchronizedMailboxTest::testUid0()
{
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 1 EXISTS\r\n");
    {
        ExpectSingleErrorHere blocker(this);
        cServer("* 1 FETCH (UID 0)\r\n");
    }
}

QTEST_GUILESS_MAIN( ImapModelObtainSynchronizedMailboxTest )
