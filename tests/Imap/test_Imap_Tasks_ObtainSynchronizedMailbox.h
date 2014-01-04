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

#ifndef TEST_IMAP_TASKS_OBTAINSYNCHRONIZEDMAILBOXTASK
#define TEST_IMAP_TASKS_OBTAINSYNCHRONIZEDMAILBOXTASK

#include "Utils/LibMailboxSync.h"

class QSignalSpy;

class ImapModelObtainSynchronizedMailboxTest : public LibMailboxSync
{
    Q_OBJECT

    typedef enum {WITHOUT_ESEARCH, WITH_ESEARCH} ESearchMode;
    void helperCacheArrivalRaceDuringUid(const ESearchMode esearch);
    void helperCacheExpunges(const ESearchMode esearch);

    typedef enum { JUST_QRESYNC, EXTRA_ENABLED } ModeForHelperTestQresyncNoChanges;
    void helperTestQresyncNoChanges(ModeForHelperTestQresyncNoChanges mode);
private slots:
    void init();
    void testSyncEmptyMinimal();
    void testSyncEmptyNormal();
    void testSyncWithMessages();
    void testSyncTwoLikeCyrus();
    void testSyncTwoInParallel();
    void testSyncNoUidnext();
    void testResyncNoArrivals();
    void testResyncOneNew();
    void testResyncUidValidity();
    void testDecreasedUidNext();
    void testReloadReadsFromCache();
    void testCacheNoChange();
    void testCacheUidValidity();
    void testCacheArrivals();
    void testCacheArrivalRaceDuringUid();
    void testCacheArrivalRaceDuringUid_ESearch();
    void testCacheArrivalRaceDuringUid2();
    void testCacheArrivalRaceDuringFlags();
    void testCacheExpunges();
    void testCacheExpunges_ESearch();
    void testCacheExpungesDuringUid();
    void testCacheExpungesDuringUid2();
    void testCacheExpungesDuringSelect();
    void testCacheExpungesDuringFlags();
    void testCacheArrivalsImmediatelyDeleted();
    void testCacheArrivalsOldDeleted();
    void testCacheArrivalsThenDynamic();
    void testCacheDeletionsThenDynamic();
    void testCondstoreNoChanges();
    void testCondstoreChangedFlags();
    void testCondstoreErrorExists();
    void testCondstoreErrorUidNext();
    void testCondstoreUidValidity();
    void testCondstoreDecreasedHighestModSeq();
    void testCacheDiscrepancyExistsUidsConstantHMS();
    void testCacheDiscrepancyExistsUidsDifferentHMS();
    void testCondstoreQresyncNomodseqHighestmodseq();

    void testQresyncNoChanges();
    void testQresyncChangedFlags();
    void testQresyncVanishedEarlier();
    void testQresyncUidValidity();
    void testQresyncNoModseqChangedFlags();
    void testQresyncErrorExists();
    void testQresyncErrorUidNext();
    void testQresyncUnreportedNewArrivals();
    void testQresyncReportedNewArrivals();
    void testQresyncDeletionsNewArrivals();
    void testQresyncSpuriousVanishedEarlier();
    void testQresyncAfterEmpty();
    void testQresyncExtraEnabled();

    void testSpuriousSearch();
    void testSpuriousESearch();

    void testOfflineOpening();

    void testQresyncEnabling();

    // We put the benchmark to the last position as this one takes a long time
    void testFlagReSyncBenchmark();

    void helperCacheDiscrepancyExistsUids(bool constantHighestModSeq);
};

#endif
