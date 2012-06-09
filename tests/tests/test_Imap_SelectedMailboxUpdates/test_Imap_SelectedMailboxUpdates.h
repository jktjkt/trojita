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

#ifndef TEST_IMAP_SELECTEDMAILBOXUPDATES
#define TEST_IMAP_SELECTEDMAILBOXUPDATES

#include "test_LibMailboxSync/test_LibMailboxSync.h"

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
private:
    void helperTestExpungeImmediatelyAfterArrival(bool sendUidNext);
    void helperGenericTraffic(bool askForEnvelopes);
    void helperGenericTrafficFirstArrivals(bool askForEnvelopes);
    void helperGenericTrafficArrive2(bool askForEnvelopes);
    void helperGenericTrafficArrive3(bool askForEnvelopes);
    void helperGenericTrafficArrive4(bool askForEnvelopes);
    QByteArray helperCreateTrivialEnvelope(const uint seq, const uint uid, const QString &subject);
    void helperCheckSubjects(const QStringList &subjects);
    void helperDeleteOneMessage(const uint seq, const QStringList &remainingSubjects);
    void helperDeleteTwoMessages(const uint seq1, const uint seq2, const QStringList &remainingSubjects);
};

#endif
