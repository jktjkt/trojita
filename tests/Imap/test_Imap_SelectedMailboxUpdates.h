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

#ifndef TEST_IMAP_SELECTEDMAILBOXUPDATES
#define TEST_IMAP_SELECTEDMAILBOXUPDATES

#include "Utils/LibMailboxSync.h"

class QSignalSpy;

class ImapModelSelectedMailboxUpdatesTest : public LibMailboxSync
{
    Q_OBJECT
private slots:

    void testExpungeImmediatelyAfterArrival();
    void testExpungeImmediatelyAfterArrivalWithUidNext();
    void testUnsolicitedFetch();
    void testGenericTraffic();
    void testGenericTrafficWithEnvelopes();
    void testVanishedUpdates();
    void testVanishedWithNonExisting();
    void testMultipleArrivals();
    void testMultipleArrivalsBlockingFurtherActivity();
    void testInnocentUidValidityChange();
    void testUnexpectedUidValidityChange();
    void testHighestModseqFlags();
    void testFetchAndConcurrentArrival();
private:
    void helperTestExpungeImmediatelyAfterArrival(bool sendUidNext);
    void helperGenericTraffic(bool askForEnvelopes);
    void helperGenericTrafficFirstArrivals(bool askForEnvelopes);
    void helperGenericTrafficArrive2(bool askForEnvelopes);
    void helperGenericTrafficArrive3(bool askForEnvelopes);
    void helperGenericTrafficArrive4(bool askForEnvelopes);
    void helperCheckSubjects(const QStringList &subjects);
    void helperDeleteOneMessage(const uint seq, const QStringList &remainingSubjects);
    void helperDeleteTwoMessages(const uint seq1, const uint seq2, const QStringList &remainingSubjects);
};

#endif
