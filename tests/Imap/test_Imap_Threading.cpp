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

#include <algorithm>
#include <QtTest>
#include "test_Imap_Threading.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Streams/FakeSocket.h"
#include "Utils/FakeCapabilitiesInjector.h"

Q_DECLARE_METATYPE(Mapping);

/** @short Test that the ThreadingMsgListModel can process a static THREAD response */
void ImapModelThreadingTest::testStaticThreading()
{
    QFETCH(uint, messageCount);
    QFETCH(QByteArray, response);
    QFETCH(Mapping, mapping);
    initialMessages(messageCount);
    QCOMPARE(SOCK->writtenStuff(), t.mk("UID THREAD REFS utf-8 ALL\r\n"));
    SOCK->fakeReading(QByteArray("* THREAD ") + response + QByteArray("\r\n") + t.last("OK thread\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    verifyMapping(mapping);
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QVERIFY(errorSpy->isEmpty());
}

/** @short Data for the testStaticThreading */
void ImapModelThreadingTest::testStaticThreading_data()
{
    QTest::addColumn<uint>("messageCount");
    QTest::addColumn<QByteArray>("response");
    QTest::addColumn<Mapping>("mapping");

    Mapping m;

    // A linear subset of messages
    m[QStringLiteral("0")] = 1; // index 0: UID
    m[QStringLiteral("0.0")] = 0; // index 0.0: invalid
    m[QStringLiteral("0.1")] = 0; // index 0.1: invalid
    m[QStringLiteral("1")] = 2;

    QTest::newRow("no-threads")
            << (uint)2
            << QByteArray("(1)(2)")
            << m;

    // No threading at all; just an unthreaded list of all messages
    m[QStringLiteral("2")] = 3;
    m[QStringLiteral("3")] = 4;
    m[QStringLiteral("4")] = 5;
    m[QStringLiteral("5")] = 6;
    m[QStringLiteral("6")] = 7;
    m[QStringLiteral("7")] = 8;
    m[QStringLiteral("8")] = 9;
    m[QStringLiteral("9")] = 10; // index 9: UID 10
    m[QStringLiteral("10")] = 0; // index 10: invalid

    QTest::newRow("no-threads-ten")
            << (uint)10
            << QByteArray("(1)(2)(3)(4)(5)(6)(7)(8)(9)(10)")
            << m;

    // A flat list of threads, but now with some added fake nodes for complexity.
    // The expected result is that they get cleared as redundant and useless nodes.
    QTest::newRow("extra-parentheses")
            << (uint)10
            << QByteArray("(1)((2))(((3)))((((4))))(((((5)))))(6)(7)(8)(9)(((10)))")
            << m;

    // A liner nested list (ie. a message is a child of the previous one)
    m.clear();
    m[QStringLiteral("0")] = 1;
    m[QStringLiteral("1")] = 0;
    m[QStringLiteral("0.0")] = 2;
    m[QStringLiteral("0.1")] = 0;
    m[QStringLiteral("0.0.0")] = 0;
    m[QStringLiteral("0.0.1")] = 0;
    QTest::newRow("linear-threading-just-two")
            << (uint)2
            << QByteArray("(1 2)")
            << m;

    // The same, but with three messages
    m[QStringLiteral("0.0.0")] = 3;
    m[QStringLiteral("0.0.0.0")] = 0;
    QTest::newRow("linear-threading-just-three")
            << (uint)3
            << QByteArray("(1 2 3)")
            << m;

    // The same, but with some added parentheses
    m[QStringLiteral("0.0.0")] = 3;
    m[QStringLiteral("0.0.0.0")] = 0;
    QTest::newRow("linear-threading-just-three-extra-parentheses-outside")
            << (uint)3
            << QByteArray("((((1 2 3))))")
            << m;
    // The same, but with the extra parentheses in the innermost item
    QTest::newRow("linear-threading-just-three-extra-parentheses-inside")
            << (uint)3
            << QByteArray("(1 2 (((3))))")
            << m;

    // The same, but with the extra parentheses in the middle item
    // This is actually a server's bug, as the fake node should've been eliminated
    // by the IMAP server.
    m.clear();
    m[QStringLiteral("0")] = 1;
    m[QStringLiteral("1")] = 0;
    m[QStringLiteral("0.0")] = 2;
    m[QStringLiteral("0.0.0")] = 0;
    m[QStringLiteral("0.1")] = 3;
    m[QStringLiteral("0.1.0")] = 0;
    m[QStringLiteral("0.2")] = 0;
    QTest::newRow("linear-threading-just-extra-parentheses-middle")
            << (uint)3
            << QByteArray("(1 (2) 3)")
            << m;

    // A complex nested hierarchy with nodes to be promoted
    QByteArray response;
    complexMapping(m, response);
    QTest::newRow("complex-threading")
            << (uint)10
            << response
            << m;
}

void ImapModelThreadingTest::testThreadDeletionsAdditions()
{
    QFETCH(uint, exists);
    QFETCH(QByteArray, response);
    QFETCH(QStringList, operations);
    Q_ASSERT(operations.size() % 2 == 0);

    initialMessages(exists);

    QCOMPARE(SOCK->writtenStuff(), t.mk("UID THREAD REFS utf-8 ALL\r\n"));
    SOCK->fakeReading(QByteArray("* THREAD ") + response + QByteArray("\r\n") + t.last("OK thread\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(response, treeToThreading(QModelIndex()));
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QVERIFY(errorSpy->isEmpty());

    for (int i = 0; i < operations.size(); i += 2) {
        QString whichOne = operations[i];
        QString expectedRes = operations[i+1];
        if (whichOne[0] == QLatin1Char('-')) {
            // Removing messages. The number specifies a *sequence number*
            Q_ASSERT(whichOne.size() > 1);
            SOCK->fakeReading(QStringLiteral("* %1 EXPUNGE\r\n").arg(whichOne.mid(1)).toUtf8());
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            QVERIFY(SOCK->writtenStuff().isEmpty());
            QVERIFY(errorSpy->isEmpty());
            QCOMPARE(QString::fromUtf8(treeToThreading(QModelIndex())), expectedRes);
        } else if (whichOne[0] == QLatin1Char('+')) {
            // New additions. The number specifies the number of new arrivals.
            Q_ASSERT(whichOne.size() > 1);
            int newArrivals = whichOne.midRef(1).toInt();
            Q_ASSERT(newArrivals > 0);

            for (int i = 0; i < newArrivals; ++i) {
                uidMapA.append(uidNextA + i);
            }

            existsA += newArrivals;

            // Send information about the new arrival
            SOCK->fakeReading(QStringLiteral("* %1 EXISTS\r\n").arg(QString::number(existsA)).toUtf8());
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();

            // At this point, the threading code should have asked for new threading and the generic IMAP model code for flags
            QByteArray expected = t.mk("UID FETCH ") + QStringLiteral("%1:* (FLAGS)\r\n").arg(QString::number(uidNextA)).toUtf8();
            uidNextA += newArrivals;
            QByteArray uidFetchResponse;
            for (int i = 0; i < newArrivals; ++i) {
                int offset = existsA - newArrivals + i;
                uidFetchResponse += QStringLiteral("* %1 FETCH (UID %2 FLAGS ())\r\n").arg(
                            // the sequqnce number is one-based, not zero-based
                            QString::number(offset + 1),
                            QString::number(uidMapA[offset])
                            ).toUtf8();
            }

            // See LibMailboxSync::helperSyncFlags for why have to do this
            for (int i = 0; i < (newArrivals / 100) + 1; ++i)
                QCoreApplication::processEvents();

            uidFetchResponse += t.last("OK fetch\r\n");
            QCOMPARE(SOCK->writtenStuff(), expected);
            SOCK->fakeReading(uidFetchResponse);
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            QByteArray expectedThread = t.mk("UID THREAD REFS utf-8 ALL\r\n");
            QByteArray uidThreadResponse = QByteArray("* THREAD ") + expectedRes.toUtf8() + QByteArray("\r\n") + t.last("OK thread\r\n");
            QCOMPARE(SOCK->writtenStuff(), expectedThread);
            SOCK->fakeReading(uidThreadResponse);
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();

            QVERIFY(SOCK->writtenStuff().isEmpty());
            QVERIFY(errorSpy->isEmpty());
            QCOMPARE(QString::fromUtf8(treeToThreading(QModelIndex())), expectedRes);
        } else {
            Q_ASSERT(false);
        }
    }
}

void ImapModelThreadingTest::testThreadDeletionsAdditions_data()
{
    QTest::addColumn<uint>("exists");
    QTest::addColumn<QByteArray>("response");
    QTest::addColumn<QStringList>("operations");

    // Just test that dumping works; no deletions yet
    QTest::newRow("basic-flat-list") << (uint)2 << QByteArray("(1)(2)") << QStringList();
    // Simple tests for flat lists
    QTest::newRow("flat-list-two-delete-first") << (uint)2 << QByteArray("(1)(2)") << (QStringList() << QStringLiteral("-1") << QStringLiteral("(2)"));
    QTest::newRow("flat-list-two-delete-last") << (uint)2 << QByteArray("(1)(2)") << (QStringList() << QStringLiteral("-2") << QStringLiteral("(1)"));
    QTest::newRow("flat-list-three-delete-first") << (uint)3 << QByteArray("(1)(2)(3)") << (QStringList() << QStringLiteral("-1") << QStringLiteral("(2)(3)"));
    QTest::newRow("flat-list-three-delete-middle") << (uint)3 << QByteArray("(1)(2)(3)") << (QStringList() << QStringLiteral("-2") << QStringLiteral("(1)(3)"));
    QTest::newRow("flat-list-three-delete-last") << (uint)3 << QByteArray("(1)(2)(3)") << (QStringList() << QStringLiteral("-3") << QStringLiteral("(1)(2)"));
    // Try to test a single thread
    QTest::newRow("simple-three-delete-first") << (uint)3 << QByteArray("(1 2 3)") << (QStringList() << QStringLiteral("-1") << QStringLiteral("(2 3)"));
    QTest::newRow("simple-three-delete-middle") << (uint)3 << QByteArray("(1 2 3)") << (QStringList() << QStringLiteral("-2") << QStringLiteral("(1 3)"));
    QTest::newRow("simple-three-delete-last") << (uint)3 << QByteArray("(1 2 3)") << (QStringList() << QStringLiteral("-3") << QStringLiteral("(1 2)"));
    // A thread with a fork:
    // 1
    // +- 2
    //    +- 3
    // +- 4
    //    +- 5
    QTest::newRow("fork") << (uint)5 << QByteArray("(1 (2 3)(4 5))") << QStringList();
    QTest::newRow("fork-delete-first") << (uint)5 << QByteArray("(1 (2 3)(4 5))") << (QStringList() << QStringLiteral("-1") << QStringLiteral("(2 (3)(4 5))"));
    QTest::newRow("fork-delete-second") << (uint)5 << QByteArray("(1 (2 3)(4 5))") << (QStringList() << QStringLiteral("-2") << QStringLiteral("(1 (3)(4 5))"));
    QTest::newRow("fork-delete-third") << (uint)5 << QByteArray("(1 (2 3)(4 5))") << (QStringList() << QStringLiteral("-3") << QStringLiteral("(1 (2)(4 5))"));
    // Remember, we're using EXPUNGE which use sequence numbers, not UIDs
    QTest::newRow("fork-delete-two-three") << (uint)5 << QByteArray("(1 (2 3)(4 5))") <<
                                              (QStringList() << QStringLiteral("-2") << QStringLiteral("(1 (3)(4 5))") << QStringLiteral("-2") << QStringLiteral("(1 4 5)"));
    QTest::newRow("fork-delete-two-four") << (uint)5 << QByteArray("(1 (2 3)(4 5))") <<
                                              (QStringList() << QStringLiteral("-2") << QStringLiteral("(1 (3)(4 5))") << QStringLiteral("-3") << QStringLiteral("(1 (3)(5))"));

    // Test new arrivals
    QTest::newRow("flat-list-new") << (uint)2 << QByteArray("(1)(2)") << (QStringList() << QStringLiteral("+1") << QStringLiteral("(1)(2)(3)"));

}

/** @short Test deletion of several nodes at once resulting in child promotion

There was a bug in https://gerrit.vesnicky.cesnet.cz/r/#/c/153/1, an assert failure at src/Imap/Model/ThreadingMsgListModel.cpp:1117.
The bug happened because an initial version of that patch failed to fix the other part of an if branch, a place where the old code
assumed that all other thread nodes had their offsets already fixed.

Testing this with Qt5 is not easy, though, due to the random iteration order of QHash which is used within the ThreadingMsgListModel.
What we're looking for is a situation where a non-leaf node is processed by the code *after* some of its preceding siblings which
happen to be leaves were already removed.
*/
void ImapModelThreadingTest::testVanishedHierarchyReplacement()
{
    // Initialize the same data structure as the one which is used within the ThreadingMsgListModel.
    // We're doing this in order to be able to prepare such a sequence of keys which will trigger that
    // particular sequence of hash traversal which we need from here.
    decltype(threadingModel->threading) dummy;
    for (int i = 0; i < 5; ++i) {
        dummy[i];
    }
    auto keys = dummy.keys();
    keys.removeOne(0); // but it's important that 0 was there for the actual hash iteration order
    QCOMPARE(keys.size(), 4);

    initialMessages(4);

    // The threading will have to look like this one:
    // 1
    // 2
    // 3
    // +- 4
    //
    // ..except that the numbers above correspond to the indexes in the list of keys of the hash
    // in the hash's iteration order, not actual UIDs.
    QCOMPARE(SOCK->writtenStuff(), t.mk("UID THREAD REFS utf-8 ALL\r\n"));
    SOCK->fakeReading("* THREAD (" + QByteArray::number(keys[0]) + ")(" + QByteArray::number(keys[1]) + ")(" +
            QByteArray::number(keys[2]) + ' ' + QByteArray::number(keys[3]) + ")\r\n" + t.last("OK thread\r\n"));
    cEmpty();
    QVERIFY(errorSpy->isEmpty());

    cServer("* VANISHED " + QByteArray::number(keys[0]) + ',' + QByteArray::number(keys[1]) + ',' + QByteArray::number(keys[2]) + "\r\n");
    cEmpty();
    QCOMPARE(QString::fromUtf8(treeToThreading(QModelIndex())), QString::fromUtf8("(%1)").arg(keys[3]));
    cEmpty();
    QVERIFY(errorSpy->isEmpty());
}

/** @short Test deletion of one message */
void ImapModelThreadingTest::testDynamicThreading()
{
    initialMessages(10);

    // A complex nested hierarchy with nodes to be promoted
    Mapping mapping;
    QByteArray response;
    complexMapping(mapping, response);

    QCOMPARE(SOCK->writtenStuff(), t.mk("UID THREAD REFS utf-8 ALL\r\n"));
    SOCK->fakeReading(QByteArray("* THREAD ") + response + QByteArray("\r\n") + t.last("OK thread\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    verifyMapping(mapping);
    QCOMPARE(threadingModel->rowCount(QModelIndex()), 4);
    IndexMapping indexMap = buildIndexMap(mapping);
    verifyIndexMap(indexMap, mapping);
    // The response is actually slightly different, but never mind (extra parentheses around 7)
    QCOMPARE(treeToThreading(QModelIndex()), QByteArray("(1)(2 3)(4 (5)(6))(7 (8)(9 10))"));

    // this one will be deleted
    QPersistentModelIndex delete10 = findItem(QStringLiteral("3.1.0"));
    QVERIFY(delete10.isValid());

    // its parent
    QPersistentModelIndex msg9 = findItem(QStringLiteral("3.1"));
    QCOMPARE(QPersistentModelIndex(delete10.parent()), msg9);
    QCOMPARE(threadingModel->rowCount(msg9), 1);

    {
        // Qt5 bug: there is now QAsbtractProxyModel::sibling which does not do the right thing anymore.
        // Make sure our workaround is in place.
        Q_ASSERT(msg9.isValid());
        auto sibling1 = msg9.sibling(msg9.row(), msg9.column() + 1);
        QVERIFY(sibling1.isValid());
        QCOMPARE(sibling1.row(), msg9.row());
        auto sibling2 = sibling1.sibling(sibling1.row(), sibling1.column() - 1);
        QVERIFY(sibling2.isValid());
        QCOMPARE(sibling2, QModelIndex(msg9));
    }

    // Delete the last message; it's some leaf
    SOCK->fakeReading("* 10 EXPUNGE\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    --existsA;
    QCOMPARE(msgListModel->rowCount(QModelIndex()), static_cast<int>(existsA));
    QCOMPARE(threadingModel->rowCount(msg9), 0);
    QVERIFY(!delete10.isValid());
    mapping.remove(QStringLiteral("3.1.0.0"));
    mapping[QStringLiteral("3.1.0")] = 0;
    indexMap.remove(QStringLiteral("3.1.0.0"));
    verifyMapping(mapping);
    verifyIndexMap(indexMap, mapping);
    QCOMPARE(treeToThreading(QModelIndex()), QByteArray("(1)(2 3)(4 (5)(6))(7 (8)(9))"));

    QPersistentModelIndex msg2 = findItem(QStringLiteral("1"));
    QVERIFY(msg2.isValid());
    QPersistentModelIndex msg3 = findItem(QStringLiteral("1.0"));
    QVERIFY(msg3.isValid());

    // Delete the root of the second thread
    SOCK->fakeReading("* 2 EXPUNGE\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    --existsA;
    QCOMPARE(msgListModel->rowCount(QModelIndex()), static_cast<int>(existsA));
    QCOMPARE(threadingModel->rowCount(QModelIndex()), 4);
    QPersistentModelIndex newMsg3 = findItem(QStringLiteral("1"));
    QVERIFY(!msg2.isValid());
    QVERIFY(msg3.isValid());
    QCOMPARE(msg3, newMsg3);
    mapping.remove(QStringLiteral("1.0.0"));
    mapping[QStringLiteral("1.0")] = 0;
    mapping[QStringLiteral("1")] = 3;
    verifyMapping(mapping);
    // Check the changed persistent indexes
    indexMap.remove(QStringLiteral("1.0.0"));
    indexMap[QStringLiteral("1")] = indexMap[QStringLiteral("1.0")];
    indexMap.remove(QStringLiteral("1.0"));
    verifyIndexMap(indexMap, mapping);
    QCOMPARE(treeToThreading(QModelIndex()), QByteArray("(1)(3)(4 (5)(6))(7 (8)(9))"));

    // Push a new message, but with an unknown UID so far
    ++existsA;
    ++uidNextA;
    QCOMPARE(existsA, 9u);
    QCOMPARE(uidNextA, 12u);
    SOCK->fakeReading("* 9 EXISTS\r\n");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // There should be a message with zero UID at the end
    QCOMPARE(treeToThreading(QModelIndex()), QByteArray("(1)(3)(4 (5)(6))(7 (8)(9))()"));

    QByteArray fetchCommand1 = t.mk("UID FETCH ") + QStringLiteral("%1:* (FLAGS)\r\n").arg(QString::number(uidNextA - 1)).toUtf8();
    QByteArray delayedFetchResponse1 = t.last("OK uid fetch\r\n");
    QByteArray threadCommand1 = t.mk("UID THREAD REFS utf-8 ALL\r\n");
    QByteArray delayedThreadResponse1 = t.last("OK threading\r\n");
    QCOMPARE(SOCK->writtenStuff(), fetchCommand1);

    QByteArray fetchUntagged1("* 9 FETCH (UID 66 FLAGS (\\Recent))\r\n");
    QByteArray threadUntagged1("* THREAD (1)(3)(4 (5)(6))((7)(8)(9)(66))\r\n");

    // Check that we've registered that change
    QCOMPARE(msgListModel->rowCount(QModelIndex()), static_cast<int>(existsA));

    // The UID haven't arrived yet
    cEmpty();

    if (1) {
        // Make the UID known
        cServer(fetchUntagged1 + delayedFetchResponse1);
        // After the UID got known, it is now time to ask for threading
        cClient(threadCommand1);
        // In the meanwhile, the message is temporarily visible as a standalone thread
        QCOMPARE(QString::fromUtf8(treeToThreading(QModelIndex())), QString::fromUtf8("(1)(3)(4 (5)(6))(7 (8)(9))(66)"));
        mapping[QStringLiteral("4")] = 66;
        indexMap[QStringLiteral("4")] = findItem(QStringLiteral("4"));
        verifyMapping(mapping);
        verifyIndexMap(indexMap, mapping);

        // Move the message into its proper place now
        cServer(threadUntagged1 + delayedThreadResponse1);
        indexMap[QStringLiteral("3.2")] = indexMap[QStringLiteral("4")];
        indexMap.remove(QStringLiteral("4"));
        mapping[QStringLiteral("3.2")] = mapping[QStringLiteral("4")];
        mapping[QStringLiteral("3.2.0")] = 0;
        mapping[QStringLiteral("3.3")] = 0;
        mapping[QStringLiteral("4")] = 0;
        QCOMPARE(QString::fromUtf8(treeToThreading(QModelIndex())), QString::fromUtf8("(1)(3)(4 (5)(6))(7 (8)(9)(66))"));
        verifyMapping(mapping);
        verifyIndexMap(indexMap, mapping);
    }

    cEmpty();
    QVERIFY(errorSpy->isEmpty());
}

/** @short Create a tuple of (mapping, string)*/
void ImapModelThreadingTest::complexMapping(Mapping &m, QByteArray &response)
{
    m.clear();
    m[QStringLiteral("0")] = 1;
    m[QStringLiteral("0.0")] = 0;
    m[QStringLiteral("1")] = 2;
    m[QStringLiteral("1.0")] = 3;
    m[QStringLiteral("1.0.0")] = 0;
    m[QStringLiteral("2")] = 4;
    m[QStringLiteral("2.0")] = 5;
    m[QStringLiteral("2.0.0")] = 0;
    m[QStringLiteral("2.1")] = 6;
    m[QStringLiteral("2.1.0")] = 0;
    m[QStringLiteral("3")] = 7;
    m[QStringLiteral("3.0")] = 8;
    m[QStringLiteral("3.0.0")] = 0;
    m[QStringLiteral("3.1")] = 9;
    m[QStringLiteral("3.1.0")] = 10;
    m[QStringLiteral("3.1.0.0")] = 0;
    m[QStringLiteral("3.2")] = 0;
    m[QStringLiteral("4")] = 0;
    response = QByteArray("(1)(2 3)(4 (5)(6))((7)(8)(9 10))");
}

/** @short Prepare an index to a threaded message based on a compressed text index description */
QModelIndex ImapModelThreadingTest::findItem(const QString &where)
{
    return findIndexByPosition(threadingModel, where);
}

/** @short Make sure that the specified indexes resolve to proper UIDs */
void ImapModelThreadingTest::verifyMapping(const Mapping &mapping)
{
    for(Mapping::const_iterator it = mapping.begin(); it != mapping.end(); ++it) {
        QModelIndex index = findItem(it.key());
        if (it.value()) {
            // it's a supposedly valid index
            if (!index.isValid()) {
                qDebug() << "Invalid index at" << it.key();
            }
            QVERIFY(index.isValid());
            int got = index.data(Imap::Mailbox::RoleMessageUid).toInt();
            if (got != it.value()) {
                qDebug() << "Index" << it.key();
            }
            QCOMPARE(got, it.value());
        } else {
            // we expect this one to be a fake
            if (index.isValid()) {
                qDebug() << "Valid index at" << it.key();
            }
            QVERIFY(!index.isValid());
        }
    }
}

/** @short Create a map of (position -> persistent_index) based on the current state of the model */
IndexMapping ImapModelThreadingTest::buildIndexMap(const Mapping &mapping)
{
    IndexMapping res;
    Q_FOREACH(const QString &key, mapping.keys()) {
        // only include real indexes
        res[key] = findItem(key);
    }
    return res;
}

void ImapModelThreadingTest::verifyIndexMap(const IndexMapping &indexMap, const Mapping &map)
{
    Q_FOREACH(const QString &key, indexMap.keys()) {
        if (!map.contains(key)) {
            qDebug() << "Table contains an index for" << key << ", but mapping to UIDs indicates that the index should not be there. Bug in the unit test, I'd say.";
            QFAIL("Extra index found in the map");
        }
        const QPersistentModelIndex &idx = indexMap[key];
        int expected = map[key];
        if (expected) {
            if (!idx.isValid()) {
                qDebug() << "Invalid persistent index for" << key;
            }
            QVERIFY(idx.isValid());
            QCOMPARE(idx.data(Imap::Mailbox::RoleMessageUid).toInt(), expected);
        } else {
            if (idx.isValid()) {
                qDebug() << "Persistent index for" << key << "should not be valid";
            }
            QVERIFY(!idx.isValid());
        }
    }
}

void ImapModelThreadingTest::init()
{
    LibMailboxSync::init();

    // Got to pretend that we support threads. Well, we really do :).
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("THREAD=REFS"));

    // Setup the threading model
    threadingModel->setUserWantsThreading(true);

    // Deactivate the helper_multipleExpunges slot. We don't want QTestLib to run it,
    // but I'm too lazy to add an extra QObject just for the slot.
    helper_multipleExpunges_hit = -1;
}

/** @short Walk the model and output a THREAD-like responsde with the UIDs */
QByteArray ImapModelThreadingTest::treeToThreading(QModelIndex index)
{
    QByteArray res = index.data(Imap::Mailbox::RoleMessageUid).toString().toUtf8();
    for (int i = 0; i < threadingModel->rowCount(index); ++i) {
        // We're the first child of something
        bool shallAddSpace = (i == 0) && index.isValid();
        // If there are multiple siblings (or at the top level), we're always enclosed in parentheses
        bool shallEncloseInParenteses = threadingModel->rowCount(index) > 1 || !index.isValid();
        if (shallAddSpace) {
            res += ' ';
        }
        if (shallEncloseInParenteses) {
            res += '(';
        }
        QModelIndex child = threadingModel->index(i, 0, index);
        res += treeToThreading(child);
        if (shallEncloseInParenteses) {
            res += ')';
        }
    }
    return res;
}

#define checkUidMapFromThreading(MAPPING) \
{ \
    QCOMPARE(threadingModel->rowCount(), MAPPING.size()); \
    Imap::Uids actual; \
    for (int i = 0; i < MAPPING.size(); ++i) { \
        QModelIndex messageIndex = threadingModel->index(i, 0); \
        QVERIFY(messageIndex.isValid()); \
        actual << messageIndex.data(Imap::Mailbox::RoleMessageUid).toUInt(); \
    } \
    QCOMPARE(actual, MAPPING); \
}

QByteArray ImapModelThreadingTest::numListToString(const Imap::Uids &seq)
{
    QStringList res;
    Q_FOREACH(const uint num, seq)
        res << QString::number(num);
    return res.join(QStringLiteral(" ")).toUtf8();
}

/** @short Test how sorting reacts to dynamic mailbox updates and the initial sync */
void ImapModelThreadingTest::testDynamicSorting()
{
    // keep preloading active

    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("QRESYNC"));
    injector.injectCapability(QStringLiteral("SORT=DISPLAY"));
    injector.injectCapability(QStringLiteral("SORT"));

    threadingModel->setUserWantsThreading(false);

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
    msgListModel->setMailbox(QStringLiteral("a"));
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
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

    checkUidMapFromThreading(uidMap);

    // A persistent index to make sure that these get updated properly
    QPersistentModelIndex msgUid6 = threadingModel->index(0, 0);
    QPersistentModelIndex msgUid9 = threadingModel->index(1, 0);
    QPersistentModelIndex msgUid10 = threadingModel->index(2, 0);
    QVERIFY(msgUid6.isValid());
    QVERIFY(msgUid9.isValid());
    QVERIFY(msgUid10.isValid());
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    QCOMPARE(msgUid9.data(Imap::Mailbox::RoleMessageUid).toUInt(), 9u);
    QCOMPARE(msgUid10.data(Imap::Mailbox::RoleMessageUid).toUInt(), 10u);
    QCOMPARE(msgUid6.row(), 0);
    QCOMPARE(msgUid9.row(), 1);
    QCOMPARE(msgUid10.row(), 2);

    threadingModel->setUserSearchingSortingPreference(QStringList(), Imap::Mailbox::ThreadingMsgListModel::SORT_SUBJECT);

    Imap::Uids expectedUidOrder;

    // suppose subjects are "qt", "trojita" and "mail"
    expectedUidOrder << 10 << 6 << 9;

    // A ery basic sorting example
    cClient(t.mk("UID SORT (SUBJECT) utf-8 ALL\r\n"));
    cServer("* SORT " + numListToString(expectedUidOrder) + "\r\n");
    cServer(t.last("OK sorted\r\n"));
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 1);
    QCOMPARE(msgUid9.row(), 2);
    QCOMPARE(msgUid10.row(), 0);

    // Sort by the same criteria, but in a reversed order
    threadingModel->setUserSearchingSortingPreference(QStringList(), Imap::Mailbox::ThreadingMsgListModel::SORT_SUBJECT, Qt::DescendingOrder);
    cEmpty();
    std::reverse(expectedUidOrder.begin(), expectedUidOrder.end());
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 1);
    QCOMPARE(msgUid9.row(), 0);
    QCOMPARE(msgUid10.row(), 2);

    // Revert back to ascending sort
    threadingModel->setUserSearchingSortingPreference(QStringList(), Imap::Mailbox::ThreadingMsgListModel::SORT_SUBJECT, Qt::AscendingOrder);
    cEmpty();
    std::reverse(expectedUidOrder.begin(), expectedUidOrder.end());
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 1);
    QCOMPARE(msgUid9.row(), 2);
    QCOMPARE(msgUid10.row(), 0);

    // Sort in a native order, reverse direction
    threadingModel->setUserSearchingSortingPreference(QStringList(), Imap::Mailbox::ThreadingMsgListModel::SORT_NONE, Qt::DescendingOrder);
    cEmpty();
    expectedUidOrder = uidMap;
    std::reverse(expectedUidOrder.begin(), expectedUidOrder.end());
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 2);
    QCOMPARE(msgUid9.row(), 1);
    QCOMPARE(msgUid10.row(), 0);

    // Let a new message arrive
    cServer("* 4 EXISTS\r\n");
    cClient(t.mk("UID FETCH 15:* (FLAGS)\r\n"));
    cServer("* 4 FETCH (UID 15 FLAGS ())\r\n" + t.last("ok fetched\r\n"));
    uidMap << 15;
    expectedUidOrder = uidMap;
    std::reverse(expectedUidOrder.begin(), expectedUidOrder.end());
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 3);
    QCOMPARE(msgUid9.row(), 2);
    QCOMPARE(msgUid10.row(), 1);
    // ...and delete it again
    cServer("* VANISHED 15\r\n");
    uidMap.remove(uidMap.indexOf(15));
    expectedUidOrder = uidMap;
    std::reverse(expectedUidOrder.begin(), expectedUidOrder.end());
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 2);
    QCOMPARE(msgUid9.row(), 1);
    QCOMPARE(msgUid10.row(), 0);

    // Check dynamic updates when some sorting criteria are active
    threadingModel->setUserSearchingSortingPreference(QStringList(), Imap::Mailbox::ThreadingMsgListModel::SORT_SUBJECT, Qt::AscendingOrder);
    expectedUidOrder.clear();
    expectedUidOrder << 10 << 6 << 9;
    cClient(t.mk("UID SORT (SUBJECT) utf-8 ALL\r\n"));
    cServer("* SORT " + numListToString(expectedUidOrder) + "\r\n");
    cServer(t.last("OK sorted\r\n"));
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 1);
    QCOMPARE(msgUid9.row(), 2);
    QCOMPARE(msgUid10.row(), 0);

    // ...new arrivals
    cServer("* 4 EXISTS\r\n");
    cClient(t.mk("UID FETCH 16:* (FLAGS)\r\n"));
    QByteArray delayedUidFetch = "* 4 FETCH (UID 16 FLAGS ())\r\n" + t.last("ok fetched\r\n");
    // ... their UID remains unknown for a while; the model won't request SORT just yet
    cEmpty();
    // that new arrival shall be visible immediately
    expectedUidOrder << 0;
    checkUidMapFromThreading(expectedUidOrder);
    cServer(delayedUidFetch);
    uidMap << 16;
    // as soon as the new UID arrives, the sorting order gets thrown out of the window
    expectedUidOrder = uidMap;
    checkUidMapFromThreading(expectedUidOrder);
    // at this point, the SORT gets issued
    expectedUidOrder.clear();
    expectedUidOrder << 10 << 6 << 16 << 9;
    cClient(t.mk("UID SORT (SUBJECT) utf-8 ALL\r\n"));
    cServer("* SORT " + numListToString(expectedUidOrder) + "\r\n");
    cServer(t.last("OK sorted\r\n"));

    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 1);
    QCOMPARE(msgUid9.row(), 3);
    QCOMPARE(msgUid10.row(), 0);
    // ...and delete it again
    cServer("* VANISHED 16\r\n");
    uidMap.remove(uidMap.indexOf(16));
    expectedUidOrder.remove(expectedUidOrder.indexOf(16));
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 1);
    QCOMPARE(msgUid9.row(), 2);
    QCOMPARE(msgUid10.row(), 0);

    // A new message arrives and the user requests a completely different sort order
    // Make it a bit more interesting, suddenly support ESORT as well
    injector.injectCapability(QStringLiteral("ESORT"));
    threadingModel->setUserSearchingSortingPreference(QStringList(), Imap::Mailbox::ThreadingMsgListModel::SORT_FROM, Qt::AscendingOrder);
    cServer("* 4 EXISTS\r\n");
    QByteArray sortReq = t.mk("UID SORT RETURN (ALL) (DISPLAYFROM) utf-8 ALL\r\n");
    QByteArray sortResp = t.last("OK sorted\r\n");
    QByteArray uidFetchReq = t.mk("UID FETCH 17:* (FLAGS)\r\n");
    delayedUidFetch = "* 4 FETCH (UID 17 FLAGS ())\r\n" + t.last("ok fetched\r\n");
    uidMap << 0;
    expectedUidOrder = uidMap;
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 0);
    QCOMPARE(msgUid9.row(), 1);
    QCOMPARE(msgUid10.row(), 2);

    cClient(sortReq + uidFetchReq);
    expectedUidOrder.clear();
    expectedUidOrder << 9 << 17 << 6 << 10;
    cServer("* SORT " + numListToString(expectedUidOrder) + "\r\n" + sortResp);
    // in this situation, the new arrival is not visible, unfortunately
    expectedUidOrder.remove(expectedUidOrder.indexOf(17));
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 1);
    QCOMPARE(msgUid9.row(), 0);
    QCOMPARE(msgUid10.row(), 2);
    // Deliver the UID; it will get listed as the last item. Now because we do cache the raw (UID-based) form of SORT/SEARCH,
    // the last SORT result will be reused.
    // Previously (when SORT responses weren't cached), this would require a asking for SORT once again; that is no longer
    // necessary.
    cServer(delayedUidFetch);
    uidMap.remove(uidMap.indexOf(0));
    uidMap << 17;
    // The sorted result previously didn't contain the missing UID, so we'll refill the expected order now
    expectedUidOrder.clear();
    expectedUidOrder << 9 << 17 << 6 << 10;
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 2);
    QCOMPARE(msgUid9.row(), 0);
    QCOMPARE(msgUid10.row(), 3);

    cEmpty();
    justKeepTask();
}

void ImapModelThreadingTest::testDynamicSortingContext()
{
    // keep preloading active

    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("QRESYNC"));
    injector.injectCapability(QStringLiteral("SORT"));
    injector.injectCapability(QStringLiteral("ESORT"));
    injector.injectCapability(QStringLiteral("CONTEXT=SORT"));

    threadingModel->setUserWantsThreading(false);

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
    msgListModel->setMailbox(QStringLiteral("a"));
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
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

    checkUidMapFromThreading(uidMap);

    // A persistent index to make sure that these get updated properly
    QPersistentModelIndex msgUid6 = threadingModel->index(0, 0);
    QPersistentModelIndex msgUid9 = threadingModel->index(1, 0);
    QPersistentModelIndex msgUid10 = threadingModel->index(2, 0);
    QVERIFY(msgUid6.isValid());
    QVERIFY(msgUid9.isValid());
    QVERIFY(msgUid10.isValid());
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    QCOMPARE(msgUid9.data(Imap::Mailbox::RoleMessageUid).toUInt(), 9u);
    QCOMPARE(msgUid10.data(Imap::Mailbox::RoleMessageUid).toUInt(), 10u);
    QCOMPARE(msgUid6.row(), 0);
    QCOMPARE(msgUid9.row(), 1);
    QCOMPARE(msgUid10.row(), 2);

    threadingModel->setUserSearchingSortingPreference(QStringList(), Imap::Mailbox::ThreadingMsgListModel::SORT_SUBJECT);

    Imap::Uids expectedUidOrder;

    // suppose subjects are "qt", "trojita" and "mail"
    expectedUidOrder << 10 << 6 << 9;

    // A ery basic sorting example
    cClient(t.mk("UID SORT RETURN (ALL UPDATE) (SUBJECT) utf-8 ALL\r\n"));
    QByteArray sortTag(t.last());
    cServer("* SORT " + numListToString(expectedUidOrder) + "\r\n");
    cServer(t.last("OK sorted\r\n"));
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 1);
    QCOMPARE(msgUid9.row(), 2);
    QCOMPARE(msgUid10.row(), 0);

    // Sort by the same criteria, but in a reversed order
    threadingModel->setUserSearchingSortingPreference(QStringList(), Imap::Mailbox::ThreadingMsgListModel::SORT_SUBJECT, Qt::DescendingOrder);
    cEmpty();
    std::reverse(expectedUidOrder.begin(), expectedUidOrder.end());
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 1);
    QCOMPARE(msgUid9.row(), 0);
    QCOMPARE(msgUid10.row(), 2);

    // Delivery of a new item
    cServer("* 4 EXISTS\r\n* ESEARCH (TAG \"" + sortTag + "\") UID ADDTO (0 15)\r\n");
    cClient(t.mk("UID FETCH 15:* (FLAGS)\r\n"));
    cServer("* 4 FETCH (UID 15 FLAGS ())\r\n" + t.last("ok fetched\r\n"));
    cEmpty();

    // We're still showing the reversed list
    expectedUidOrder << 15;
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 1);
    QCOMPARE(msgUid9.row(), 0);
    QCOMPARE(msgUid10.row(), 2);

    // Remove one message
    cServer("* ESEARCH (TAG \"" + sortTag + "\") UID REMOVEFROM (4 9)\r\n");
    cServer("* VANISHED 9\r\n");
    expectedUidOrder.remove(expectedUidOrder.indexOf(9));
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 0);
    QVERIFY(!msgUid9.isValid());
    QCOMPARE(msgUid10.row(), 1);

    // Deliver a few more messages to give removals a decent test
    cServer("* 6 EXISTS\r\n");
    cClient(t.mk("UID FETCH 16:* (FLAGS)\r\n"));
    QByteArray uidFetchResp = "* 4 FETCH (UID 16 FLAGS ())\r\n"
            "* 6 FETCH (UID 18 FLAGS ())\r\n"
            "* 5 FETCH (UID 17 FLAGS ())\r\n" + t.last("OK fetched\r\n");

    // At the same time, request a different sorting criteria
    threadingModel->setUserSearchingSortingPreference(QStringList(), Imap::Mailbox::ThreadingMsgListModel::SORT_CC, Qt::AscendingOrder);

    QByteArray cancelReq = t.mk(QByteArray("CANCELUPDATE \"" + sortTag + "\"\r\n"));
    QByteArray cancelResponse = t.last("OK no more updates for you\r\n");
    cClient(cancelReq + t.mk("UID SORT RETURN (ALL UPDATE) (CC) utf-8 ALL\r\n"));
    sortTag = t.last();
    cServer("* ESEARCH (TAG \"" + sortTag + "\") UID ALL 15:17,6,18,10\r\n");
    cServer(cancelResponse + t.last("OK sorted\r\n"));
    cEmpty();

    expectedUidOrder.clear();
    expectedUidOrder << 15 << 6 << 10;
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);

    // deliver the UIDs
    cServer(uidFetchResp);
    expectedUidOrder.clear();
    expectedUidOrder << 15 << 16 << 17 << 6 << 18 << 10;
    checkUidMapFromThreading(expectedUidOrder);

    // Remove a message, now through a response without an explicit offset
    cServer("* ESEARCH (TAG \"" + sortTag + "\") UID REMOVEFROM (0 17)\r\n");
    expectedUidOrder.remove(expectedUidOrder.indexOf(17));
    checkUidMapFromThreading(expectedUidOrder);

    // Try to push it back now
    cServer("* ESEARCH (TAG \"" + sortTag + "\") UID ADDTO (2 17)\r\n");
    expectedUidOrder.clear();
    expectedUidOrder << 15 << 17 << 16 << 6 << 18 << 10;
    checkUidMapFromThreading(expectedUidOrder);

    // Insert one message at the end
    cServer("* ESEARCH (TAG \"" + sortTag + "\") UID ADDTO (7 18)\r\n");
    expectedUidOrder << 18;
    checkUidMapFromThreading(expectedUidOrder);

    model->switchToMailbox(idxB);
    cClient(t.mk(QByteArray("CANCELUPDATE \"" + sortTag + "\"\r\n")));
    cServer(t.last("OK no further updates\r\n"));
    cClient(t.mk("SELECT b\r\n"));
    cServer("* OK [CLOSED] previous mailbox closed\r\n* 0 EXISTS\r\n" + t.last("OK selected\r\n"));

    cEmpty();
    justKeepTask();

    // FIXME: finalize me -- test the incrmeental updates
    // FIXME: also test behavior when we get "* NO [NOUPDATE "tag"] ..."
}

/** @short Test how filtering (searching) works */
void ImapModelThreadingTest::testDynamicSearch()
{
    // keep preloading active

    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("QRESYNC"));

    threadingModel->setUserWantsThreading(false);

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
    msgListModel->setMailbox(QStringLiteral("a"));
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
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

    checkUidMapFromThreading(uidMap);

    // A persistent index to make sure that these get updated properly
    QPersistentModelIndex msgUid6 = threadingModel->index(0, 0);
    QPersistentModelIndex msgUid9 = threadingModel->index(1, 0);
    QPersistentModelIndex msgUid10 = threadingModel->index(2, 0);
    QVERIFY(msgUid6.isValid());
    QVERIFY(msgUid9.isValid());
    QVERIFY(msgUid10.isValid());
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    QCOMPARE(msgUid9.data(Imap::Mailbox::RoleMessageUid).toUInt(), 9u);
    QCOMPARE(msgUid10.data(Imap::Mailbox::RoleMessageUid).toUInt(), 10u);
    QCOMPARE(msgUid6.row(), 0);
    QCOMPARE(msgUid9.row(), 1);
    QCOMPARE(msgUid10.row(), 2);

    threadingModel->setUserSearchingSortingPreference(QStringList() << QStringLiteral("SUBJECT") << QStringLiteral("foo"),
                                                      threadingModel->currentSortCriterium(), threadingModel->currentSortOrder());

    Imap::Uids expectedUidOrder;

    expectedUidOrder << 6 << 9 << 10;

    // A very basic searching example
    cClient(t.mk("UID SEARCH CHARSET utf-8 SUBJECT foo\r\n"));
    cServer("* SEARCH " + numListToString(expectedUidOrder) + "\r\n");
    cServer(t.last("OK sorted\r\n"));
    QCOMPARE(msgUid6.data(Imap::Mailbox::RoleMessageUid).toUInt(), 6u);
    checkUidMapFromThreading(expectedUidOrder);
    QCOMPARE(msgUid6.row(), 0);
    QCOMPARE(msgUid9.row(), 1);
    QCOMPARE(msgUid10.row(), 2);

    // Enable ESEARCH
    injector.injectCapability(QStringLiteral("ESEARCH"));

    // Try a "different" search
    threadingModel->setUserSearchingSortingPreference(QStringList() << QStringLiteral("SUBJECT") << QStringLiteral("blah"),
                                                      threadingModel->currentSortCriterium(), threadingModel->currentSortOrder());
    expectedUidOrder.clear();
    expectedUidOrder << 9;

    cClient(t.mk("UID SEARCH RETURN (ALL) CHARSET utf-8 SUBJECT blah\r\n"));
    QByteArray searchTag = t.last();
    cServer("* ESEARCH (TAG \"" + searchTag + "\") UID ALL 9\r\n" + t.last("OK searched\r\n"));
    checkUidMapFromThreading(expectedUidOrder);

    // Try yet another search, this time something which yields an empty result
    threadingModel->setUserSearchingSortingPreference(QStringList() << QStringLiteral("SUBJECT") << QStringLiteral("foobar"),
                                                      threadingModel->currentSortCriterium(), threadingModel->currentSortOrder());
    expectedUidOrder.clear();
    cClient(t.mk("UID SEARCH RETURN (ALL) CHARSET utf-8 SUBJECT foobar\r\n"));
    searchTag = t.last();
    cServer("* ESEARCH (TAG \"" + searchTag + "\") UID\r\n" + t.last("OK searched\r\n"));
    checkUidMapFromThreading(expectedUidOrder);

    // Try to go back to no filtering
    threadingModel->setUserSearchingSortingPreference(QStringList(), threadingModel->currentSortCriterium(), threadingModel->currentSortOrder());
    expectedUidOrder = uidMap;
    checkUidMapFromThreading(expectedUidOrder);

    // FIXME: check threading & searching combo
    // FIXME: check sorting & searching combo
    // FIXME: check threading & sorting & searching combo
    // FIXME: check incremental updates
    // FIXME: finalize me

    cEmpty();
    justKeepTask();
}

QByteArray ImapModelThreadingTest::prepareHugeUntaggedThread(const uint num)
{
    QString sampleThread = QStringLiteral("(%1 (%2 %3 (%4)(%5 %6 %7))(%8 %9 %10))");
    QString linearThread = QStringLiteral("(%1 %2 %3 %4 %5 %6 %7 %8 %9 %10)");
    QString flatThread = QStringLiteral("(%1 (%2)(%3)(%4)(%5)(%6)(%7)(%8)(%9)(%10))");
    Q_ASSERT(num % 10 == 0);
    QString response = QStringLiteral("* THREAD ");
    for (uint i = 1; i < num; i += 10) {
        QString *format = 0;
        switch (i % 100) {
        case 1:
        case 11:
        case 21:
        case 31:
        case 41:
            format = &sampleThread;
            break;
        case 51:
        case 61:
            format = &linearThread;
            break;
        case 71:
        case 81:
        case 91:
            format = &flatThread;
            break;
        }
        Q_ASSERT(format);
        response += format->arg(QString::number(i), QString::number(i+1), QString::number(i+2), QString::number(i+3),
                                QString::number(i+4), QString::number(i+5), QString::number(i+6), QString::number(i+7),
                                QString::number(i+8)).arg(QString::number(i+9));
    }
    response += QLatin1String("\r\n");
    return response.toUtf8();
}

void ImapModelThreadingTest::testThreadingPerformance()
{
    const uint num = 100000;
    initialMessages(num);
    QByteArray untaggedThread = prepareHugeUntaggedThread(num);
    QBENCHMARK {
        QCOMPARE(SOCK->writtenStuff(), t.mk("UID THREAD REFS utf-8 ALL\r\n"));
        SOCK->fakeReading(untaggedThread + t.last("OK thread\r\n"));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        model->cache()->setMessageThreading(QStringLiteral("a"), QVector<Imap::Responses::ThreadingNode>());
        threadingModel->wantThreading();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
    }
}

void ImapModelThreadingTest::testSortingPerformance()
{
    threadingModel->setUserWantsThreading(false);

    using namespace Imap::Mailbox;

    const int num = 100000;
    initialMessages(num);

    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("QRESYNC"));
    injector.injectCapability(QStringLiteral("SORT=DISPLAY"));
    injector.injectCapability(QStringLiteral("SORT"));

    QStringList sortOrder;
    int i = 0;
    while (i < num / 2) {
        sortOrder << QString::number(num / 2 + 1 + i);
        ++i;
    }
    while (i < num) {
        sortOrder << QString::number(i - num / 2);
        ++i;
    }
    QCOMPARE(sortOrder.size(), num);
    QByteArray resp = ("* SORT " + sortOrder.join(QStringLiteral(" ")) + "\r\n").toUtf8();

    QBENCHMARK {
        threadingModel->setUserSearchingSortingPreference(QStringList(), ThreadingMsgListModel::SORT_NONE, Qt::AscendingOrder);
        threadingModel->setUserSearchingSortingPreference(QStringList(), ThreadingMsgListModel::SORT_NONE, Qt::DescendingOrder);
    }

    bool flag = false;
    QBENCHMARK {
        ThreadingMsgListModel::SortCriterium criterium = flag ? ThreadingMsgListModel::SORT_SUBJECT : ThreadingMsgListModel::SORT_CC;
        Qt::SortOrder order = flag ? Qt::AscendingOrder : Qt::DescendingOrder;
        threadingModel->setUserSearchingSortingPreference(QStringList(), criterium, order);
        if (flag) {
            cClient(t.mk("UID SORT (SUBJECT) utf-8 ALL\r\n"));
        } else {
            cClient(t.mk("UID SORT (CC) utf-8 ALL\r\n"));
        }
        flag = !flag;
        cServer(resp);
        cServer(t.last("OK sorted\r\n"));
    }
}

void ImapModelThreadingTest::testSearchingPerformance()
{
    threadingModel->setUserWantsThreading(false);

    using namespace Imap::Mailbox;

    const int num = 100000;
    initialMessages(num);

    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("QRESYNC"));

    threadingModel->setUserSearchingSortingPreference(QStringList(), ThreadingMsgListModel::SORT_NONE, Qt::DescendingOrder);
    /*cClient(t.mk("UID THREAD REFS utf-8 ALL\r\n"));
    QByteArray untaggedThread = prepareHugeUntaggedThread(num);
    cServer(untaggedThread + t.last("OK thread\r\n"));*/

    Imap::Uids result;
    result << 1 << 5 << 59 << 666;
    QStringList buf;
    Q_FOREACH(const int uid, result)
        buf << QString::number(uid);
    QByteArray sortResult = buf.join(QStringLiteral(" ")).toUtf8();

    QBENCHMARK {
        threadingModel->setUserSearchingSortingPreference(QStringList() << QStringLiteral("SUBJECT") << QStringLiteral("x"),
                                                          ThreadingMsgListModel::SORT_NONE, Qt::AscendingOrder);
        cClient(t.mk("UID SEARCH CHARSET utf-8 SUBJECT x\r\n"));
        cServer("* SEARCH " + sortResult + "\r\n");
        cServer(t.last("OK sorted\r\n"));
    }
    QCOMPARE(threadingModel->rowCount(), result.size());
    for (int i = 0; i < result.size(); ++i) {
        QModelIndex index = threadingModel->index(i, 0);
        QVERIFY(index.isValid());
        QCOMPARE(index.data(Imap::Mailbox::RoleMessageUid).toUInt(), result[i]);
    }
}

/** @short Test that the INCTHREAD extension works as advertized */
void ImapModelThreadingTest::testIncrementalThreading()
{
    initialMessages(10);

    // A complex nested hierarchy with nodes to be promoted
    Mapping mapping;
    QByteArray response;
    complexMapping(mapping, response);

    cClient(t.mk("UID THREAD REFS utf-8 ALL\r\n"));
    cServer(QByteArray("* THREAD ") + response + QByteArray("\r\n") + t.last("OK thread\r\n"));
    verifyMapping(mapping);
    QCOMPARE(threadingModel->rowCount(QModelIndex()), 4);
    IndexMapping indexMap = buildIndexMap(mapping);
    verifyIndexMap(indexMap, mapping);
    // The response is actually slightly different, but never mind (extra parentheses around 7)
    QCOMPARE(treeToThreading(QModelIndex()), QByteArray("(1)(2 3)(4 (5)(6))(7 (8)(9 10))"));

    // Activate support for the INCTHREAD extension
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("ETHREAD"));
    injector.injectCapability(QStringLiteral("INCTHREAD"));

    // Fake delivery of one new message
    cServer("* 11 EXISTS\r\n");
    // Ask for the UID and deliver it immediately
    cClient(t.mk("UID FETCH 11:* (FLAGS)\r\n"));
    cServer("* 11 FETCH (UID 11 FLAGS ())\r\n" + t.last("OK fetch\r\n"));

    // Test the incremental threading
    cClient(t.mk("UID THREAD RETURN (INCTHREAD) REFS utf-8 INTHREAD REFS UID 11:*\r\n"));
    // Yes, it's a rather funky response
    cServer("* ESEARCH (TAG \"" + t.last() + "\") UID INCTHREAD 2 (7 (8 9 11)(10))\r\n");
    QCOMPARE(treeToThreading(QModelIndex()), QByteArray("(1)(2 3)(7 (8 9 11)(10))(4 (5)(6))"));
    cServer(t.last("OK done\r\n"));

    cEmpty();
}

/** Test what happens when a thread root ceases to exist while the THREAD response is in flight */
void ImapModelThreadingTest::testRemovingRootWithThreadingInFlight()
{
    // At first, add two standalone threads
    initialMessages(2);
    Mapping mapping;
    mapping.clear();
    mapping[QStringLiteral("0")] = 1;
    mapping[QStringLiteral("0.0")] = 0;
    mapping[QStringLiteral("1")] = 2;
    mapping[QStringLiteral("1.0")] = 0;
    mapping[QStringLiteral("2")] = 0;
    cClient(t.mk("UID THREAD REFS utf-8 ALL\r\n"));
    cServer(QByteArray("* THREAD (1)(2)\r\n") + t.last("OK thread\r\n"));
    verifyMapping(mapping);
    QCOMPARE(threadingModel->rowCount(QModelIndex()), 2);
    verifyIndexMap(buildIndexMap(mapping), mapping);
    QCOMPARE(treeToThreading(QModelIndex()), QByteArray("(1)(2)"));

    // Now let one more message arrive
    cServer("* 3 EXISTS\r\n");
    cClient(t.mk("UID FETCH 3:* (FLAGS)\r\n"));
    QByteArray fetchUntagged("* 3 FETCH (UID 3 FLAGS ())\r\n");
    QByteArray fetchTagged(t.last("OK fetched\r\n"));
    cServer(fetchUntagged);
    cServer(fetchTagged);
    // While the threading is requested, one thread root gets removed
    cClient(t.mk("UID THREAD REFS utf-8 ALL\r\n"));
    QByteArray threadUntagged("* THREAD (1)(2 3)\r\n");
    QByteArray threadTagged(t.last("OK thread\r\n"));
    cServer(threadUntagged);
    // The important bit is EXPUNGE prior to the tagged OK for the threading. This is a difference which matters.
    cServer("* 2 EXPUNGE\r\n");
    cServer(threadTagged);
    QCOMPARE(QString::fromUtf8(treeToThreading(QModelIndex())), QString::fromUtf8("(1)(3)"));
    mapping.clear();
    mapping[QStringLiteral("0")] = 1;
    mapping[QStringLiteral("0.0")] = 0;
    mapping[QStringLiteral("1")] = 3;
    mapping[QStringLiteral("1.0")] = 0;
    mapping[QStringLiteral("2")] = 0;
    verifyMapping(mapping);
    QCOMPARE(threadingModel->rowCount(QModelIndex()), 2);
    verifyIndexMap(buildIndexMap(mapping), mapping);
    cEmpty();
}

/** @short Check that multiple messages being removed at once doesn't break stuff */
void ImapModelThreadingTest::testMultipleExpunges()
{
    initialMessages(4);
    Mapping mapping;
    mapping[QStringLiteral("0")] = 1;
    mapping[QStringLiteral("0.0")] = 0;
    mapping[QStringLiteral("1")] = 2;
    mapping[QStringLiteral("1.0")] = 0;
    mapping[QStringLiteral("2")] = 3;
    mapping[QStringLiteral("2.0")] = 0;
    mapping[QStringLiteral("3")] = 4;
    mapping[QStringLiteral("3.0")] = 0;
    mapping[QStringLiteral("4")] = 0;
    cClient(t.mk("UID THREAD REFS utf-8 ALL\r\n"));
    cServer(QByteArray("* THREAD (1)(2)(3)(4)\r\n") + t.last("OK thread\r\n"));
    verifyMapping(mapping);
    QCOMPARE(threadingModel->rowCount(QModelIndex()), 4);
    verifyIndexMap(buildIndexMap(mapping), mapping);
    QCOMPARE(treeToThreading(QModelIndex()), QByteArray("(1)(2)(3)(4)"));

    QPersistentModelIndex m1 = findItem(QStringLiteral("0"));
    QPersistentModelIndex m2 = findItem(QStringLiteral("1"));
    QPersistentModelIndex m4 = findItem(QStringLiteral("3"));
    helper_multipleExpunges_hit = 0;

    QVERIFY(m1.isValid());
    QVERIFY(m2.isValid());
    QVERIFY(!helper_indexMultipleExpunges_1.isValid());
    QVERIFY(m4.isValid());

    // The tricky part is here. The bug we want to test for was this: some code within QItemSelectionModel (?)
    // grabbed a new persistent index most likely within a slot tied to layoutAboutToBeChanged signal. This persistent
    // index was, however, not updated by the delayedPrune method, and therefore it was left dangling. In the GUI, this
    // was apparent because some items were suddenly getting selected after some other messages were removed, and the visual
    // position of the now bogous selection was conspicuously similar to the expunged messages.
    connect(threadingModel, &QAbstractItemModel::layoutAboutToBeChanged, this, &ImapModelThreadingTest::helper_multipleExpunges);
    cServer("* 2 EXPUNGE\r\n* 2 EXPUNGE\r\n");
    disconnect(threadingModel, &QAbstractItemModel::layoutAboutToBeChanged, this, &ImapModelThreadingTest::helper_multipleExpunges);
    QCOMPARE(helper_multipleExpunges_hit, 1);

    QCOMPARE(QString::fromUtf8(treeToThreading(QModelIndex())), QString::fromUtf8("(1)(4)"));
    QCOMPARE(threadingModel->rowCount(QModelIndex()), 2);
    QVERIFY(m1.isValid());
    QVERIFY(!m2.isValid());
    QVERIFY(!helper_indexMultipleExpunges_1.isValid());
    QVERIFY(m4.isValid());

    cEmpty();
}

void ImapModelThreadingTest::helper_multipleExpunges()
{
    if (helper_multipleExpunges_hit == -1) {
        // hack: don't let the QTestLib "run" this method in a standalone manner
        return;
    }
    helper_indexMultipleExpunges_1 = findItem(QStringLiteral("2"));
    QVERIFY(helper_indexMultipleExpunges_1.isValid());
    ++helper_multipleExpunges_hit;
}

/** @short Check how fast it is to delete a substantial number of e-mails from the mailbox using flat threading */
void ImapModelThreadingTest::testFlatThreadDeletionPerformance()
{
    threadingModel->setUserWantsThreading(false);
    // only send NOOPs after a day; the default timeout of two minutes is way too short for valgrind's callgrind
    model->setProperty("trojita-imap-noop-period", 24 * 60 * 60 * 1000);

    const int num = 30000; // 30k messages translate into roughly 3-5s, which is acceptable
    initialMessages(num);
    auto numDeletes = num / 2;

    // perform the deletes in the middle
    QByteArray deletes = QByteArray("* " + QByteArray::number(num - numDeletes - 5) + " EXPUNGE\r\n").repeated(numDeletes);

    QSignalSpy layoutChanged(threadingModel, SIGNAL(layoutChanged()));

    QBENCHMARK_ONCE {
        cServer(deletes);
        // make sure all events are delivered; the model scheduler processes them on a 100-sized chunks
        // plus add one for actual processing of the delayed delete
        for (auto i = 0; i < numDeletes / 100 + 1; ++i) {
            QCoreApplication::processEvents();
        }
    }
    QCOMPARE(model->rowCount(msgListB), 0);
    QVERIFY(layoutChanged.size() >= 1);
    QCOMPARE(threadingModel->rowCount(), num - numDeletes);
    QCOMPARE(model->rowCount(msgListA), num - numDeletes);

    // Make sure that the mailbox switchover won't cause any events to get lost.
    // This should flush the delayed timer unconditionally.
    cClient(t.mk("SELECT b\r\n"));
    cServer("* 0 EXISTS\r\n* OK [UIDVALIDITY 666]  \r\n* OK [UIDNEXT 1]  \r\n" + t.last("OK selected\r\n"));

    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState(QLatin1String("a")).exists()), num - numDeletes);
    justKeepTask();
    cEmpty();
}

/** @short Make sure that changes of a yet unsynced message doesn't confuse us */
void ImapModelThreadingTest::testDataChangedUnknownUid()
{
    initialMessages(1);
    cServer("* 1 FETCH (FLAGS ())\r\n");
    cClient(t.mk("UID THREAD REFS utf-8 ALL\r\n"));
    cServer("* THREAD (1)\r\n" + t.last("OK thread\r\n"));
    justKeepTask();
    cEmpty();

    model->markMailboxAsRead(idxA);
    cClient(t.mk("STORE 1:* +FLAGS.SILENT \\Seen\r\n"));
    cServer("* 2 EXISTS\r\n* 2 FETCH (FLAGS (pwn))\r\n" + t.last("OK marked\r\n"));
    cClient(t.mk("UID FETCH 2:* (FLAGS)\r\n"));
    cServer("* 2 FETCH (UID 1002 FLAGS (ahoy))\r\n" + t.last("OK done\r\n"));
    cClient(t.mk("UID THREAD REFS utf-8 ALL\r\n"));
    cServer("* THREAD (1)(1002)\r\n" + t.last("OK thread\r\n"));
    QCOMPARE(model->cache()->msgFlags(QLatin1String("a"), 1002), QStringList() << QLatin1String("ahoy"));
    justKeepTask();
    cEmpty();
}

/** @short Verify parsing of various ESEARCH return results */
void ImapModelThreadingTest::testESearchResults()
{
    using namespace Imap::Mailbox;
    threadingModel->setUserWantsThreading(false);
    initialMessages(1);
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("QRESYNC"));
    injector.injectCapability(QStringLiteral("ESEARCH"));

    // An empty result, Dovecot style
    threadingModel->setUserSearchingSortingPreference(QStringList() << QStringLiteral("SUBJECT") << QStringLiteral("x"),
                                                      ThreadingMsgListModel::SORT_NONE, Qt::AscendingOrder);
    cClient(t.mk("UID SEARCH RETURN (ALL) CHARSET utf-8 SUBJECT x\r\n"));
    // Dovecot sends the UID response, as expected
    cServer("* ESEARCH (TAG \"" + t.last() + "\") UID \r\n");
    cServer(t.last("OK searched\r\n"));
    QCOMPARE(threadingModel->rowCount(), 0);

    // Some extra data in the ESEARCH response -- just to make sure that the code doesn't expect a fixed position
    // of the ALL data set
    threadingModel->setUserSearchingSortingPreference(QStringList() << QStringLiteral("SUBJECT") << QStringLiteral("y"),
                                                      ThreadingMsgListModel::SORT_NONE, Qt::AscendingOrder);
    cClient(t.mk("UID SEARCH RETURN (ALL) CHARSET utf-8 SUBJECT y\r\n"));
    // Check if random crap in these resplies doesn't break stuff
    cServer("* ESEARCH (TAG \"" + t.last() + "\") UID random0 0 Random00 0 ALL 1 random1 666 RANDOM2 333\r\n");
    cServer(t.last("OK searched\r\n"));
    QCOMPARE(threadingModel->rowCount(), 1);

    // Empty result, Cyrus 2.9.17-style
    threadingModel->setUserSearchingSortingPreference(QStringList() << QStringLiteral("SUBJECT") << QStringLiteral("z"),
                                                      ThreadingMsgListModel::SORT_NONE, Qt::AscendingOrder);
    cClient(t.mk("UID SEARCH RETURN (ALL) CHARSET utf-8 SUBJECT z\r\n"));
    // Cyrus, however, omits the UID part of the response
    // https://bugs.kde.org/show_bug.cgi?id=350698
    // https://bugzilla.cyrusimap.org/show_bug.cgi?id=3900
    cServer("* ESEARCH (TAG \"" + t.last() + "\")\r\n");
    cServer(t.last("OK searched\r\n"));
    QCOMPARE(threadingModel->rowCount(), 0);
}

QTEST_GUILESS_MAIN( ImapModelThreadingTest )
