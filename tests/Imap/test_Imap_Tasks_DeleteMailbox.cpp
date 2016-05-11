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

#include <QtTest>
#include "test_Imap_Tasks_DeleteMailbox.h"
#include "Common/MetaTypes.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/Model.h"

void ImapModelDeleteMailboxTest::init()
{
    fakeListChildMailboxesMap[QLatin1String("")] = QStringList() << QStringLiteral("a");
    LibMailboxSync::init();
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    deletedSpy = new QSignalSpy(model, SIGNAL(mailboxDeletionSucceded(QString)));
    failedSpy = new QSignalSpy(model, SIGNAL(mailboxDeletionFailed(QString,QString)));
    t.reset();
}

void ImapModelDeleteMailboxTest::cleanup()
{
    delete deletedSpy;
    deletedSpy = 0;
    delete failedSpy;
    failedSpy = 0;
    LibMailboxSync::cleanup();
}

void ImapModelDeleteMailboxTest::initTestCase()
{
    deletedSpy = 0;
    failedSpy = 0;
    LibMailboxSync::initTestCase();
}

void ImapModelDeleteMailboxTest::initWithOne()
{
    // Init with one example mailbox
    model->rowCount(QModelIndex());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(model->rowCount(QModelIndex()), 2);
    idxA = model->index(1, 0, QModelIndex());
    QCOMPARE(model->data(idxA, Qt::DisplayRole), QVariant(QLatin1String("a")));
    QVERIFY(idxA.isValid());
    msgListA = idxA.child(0, 0);
    QVERIFY(msgListA.isValid());
    cEmpty();
}

void ImapModelDeleteMailboxTest::initWithTwo()
{
    // oops, we need at least two mailboxes here
    fakeListChildMailboxesMap[QLatin1String("")] = QStringList() << QStringLiteral("a") << QStringLiteral("b");
    LibMailboxSync::init();
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    deletedSpy = new QSignalSpy(model, SIGNAL(mailboxDeletionSucceded(QString)));
    failedSpy = new QSignalSpy(model, SIGNAL(mailboxDeletionFailed(QString,QString)));
    t.reset();

    model->rowCount(QModelIndex());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(model->rowCount(QModelIndex()), 3);
    idxA = model->index(1, 0, QModelIndex());
    QCOMPARE(model->data(idxA, Qt::DisplayRole), QVariant(QLatin1String("a")));
    idxB = model->index(2, 0, QModelIndex());
    QCOMPARE(model->data(idxB, Qt::DisplayRole), QVariant(QLatin1String("b")));
    msgListA = idxA.child(0, 0);
    QVERIFY(msgListA.isValid());
    msgListB = idxB.child(0, 0);
    QVERIFY(msgListB.isValid());
    cEmpty();
}

void ImapModelDeleteMailboxTest::testDeleteOne()
{
    initWithOne();

    // Now test the actual creating process
    model->deleteMailbox(QStringLiteral("a"));
    cClient(t.mk("DELETE a\r\n"));
    cServer(t.last("OK deleted\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(model->rowCount(QModelIndex()), 1);
    QCoreApplication::processEvents();
    cEmpty();
    QCOMPARE(deletedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
}

void ImapModelDeleteMailboxTest::testDeleteFail()
{
    initWithOne();

    // Test failure of the DELETE command
    model->deleteMailbox(QStringLiteral("a"));
    cClient(t.mk("DELETE a\r\n"));
    cServer(t.last("NO muhehe\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(model->rowCount(QModelIndex()), 2);
    cEmpty();
    QCOMPARE(failedSpy->size(), 1);
    QVERIFY(deletedSpy->isEmpty());
}

void ImapModelDeleteMailboxTest::testDeleteAnother()
{
    initWithTwo();
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer(t.last("OK selected\r\n"));
    model->deleteMailbox(QStringLiteral("b"));
    cClient(t.mk("DELETE b\r\n"));
    cServer(t.last("OK deleted\r\n"));
    cEmpty();
    QCOMPARE(model->rowCount(QModelIndex()), 2);
    QCOMPARE(deletedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
}

/** @short Removing mailbox which is being synced */
void ImapModelDeleteMailboxTest::testDeleteSyncing()
{
    initWithOne();
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));

    // Cannot delete while the sync is still pending
    model->deleteMailbox(QStringLiteral("a"));
    cEmpty();
    QVERIFY(deletedSpy->isEmpty());
    QCOMPARE(failedSpy->size(), 1);
    QCOMPARE(model->rowCount(QModelIndex()), 2);
    failedSpy->clear();

    // the deletion will succeed now
    cServer(t.last("OK selected\r\n"));
    model->deleteMailbox(QStringLiteral("a"));
    cClient(t.mk("CLOSE\r\n"));
    cServer(t.last("OK closed\r\n"));
    cClient(t.mk("DELETE a\r\n"));
    cServer(t.last("OK deleted\r\n"));
    cEmpty();
    QCOMPARE(model->rowCount(QModelIndex()), 1);
    QCOMPARE(deletedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
}

/** @short Removing currently selected mailbox with no parallel activity */
void ImapModelDeleteMailboxTest::testDeleteSelected()
{
    initWithTwo();
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer(t.last("OK selected\r\n"));
    model->deleteMailbox(QStringLiteral("a"));
    cClient(t.mk("CLOSE\r\n"));
    cServer(t.last("OK closed\r\n"));
    cClient(t.mk("DELETE a\r\n"));
    cServer(t.last("OK deleted\r\n"));
    cEmpty();
    QCOMPARE(model->rowCount(QModelIndex()), 2);
    QCOMPARE(deletedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());

    // Check that we can select another mailbox now
    QCOMPARE(model->rowCount(msgListB), 0);
    cClient(t.mk("SELECT b\r\n"));
}

/** @short Trying to remove mailbox where some data are still being transferred */
void ImapModelDeleteMailboxTest::testDeleteSelectedPendingMetadata()
{
    initWithOne();
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 1 EXISTS\r\n" + t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 666\r\n" + t.last("OK searched\r\n"));
    cClient(t.mk("FETCH 1 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    cEmpty();

    // Send a request for message metadata, but hold the response
    QCOMPARE(model->rowCount(msgListA.child(0, 0)), 0);
    cClient(t.mk("UID FETCH 666 (" FETCH_METADATA_ITEMS ")\r\n"));
    QByteArray metadataFetchResp = t.last("OK fetched\r\n");
    cEmpty();

    // This deletion attempt shall fail
    model->deleteMailbox(QStringLiteral("a"));
    cEmpty();
    QVERIFY(deletedSpy->isEmpty());
    QCOMPARE(failedSpy->size(), 1);
    QCOMPARE(model->rowCount(QModelIndex()), 2);
    failedSpy->clear();

    // After the data arrive, though, we're all set and can delete the mailbox just fine
    cServer(metadataFetchResp);
    model->deleteMailbox(QStringLiteral("a"));
    cClient(t.mk("CLOSE\r\n"));
    cServer(t.last("OK closed\r\n"));
    cClient(t.mk("DELETE a\r\n"));
    cServer(t.last("OK deleted\r\n"));
    cEmpty();
    QCOMPARE(model->rowCount(QModelIndex()), 1);
    QCOMPARE(deletedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
}

/** @short Deleting a mailbox which is currently selected while another mailbox is currently synced */
void ImapModelDeleteMailboxTest::testDeleteSelectedPendingAnotherMailbox()
{
    initWithTwo();

    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer(t.last("OK selected\r\n"));
    cEmpty();

    // Request syncing another mailbox
    QCOMPARE(model->rowCount(msgListB), 0);
    cClient(t.mk("SELECT b\r\n"));

    // Now before the other mailbox gets synced, ask for deletion of the original one
    model->deleteMailbox(QStringLiteral("a"));
    cEmpty();

    cServer(t.last("OK selected\r\n"));

    QVERIFY(deletedSpy->isEmpty());
    QVERIFY(failedSpy->isEmpty());

    cClient(t.mk("DELETE a\r\n"));
    cServer(t.last("OK deleted\r\n"));
    cEmpty();
    QCOMPARE(model->rowCount(QModelIndex()), 2);
    QCOMPARE(deletedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
}

/** @short Deleting currently selected mailbox with pending activity while a request to switch to another mailbox is pending */
void ImapModelDeleteMailboxTest::testDeleteSelectedPendingAnotherMailboxDelayed()
{
    initWithTwo();

    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 1 EXISTS\r\n" + t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 666\r\n" + t.last("OK searched\r\n"));
    cClient(t.mk("FETCH 1 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    cEmpty();

    // Send a request for message metadata, but hold the response
    QCOMPARE(model->rowCount(msgListA.child(0, 0)), 0);
    cClient(t.mk("UID FETCH 666 (" FETCH_METADATA_ITEMS ")\r\n"));
    QByteArray metadataFetchResp = t.last("OK fetched\r\n");
    cEmpty();

    // Request syncing another mailbox
    QCOMPARE(model->rowCount(msgListB), 0);

    // Send back the pending response for this mailbox' messages
    cServer(metadataFetchResp);

    // Now before the other mailbox gets synced, ask for deletion of the original one
    model->deleteMailbox(QStringLiteral("a"));

    // In the meanwhile, we're in process of selecting another mailbox
    cClient(t.mk("SELECT b\r\n"));
    // As such, our original mailbox won't be deleted yet
    QVERIFY(deletedSpy->isEmpty());
    QVERIFY(failedSpy->isEmpty());
    cEmpty();

    // After the second mailbox gets selected, though, the deletion of the original is swift
    cServer(t.last("OK selected\r\n"));
    cClient(t.mk("DELETE a\r\n"));
    cServer(t.last("OK deleted\r\n"));
    cEmpty();
    QCOMPARE(model->rowCount(QModelIndex()), 2);
    QCOMPARE(deletedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
}

/** @short There should be no [CLOSED] when deleting a current mailbox

https://bugs.kde.org/show_bug.cgi?id=362854
*/
void ImapModelDeleteMailboxTest::testDeleteCurrentQresync()
{
    helperDeleteCurrentQresync(DeleteStatus::OK);
}

/** @short There should be no [CLOSED] when deleting a current mailbox

...check what happens when the DELETE itself fails, for the sake of completeness.

https://bugs.kde.org/show_bug.cgi?id=362854
*/
void ImapModelDeleteMailboxTest::testDeleteCurrentQresyncDeleteNo()
{
    helperDeleteCurrentQresync(DeleteStatus::NO);
}

/** @short Mailbox deletion and the absence of the [CLOSED] response code

https://bugs.kde.org/show_bug.cgi?id=362854
*/
void ImapModelDeleteMailboxTest::helperDeleteCurrentQresync(const DeleteStatus deleteSucceeded)
{
    initWithTwo();
    Imap::Mailbox::SyncState syncState;
    helperQresyncAInitial(syncState);
    cEmpty();
    model->deleteMailbox(QStringLiteral("a"));
    cClient(t.mk("CLOSE\r\n"));
    cServer(t.last("OK closed\r\n"));
    cClient(t.mk("DELETE a\r\n"));
    switch (deleteSucceeded) {
    case DeleteStatus::OK:
        cServer(t.last("OK deleted\r\n"));
        break;
    case DeleteStatus::NO:
        cServer(t.last("NO delete failed\r\n"));
        break;
    }
    cEmpty();
    model->switchToMailbox(idxB);
    cClient(t.mk("SELECT b\r\n"));
    cServer("* 0 EXISTS\r\n"
            + t.last("OK selected\r\n"));
    cEmpty();
    justKeepTask();
}

QTEST_GUILESS_MAIN(ImapModelDeleteMailboxTest)
