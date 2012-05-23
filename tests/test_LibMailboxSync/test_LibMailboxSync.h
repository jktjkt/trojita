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

#ifndef TEST_IMAP_LIBMAILBOXSYNC
#define TEST_IMAP_LIBMAILBOXSYNC

#include "Imap/Model/Model.h"
#include "Streams/SocketFactory.h"
#include "TagGenerator.h"

class QSignalSpy;

class LibMailboxSync : public QObject
{
    Q_OBJECT
public:
    virtual ~LibMailboxSync();
protected slots:
    virtual void init();
    virtual void cleanup();
    virtual void cleanupTestCase();
    virtual void initTestCase();

    void modelSignalsError(const QString &message);
    void modelLogged(uint parserId, const Imap::Mailbox::LogMessage &message);

protected:
    virtual void helperSyncAWithMessagesEmptyState();
    virtual void helperSyncAFullSync();
    virtual void helperSyncANoMessagesCompleteState();
    virtual void helperSyncBNoMessages();
    virtual void helperSyncAWithMessagesNoArrivals();
    virtual void helperSyncFlags();
    virtual void helperSyncASomeNew( int number );

    virtual void helperFakeExistsUidValidityUidNext();
    virtual void helperFakeUidSearch( uint start=0 );
    virtual void helperVerifyUidMapA();
    virtual void helperCheckCache(bool ignoreUidNext=false);

    virtual void helperOneFlagUpdate( const QModelIndex &message );

    void helperInitialListing();

    Imap::Mailbox::Model* model;
    Imap::Mailbox::FakeSocketFactory* factory;
    Imap::Mailbox::TestingTaskFactory* taskFactoryUnsafe;
    QSignalSpy* errorSpy;

    QPersistentModelIndex idxA, idxB, msgListA, msgListB;
    TagGenerator t;
    uint existsA, uidValidityA, uidNextA;
    QList<uint> uidMapA;
    bool m_verbose;
};

#define SOCK static_cast<Imap::FakeSocket*>( factory->lastSocket() )

#define cServer(data) \
{ \
    SOCK->fakeReading(data); \
    for (int i=0; i<4; ++i) \
        QCoreApplication::processEvents(); \
}

#define cClient(data) \
{ \
    for (int i=0; i<4; ++i) \
        QCoreApplication::processEvents(); \
    QCOMPARE(QString::fromAscii(SOCK->writtenStuff()), QString::fromAscii(data));\
}

#define cEmpty() \
{ \
    for (int i=0; i<4; ++i) \
        QCoreApplication::processEvents(); \
    QCOMPARE(QString::fromAscii(SOCK->writtenStuff()), QString()); \
}


#endif
