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

#include <QTest>
#include "LibMailboxSync.h"
#include "headless_test.h"
#include "Common/MetaTypes.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/Cache.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Imap/Tasks/KeepMailboxOpenTask.h"

ExpectSingleErrorHere::ExpectSingleErrorHere(LibMailboxSync *syncer):
    m_syncer(syncer)
{
    m_syncer->m_expectsError = true;
}

ExpectSingleErrorHere::~ExpectSingleErrorHere()
{
    if (m_syncer->m_expectsError) {
        QFAIL("Expected an error from the IMAP model, but there wasn't one");
    }
    QCOMPARE(m_syncer->errorSpy->size(), 1);
    m_syncer->errorSpy->removeFirst();
}

LibMailboxSync::~LibMailboxSync()
{
}

void LibMailboxSync::init()
{
    m_verbose = qgetenv("TROJITA_IMAP_DEBUG") == QByteArray("1");
    m_expectsError = false;
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache(this);
    factory = new Streams::FakeSocketFactory(Imap::CONN_STATE_AUTHENTICATED);
    Imap::Mailbox::TaskFactoryPtr taskFactory(new Imap::Mailbox::TestingTaskFactory());
    taskFactoryUnsafe = static_cast<Imap::Mailbox::TestingTaskFactory*>(taskFactory.get());
    taskFactoryUnsafe->fakeOpenConnectionTask = true;
    taskFactoryUnsafe->fakeListChildMailboxes = true;
    taskFactoryUnsafe->fakeListChildMailboxesMap[ QLatin1String("") ] = QStringList() <<
        QLatin1String("a") << QLatin1String("b") << QLatin1String("c") <<
        QLatin1String("d") << QLatin1String("e") << QLatin1String("f") <<
        QLatin1String("g") << QLatin1String("h") << QLatin1String("i") <<
        QLatin1String("j") << QLatin1String("k") << QLatin1String("l") <<
        QLatin1String("m") << QLatin1String("n") << QLatin1String("o") <<
        QLatin1String("p") << QLatin1String("q") << QLatin1String("r") <<
        QLatin1String("s") << QLatin1String("q") << QLatin1String("u") <<
        QLatin1String("v") << QLatin1String("w") << QLatin1String("x") <<
        QLatin1String("y") << QLatin1String("z");
    model = new Imap::Mailbox::Model(this, cache, Imap::Mailbox::SocketFactoryPtr(factory), std::move(taskFactory));
    errorSpy = new QSignalSpy(model, SIGNAL(connectionError(QString)));
    connect(model, SIGNAL(connectionError(QString)), this, SLOT(modelSignalsError(QString)));
    connect(model, SIGNAL(logged(uint,Common::LogMessage)), this, SLOT(modelLogged(uint,Common::LogMessage)));

    msgListModel = new Imap::Mailbox::MsgListModel(this, model);

    threadingModel = new Imap::Mailbox::ThreadingMsgListModel(this);
    threadingModel->setSourceModel(msgListModel);

    QCoreApplication::processEvents();

    helperInitialListing();
}

void LibMailboxSync::modelSignalsError(const QString &message)
{
    if (m_expectsError) {
        m_expectsError = false;
    } else {
        qDebug() << message;
        QFAIL("Model emits an error");
    }
}

void LibMailboxSync::modelLogged(uint parserId, const Common::LogMessage &message)
{
    if (!m_verbose)
        return;

    qDebug() << "LOG" << parserId << message.source <<
                (message.message.endsWith(QLatin1String("\r\n")) ?
                     message.message.left(message.message.size() - 2) : message.message);
}

void LibMailboxSync::helperInitialListing()
{
    model->setNetworkPolicy(Imap::Mailbox::NETWORK_ONLINE);
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 26 );
    idxA = model->index( 1, 0, QModelIndex() );
    idxB = model->index( 2, 0, QModelIndex() );
    idxC = model->index( 3, 0, QModelIndex() );
    QCOMPARE( model->data( idxA, Qt::DisplayRole ), QVariant(QLatin1String("a")) );
    QCOMPARE( model->data( idxB, Qt::DisplayRole ), QVariant(QLatin1String("b")) );
    QCOMPARE( model->data( idxC, Qt::DisplayRole ), QVariant(QLatin1String("c")) );
    msgListA = model->index( 0, 0, idxA );
    msgListB = model->index( 0, 0, idxB );
    msgListC = model->index( 0, 0, idxC );
    cEmpty();
    t.reset();
    existsA = 0;
    uidNextA = 0;
    uidValidityA = 0;
    uidMapA.clear();
}

void LibMailboxSync::cleanup()
{
    delete model;
    model = 0;
    msgListModel->deleteLater();
    msgListModel = 0;
    threadingModel->deleteLater();
    threadingModel = 0;
    taskFactoryUnsafe = 0;
    QVERIFY( errorSpy->isEmpty() );
    delete errorSpy;
    errorSpy = 0;
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
}

void LibMailboxSync::cleanupTestCase()
{
}

void LibMailboxSync::initTestCase()
{
    Common::registerMetaTypes();
    model = 0;
    msgListModel = 0;
    threadingModel = 0;
    errorSpy = 0;
}

/** @short Helper: simulate sync of mailbox A that contains some messages from an empty state */
void LibMailboxSync::helperSyncAWithMessagesEmptyState()
{
    // Ask the model to sync stuff
    QCOMPARE( model->rowCount( msgListA ), 0 );
    helperSyncAFullSync();
}

/** @short Helper: perform a full sync of the mailbox A */
void LibMailboxSync::helperSyncAFullSync()
{
    cClient(t.mk("SELECT a\r\n"));

    helperFakeExistsUidValidityUidNext();

    // Verify that we indeed received what we wanted
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( ! list->fetched() );

    // The messages are added immediately, even when their UID is not known yet
    QCOMPARE(static_cast<int>(list->childrenCount(model)), static_cast<int>(existsA));

    helperFakeUidSearch();

    QCOMPARE( model->rowCount( msgListA ), static_cast<int>( existsA ) );
    QVERIFY( errorSpy->isEmpty() );

    helperSyncFlags();

    // No errors
    if ( ! errorSpy->isEmpty() )
        qDebug() << errorSpy->first();
    QVERIFY( errorSpy->isEmpty() );

    helperCheckCache();

    QVERIFY( list->fetched() );

    // and the first mailbox is fully synced now.
}

void LibMailboxSync::helperFakeExistsUidValidityUidNext()
{
    // Try to feed it with absolute minimum data
    QByteArray buf;
    QTextStream ss( &buf );
    ss << "* " << existsA << " EXISTS\r\n";
    ss << "* OK [UIDVALIDITY " << uidValidityA << "] UIDs valid\r\n";
    ss << "* OK [UIDNEXT " << uidNextA << "] Predicted next UID\r\n";
    ss.flush();
    cServer(buf + t.last("OK [READ-WRITE] Select completed.\r\n"));
}

void LibMailboxSync::helperFakeUidSearch( uint start )
{
    Q_ASSERT( start < existsA );
    if ( start == 0  ) {
        cClient(t.mk("UID SEARCH ALL\r\n"));
    } else {
        cClient(t.mk("UID SEARCH UID ") + QString::number( uidMapA[ start ] ).toUtf8() + QByteArray(":*\r\n"));
    }

    QByteArray buf;
    QTextStream ss( &buf );
    ss << "* SEARCH";

    // Try to be nasty and confuse the model with out-of-order UIDs
    QList<uint> shuffledMap = uidMapA;
    if ( shuffledMap.size() > 2 )
        qSwap(shuffledMap[0], shuffledMap[2]);

    for ( uint i = start; i < existsA; ++i ) {
        ss << " " << shuffledMap[i];
    }
    ss << "\r\n";
    ss.flush();
    cServer(buf + t.last("OK search\r\n"));
}

/** @short Helper: make the parser switch to mailbox B which is actually empty

This function is useful for making sure that the parser switches to another mailbox and will perform
a fresh SELECT when it goes back to the original mailbox.
*/
void LibMailboxSync::helperSyncBNoMessages()
{
    // Try to go to second mailbox
    QCOMPARE( model->rowCount( msgListB ), 0 );
    model->switchToMailbox( idxB );
    cClient(t.mk("SELECT b\r\n"));
    cServer(QByteArray("* 0 exists\r\n") + t.last("ok completed\r\n"));

    // Check the cache
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QLatin1String("b") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.isUsableForSyncing(), false );
    QCOMPARE( syncState.uidNext(), 0u );
    QCOMPARE( syncState.uidValidity(), 0u );

    // Verify that we indeed received what we wanted
    Q_ASSERT(msgListB.isValid());
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListB.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->fetched() );

    cEmpty();
    QVERIFY( errorSpy->isEmpty() );
}

/** @short Helper: synchronization of an empty mailbox A

Unlike helperSyncBNoMessages(), this function actually performs the sync with all required
responses like UIDVALIDITY and UIDNEXT.

It is the caller's responsibility to provide reasonable values for uidNextA and uidValidityA.

@see helperSyncBNoMessages()
*/
void LibMailboxSync::helperSyncANoMessagesCompleteState()
{
    QCOMPARE( model->rowCount( msgListA ), 0 );
    model->switchToMailbox( idxA );
    cClient(t.mk("SELECT a\r\n"));
    cServer(QString::fromUtf8("* 0 exists\r\n* OK [uidnext %1] foo\r\n* ok [uidvalidity %2] bar\r\n"
                                          ).arg(QString::number(uidNextA), QString::number(uidValidityA)).toUtf8()
                                  + t.last("ok completed\r\n"));

    // Check the cache
    Imap::Mailbox::SyncState syncState = model->cache()->mailboxSyncState( QLatin1String("a") );
    QCOMPARE( syncState.exists(), 0u );
    QCOMPARE( syncState.isUsableForSyncing(), true );
    QCOMPARE( syncState.uidNext(), uidNextA );
    QCOMPARE( syncState.uidValidity(), uidValidityA );

    existsA = 0;
    uidMapA.clear();
    helperCheckCache();
    helperVerifyUidMapA();

    // Verify that we indeed received what we wanted
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->fetched() );

    cEmpty();
    QVERIFY( errorSpy->isEmpty() );
}


/** @short Simulates what happens when mailbox A gets opened again, assuming that nothing has changed since the last time etc */
void LibMailboxSync::helperSyncAWithMessagesNoArrivals()
{
    // assume we've got some messages from the last case
    QCOMPARE( model->rowCount( msgListA ), static_cast<int>(existsA) );
    model->switchToMailbox( idxA );
    cClient(t.mk("SELECT a\r\n"));

    helperFakeExistsUidValidityUidNext();

    // Verify that we indeed received what we wanted
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( list->fetched() );

    QCOMPARE( model->rowCount( msgListA ), static_cast<int>(existsA) );
    QVERIFY( errorSpy->isEmpty() );

    helperSyncFlags();

    // No errors
    if ( ! errorSpy->isEmpty() )
        qDebug() << errorSpy->first();
    QVERIFY( errorSpy->isEmpty() );

    helperCheckCache();

    QVERIFY( list->fetched() );
}

/** @short Simulates fetching flags for messages 1:$exists */
void LibMailboxSync::helperSyncFlags()
{
    QCoreApplication::processEvents();
    QByteArray expectedFetch = t.mk("FETCH ") +
            (existsA == 1 ? QByteArray("1") : QByteArray("1:") + QString::number(existsA).toUtf8()) +
            QByteArray(" (FLAGS)\r\n");
    cClient(expectedFetch);
    QByteArray buf;
    for (uint i = 1; i <= existsA; ++i) {
        buf += "* " + QByteArray::number(i) + " FETCH (FLAGS (";
        switch (i % 10) {
        case 0:
        case 1:
            buf += "\\Seen";
            break;
        case 2:
        case 3:
        case 4:
            buf += "\\Seen \\Answered";
            break;
        case 5:
            buf += "\\Seen starred";
            break;
        case 6:
        case 7:
        case 8:
            buf += "\\Seen \\Answered $NotJunk";
            break;
        case 9:
            break;
        }
        buf += "))\r\n";
        if (buf.size() > 10*1024) {
            // Flush the output buffer roughly every 10kB
            cServer(buf);
            buf.clear();
        }
    }
    // Now the ugly part -- we know that the Model has that habit of processing at most 100 responses at once and then returning
    // from the event handler.  The idea is to make sure that the GUI can run even in presence of incoming FLAGS in a 50k mailbox
    // (or really anything else; the point is to avoid GUI blocking).  Under normal circumstances, this shouldn't really matter as
    // the event loop will take care of processing all the events, but with tests which "emulate" the event loop through
    // repeatedly calling QCoreApplication::processEvents(), we might *very easily* leave some responses undelivered.
    // This is where our crude hack comes into play -- we know that each message generated one response, so we just make sure that
    // Model's responseReceived will surely runs enough times. Nasty, isn't it? Well, better than introducing a side channel from
    // Model to signal "I really need to process more events, KTHXBYE", IMHO.
    for (uint i = 0; i < (existsA / 100) + 1; ++i)
        QCoreApplication::processEvents();

    cServer(buf + t.last("OK yay\r\n"));
}

/** @short Helper: update flags for some message */
void LibMailboxSync::helperOneFlagUpdate( const QModelIndex &message )
{
    cServer(QString::fromUtf8("* %1 FETCH (FLAGS (\\SeEn fOo bar))\r\n").arg( message.row() + 1 ).toUtf8());
    QStringList expectedFlags;
    expectedFlags << QLatin1String("\\Seen") << QLatin1String("fOo") << QLatin1String("bar");
    expectedFlags.sort();
    QCOMPARE(message.data(Imap::Mailbox::RoleMessageFlags).toStringList(), expectedFlags);
    QVERIFY(errorSpy->isEmpty());
}

void LibMailboxSync::helperSyncASomeNew( int number )
{
    QCOMPARE( model->rowCount( msgListA ), static_cast<int>(existsA) );
    model->switchToMailbox( idxA );
    cClient(t.mk("SELECT a\r\n"));

    uint oldExistsA = existsA;
    for ( int i = 0; i < number; ++i ) {
        ++existsA;
        uidMapA.append( uidNextA );
        ++uidNextA;
    }
    helperFakeExistsUidValidityUidNext();

    // Verify that we indeed received what we wanted
    Imap::Mailbox::TreeItemMsgList* list = dynamic_cast<Imap::Mailbox::TreeItemMsgList*>( static_cast<Imap::Mailbox::TreeItem*>( msgListA.internalPointer() ) );
    Q_ASSERT( list );
    QVERIFY( ! list->fetched() );

    helperFakeUidSearch( oldExistsA );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY( list->fetched() );

    QCOMPARE( model->rowCount( msgListA ), static_cast<int>( existsA ) );
    QVERIFY( errorSpy->isEmpty() );

    helperSyncFlags();

    // No errors
    if ( ! errorSpy->isEmpty() )
        qDebug() << errorSpy->first();
    QVERIFY( errorSpy->isEmpty() );

    helperCheckCache();

    QVERIFY( list->fetched() );
}

/** @short Helper: make sure that UID mapping is correct */
void LibMailboxSync::helperVerifyUidMapA()
{
    QCOMPARE( model->rowCount( msgListA ), static_cast<int>(existsA) );
    Q_ASSERT( static_cast<int>(existsA) == uidMapA.size() );
    for ( int i = 0; i < uidMapA.size(); ++i ) {
        QModelIndex message = model->index( i, 0, msgListA );
        Q_ASSERT( message.isValid() );
        QVERIFY( uidMapA[i] != 0 );
        QCOMPARE( message.data( Imap::Mailbox::RoleMessageUid ).toUInt(), uidMapA[i] );
    }
}

/** @short Helper: verify that values recorded in the cache are valid */
void LibMailboxSync::helperCheckCache(bool ignoreUidNext)
{
    using namespace Imap::Mailbox;

    // Check the cache
    SyncState syncState = model->cache()->mailboxSyncState(QLatin1String("a"));
    QCOMPARE(syncState.exists(), existsA);
    QCOMPARE(syncState.isUsableForSyncing(), true);
    if (!ignoreUidNext) {
        QCOMPARE(syncState.uidNext(), uidNextA);
    }
    QCOMPARE(syncState.uidValidity(), uidValidityA);
    QCOMPARE(model->cache()->uidMapping(QLatin1String("a")), uidMapA);
    QCOMPARE(static_cast<uint>(uidMapA.size()), existsA);

    SyncState ssFromTree = model->findMailboxByName(QLatin1String("a"))->syncState;
    SyncState ssFromCache = syncState;
    if (ignoreUidNext) {
        ssFromTree.setUidNext(0);
        ssFromCache.setUidNext(0);
    }
    QCOMPARE(ssFromCache, ssFromTree);

    if (model->isNetworkAvailable()) {
        // when offline, calling cEmpty would assert fail
        cEmpty();
    }

    QVERIFY(errorSpy->isEmpty());
}

void LibMailboxSync::initialMessages(const uint exists)
{
    // Setup ten fake messages
    existsA = exists;
    uidValidityA = 333;
    for (uint i = 1; i <= existsA; ++i) {
        uidMapA << i;
    }
    uidNextA = uidMapA.last() + 1;
    helperSyncAWithMessagesEmptyState();

    for (uint i = 1; i <= existsA; ++i) {
        QModelIndex messageIndex = msgListA.child(i - 1, 0);
        Q_ASSERT(messageIndex.isValid());
        QCOMPARE(messageIndex.data(Imap::Mailbox::RoleMessageUid).toUInt(), i);
    }

    // open the mailbox
    msgListModel->setMailbox(idxA);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

/** @short Helper for creating a fake FETCH response with all usually fetched fields

This function will prepare a response mentioning a minimal set of ENVELOPE, UID, BODYSTRUCTURE etc. Please note that
the actual string won't be passed to the fake socket, but rather returned; this is needed because the fake socket can't accept
incremental data, but we have to feed it with stuff at once.
*/
QByteArray LibMailboxSync::helperCreateTrivialEnvelope(const uint seq, const uint uid, const QString &subject)
{
    return QString::fromUtf8("* %1 FETCH (UID %2 RFC822.SIZE 89 ENVELOPE (NIL \"%3\" NIL NIL NIL NIL NIL NIL NIL NIL) "
                              "BODYSTRUCTURE (\"text\" \"plain\" () NIL NIL NIL 19 2 NIL NIL NIL NIL))\r\n").arg(
                                      QString::number(seq), QString::number(uid), subject ).toUtf8();
}

/** @short Make sure that the only existing task is the KeepMailboxOpenTask and nothing else */
void LibMailboxSync::justKeepTask()
{
    QCOMPARE(model->taskModel()->rowCount(), 1);
    QModelIndex parser1 = model->taskModel()->index(0, 0);
    QVERIFY(parser1.isValid());
    QCOMPARE(model->taskModel()->rowCount(parser1), 1);
    QModelIndex firstTask = parser1.child(0, 0);
    QVERIFY(firstTask.isValid());
    QVERIFY(!firstTask.child(0, 0).isValid());
    QCOMPARE(model->accessParser(static_cast<Imap::Parser*>(parser1.internalPointer())).connState,
             Imap::CONN_STATE_SELECTED);
    Imap::Mailbox::KeepMailboxOpenTask *keepTask = dynamic_cast<Imap::Mailbox::KeepMailboxOpenTask*>(static_cast<Imap::Mailbox::ImapTask*>(firstTask.internalPointer()));
    QVERIFY(keepTask);
    QVERIFY(keepTask->requestedEnvelopes.isEmpty());
    QVERIFY(keepTask->requestedParts.isEmpty());
    QVERIFY(keepTask->newArrivalsFetch.isEmpty());
}

/** @short Find an item within a tree identified by a "path"

Based on a textual "path" like "1.2.3" or "6", find an index within the model which corresponds to that location.
Indexing is zero-based, so "6" means model->index(6, 0) while "1.2.3" actually refers to model->index(1, 0).child(2, 0).child(3, 0).
*/
QModelIndex LibMailboxSync::findIndexByPosition(const QAbstractItemModel *model, const QString &where)
{
    QStringList list = where.split(QLatin1Char('.'));
    Q_ASSERT(!list.isEmpty());
    QList<QPair<int,int>> items;
    Q_FOREACH(const QString &item, list) {
        QStringList rowColumn = item.split(QLatin1Char('c'));
        Q_ASSERT(rowColumn.size() >= 1);
        Q_ASSERT(rowColumn.size() <= 2);
        bool ok;
        int row = rowColumn[0].toInt(&ok);
        Q_ASSERT(ok);
        int column = 0;
        if (rowColumn.size() == 2) {
            column = rowColumn[1].toInt(&ok);
            Q_ASSERT(ok);
        }
        items << qMakePair(row, column);
    }

    QModelIndex index = QModelIndex();
    for (auto it = items.constBegin(); it != items.constEnd(); ++it) {
        index = model->index(it->first, it->second, index);
        if (it + 1 != items.constEnd()) {
            // this index is an intermediate one in the path, hence it should not really fail
            Q_ASSERT(index.isValid());
        }
    }
    return index;
}

/** @short Forwarding function which allows this to work without explicitly befriending each and every test */
void LibMailboxSync::setModelNetworkPolicy(Imap::Mailbox::Model *model, const Imap::Mailbox::NetworkPolicy policy)
{
    model->setNetworkPolicy(policy);
}

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
    return qstrdup(buf.toUtf8().constData());
}


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
    return qstrdup(buf.toUtf8().constData());
}

/** @short Debug data dumper for the MessageDataBundle */
template<>
char *toString(const Imap::Mailbox::AbstractCache::MessageDataBundle &bundle)
{
    QString buf;
    QDebug d(&buf);
    d << "UID:" << bundle.uid << "Envelope:" << bundle.envelope << "size:" << bundle.size <<
         "bodystruct:" << bundle.serializedBodyStructure;
    return qstrdup(buf.toUtf8().constData());
}
}
