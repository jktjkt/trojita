/* Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

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
#include <QStandardItemModel>
#include <QTest>
#include "test_QaimDfsIterator.h"
#include "Common/StashingReverseIterator.h"
#include "UiUtils/QaimDfsIterator.h"

#if defined(__has_feature)
#  if  __has_feature(address_sanitizer)
    // better would be checking for UBSAN, but I don't think that there's an appropriate feature for this
#    define SKIP_QSTANDARDITEMMODEL
#  endif
#endif

void TestQaimDfsIterator::testQaimDfsIterator()
{
#ifdef SKIP_QSTANDARDITEMMODEL
    QSKIP("ASAN build, https://bugreports.qt.io/browse/QTBUG-56027");
#else
    QFETCH(QString, order);
    QFETCH(QSharedPointer<QStandardItemModel>, model);
    QFETCH(UiUtils::QaimDfsIterator, begin);
    QFETCH(UiUtils::QaimDfsIterator, end);

    QStringList buf;
    std::transform(begin, end, std::back_inserter(buf),
                   [](const QModelIndex &what) -> QString {
        return what.data().toString();
    });
    QCOMPARE(buf.join(QLatin1Char(' ')), order);

    // Check that operator++ can be reversed (while it points to a valid element or to end)
    for (auto it = begin; it != end; ++it) {
        auto another = it;
        ++it;
        --it;
        QVERIFY(it == another);
        auto reverse = Common::stashing_reverse_iterator<decltype(it)>(it);
        --reverse;
        QVERIFY(reverse->row() == it->row());
        QVERIFY(reverse->internalPointer() == it->internalPointer());
    }

    if (begin != end) {
        // check iteration in reverse
        QStringList reversed;
        auto it = end;
        do {
            --it;
            reversed.push_back(it->data().toString());
        } while (it != begin);
        std::reverse(reversed.begin(), reversed.end());
        QCOMPARE(reversed.join(QLatin1Char(' ')), order);

        // check that we won't wrap around
        if (*begin == model->index(0, 0, QModelIndex())) {
            --it;
            QVERIFY(!it->isValid());
            --it;
            QVERIFY(!it->isValid());
        }

        // now check using an adapted version of STL's reverse_iterator
        reversed.clear();
        Common::stashing_reverse_iterator<UiUtils::QaimDfsIterator> rbegin(end), rend(begin);
        std::transform(rbegin, rend, std::back_inserter(reversed),
                       [](const QModelIndex &what) -> QString {
            return what.data().toString();
        });
        std::reverse(reversed.begin(), reversed.end());
        QCOMPARE(reversed.join(QLatin1Char(' ')), order);
    } else {
        auto it = end;
        --it;
        ++it;
        QVERIFY(it == end);
        QVERIFY(!(it != end));
        QVERIFY(it == begin);
        QVERIFY(!(it != begin));
    }

    if (order.isEmpty()) {
        QCOMPARE(std::distance(begin, end), 0);
    } else {
        QCOMPARE(std::distance(begin, end), order.count(QLatin1Char(' ')) + 1);
    }
#endif
}

void TestQaimDfsIterator::testQaimDfsIterator_data()
{
#ifdef SKIP_QSTANDARDITEMMODEL
    QSKIP("ASAN build, https://bugreports.qt.io/browse/QTBUG-56027");
#else
    QTest::addColumn<QString>("order");
    QTest::addColumn<QSharedPointer<QStandardItemModel>>("model");
    QTest::addColumn<UiUtils::QaimDfsIterator>("begin");
    QTest::addColumn<UiUtils::QaimDfsIterator>("end");
    QSharedPointer<QStandardItemModel> m;
    UiUtils::QaimDfsIterator begin, end;

    m.reset(new QStandardItemModel());
    begin = UiUtils::QaimDfsIterator();
    Q_ASSERT(!begin->isValid());
    end = UiUtils::QaimDfsIterator(QModelIndex(), m.data());
    Q_ASSERT(!end->isValid());
    QTest::newRow("empty-model") << QString() << m << begin << end;

    m.reset(new QStandardItemModel());
    m->appendRow(new QStandardItem("a"));
    begin = UiUtils::QaimDfsIterator(m->index(0, 0, QModelIndex()));
    Q_ASSERT(begin->data().toString() == "a");
    end = UiUtils::QaimDfsIterator(QModelIndex(), m.data());
    Q_ASSERT(!end->isValid());
    QTest::newRow("one-item") << "a" << m << begin << end;

    m.reset(new QStandardItemModel());
    m->appendRow(new QStandardItem("a"));
    m->appendRow(new QStandardItem("b"));
    m->appendRow(new QStandardItem("c"));
    begin = UiUtils::QaimDfsIterator(m->index(0, 0, QModelIndex()));
    Q_ASSERT(begin->data().toString() == "a");
    end = UiUtils::QaimDfsIterator(QModelIndex(), m.data());
    Q_ASSERT(!end->isValid());
    QTest::newRow("flat-list") << "a b c" << m << begin << end;

    m.reset(new QStandardItemModel());
    auto item3 = new QStandardItem("a.A.1");
    auto item2 = new QStandardItem("a.A");
    item2->appendRow(item3);
    auto item1 = new QStandardItem("a");
    item1->appendRow(item2);
    m->appendRow(item1);
    item1 = item2 = item3 = nullptr;
    begin = UiUtils::QaimDfsIterator(m->index(0, 0, QModelIndex()));
    Q_ASSERT(begin->data().toString() == "a");
    end = UiUtils::QaimDfsIterator(QModelIndex(), m.data());
    Q_ASSERT(!end->isValid());
    QTest::newRow("linear-hierarchy") << "a a.A a.A.1" << m << begin << end;

    m.reset(new QStandardItemModel());
    item3 = new QStandardItem("a.A.1");
    item2 = new QStandardItem("a.A");
    item2->appendRow(item3);
    item1 = new QStandardItem("a");
    item1->appendRow(item2);
    item3 = new QStandardItem("a.B.1");
    item2 = new QStandardItem("a.B");
    item2->appendRow(item3);
    item1->appendRow(item2);
    m->appendRow(item1);
    item1 = new QStandardItem("b");
    m->appendRow(item1);
    begin = UiUtils::QaimDfsIterator(m->index(0, 0, QModelIndex()));
    Q_ASSERT(begin->data().toString() == "a");
    end = UiUtils::QaimDfsIterator(QModelIndex(), m.data());
    Q_ASSERT(!end->isValid());
    QTest::newRow("backtracking") << "a a.A a.A.1 a.B a.B.1 b" << m << begin << end;

    m.reset(new QStandardItemModel());
    item3 = new QStandardItem("a.A.1");
    item2 = new QStandardItem("a.A");
    item2->appendRow(item3);
    item1 = new QStandardItem("a");
    item1->appendRow(item2);
    item3 = new QStandardItem("a.B.1");
    item2 = new QStandardItem("a.B");
    item2->appendRow(item3);
    item1->appendRow(item2);
    m->appendRow(item1);
    item1 = new QStandardItem("b");
    m->appendRow(item1);
    begin = UiUtils::QaimDfsIterator(m->index(0, 0, QModelIndex()).child(0, 0));
    Q_ASSERT(begin->data().toString() == "a.A");
    end = UiUtils::QaimDfsIterator(m->index(1, 0, QModelIndex()));
    Q_ASSERT(end->data().toString() == "b");
    QTest::newRow("backtracking-until-top-level") << "a.A a.A.1 a.B a.B.1" << m << begin << end;
#endif
}

void TestQaimDfsIterator::testWrappedFind()
{
#ifdef SKIP_QSTANDARDITEMMODEL
    QSKIP("ASAN build, https://bugreports.qt.io/browse/QTBUG-56027");
#else
    QFETCH(QString, items);
    QFETCH(QString, commands);

    QStandardItemModel model;
    const auto splitItems = items.split(QLatin1Char(' '));
    for (const auto &i: splitItems) {
        model.appendRow(new QStandardItem(i));
    }
    const auto cmdStream = commands.split(QLatin1Char(' '));
    QVERIFY(cmdStream.size() % 2 == 0);

    QModelIndex currentItem;

    auto matcher = [](const QModelIndex &idx) { return idx.data().toString().contains(QLatin1Char('_')); };

    QStringList foundValues, expectedValues;

    for (int i = 0; i < cmdStream.size() - 1; ++i) {
        bool numberOk;

        auto positiveAction = [&currentItem, cmdStream, &i, &foundValues, &expectedValues](const QModelIndex &idx) {
            currentItem = idx;
            foundValues.push_back(idx.data().toString());
            expectedValues.push_back(cmdStream[i] + "_");
            QCOMPARE(foundValues, expectedValues);
        };

        auto negativeAction = [&currentItem, cmdStream, &i, &foundValues, &expectedValues]() {
            if (currentItem.isValid()) {
                // ensure we haven't moved
                foundValues.push_back(currentItem.data().toString());
                expectedValues.push_back(cmdStream[i] + "_");
            } else {
                // current index should be invalid
                foundValues.push_back("-1");
                expectedValues.push_back(cmdStream[i]);
            };
            QCOMPARE(foundValues, expectedValues);
        };

        if (cmdStream[i] == "C") {
            ++i;
            // set current item
            currentItem = model.index(cmdStream[i].toInt(&numberOk), 0);
            Q_ASSERT(numberOk);
            Q_ASSERT(currentItem.isValid());
            foundValues.push_back("C");
            expectedValues.push_back("C");
        } else if (cmdStream[i] == "N") {
            ++i;
            UiUtils::gotoNext(&model, currentItem, matcher, positiveAction, negativeAction);
        } else if (cmdStream[i] == "P") {
            ++i;
            UiUtils::gotoPrevious(&model, currentItem, matcher, positiveAction, negativeAction);
        } else {
            Q_ASSERT(false);
        }
    }
    QCOMPARE(foundValues.size(), cmdStream.size() / 2);
#endif
}

void TestQaimDfsIterator::testWrappedFind_data()
{
    QTest::addColumn<QString>("items");
    QTest::addColumn<QString>("commands");

    // So this is a "fancy DSL" for controlling iteration through the contents of this random model.
    // At first, we specify the content. Our model is linear, not a subtree one (we have other tests
    // for that). Interesting items (those that we're looking for) are marked by an underscore.
    //
    // Next come the actual commands -- a character followed by a number.
    // - C: set the current index to row #something (note that it starts at zero).
    // - N: go to next interesting index and ensure that its number matches our expectation
    // - P: previous one, i.e., N in the other direction
    QTest::newRow("no-marked-items") << "1 2 3 4 5" << "N -1 P -1 N -1 N -1 P -1 P -1 P -1";
    QTest::newRow("simple-forward") << "1 2_ 3 4 5_ 6_ 7_ 8" << "N 2 N 5 N 6 N 7 N 2 N 5 N 6";
    QTest::newRow("simple-forward-interesting-at-beginning") << "1_ 2_ 3 4 5_ 6_ 7_ 8" << "N 1 N 2 N 5 N 6 N 7 N 1 N 2 N 5 N 6";
    QTest::newRow("simple-backward") << "1 2_ 3 4 5_ 6_ 7_ 8" << "P 7 P 6 P 5 P 2 P 7 P 6";
    QTest::newRow("simple-backward-interesting-at-end") << "1_ 2_ 3 4 5 6_ 7_ 8_" << "P 8 P 7 P 6 P 2 P 1 P 8 P 7 P 6";
    QTest::newRow("simple-navigation") << "1 2_ 3 4 5_ 6_ 7_ 8" << "N 2 N 5 N 6 P 5 P 2 P 7 P 6 N 7 N 2";
    QTest::newRow("navigation-with-initial") << "0 1 2_ 3 4 5_ 6_ 7_ 8" << "C 3 N 5 N 6 P 5 P 2 P 7 P 6 N 7 N 2";
}


QTEST_GUILESS_MAIN(TestQaimDfsIterator)
