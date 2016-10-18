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
#include "UiUtils/QaimDfsIterator.h"

void TestQaimDfsIterator::testQaimDfsIterator()
{
    QFETCH(QString, order);
    QFETCH(QSharedPointer<QStandardItemModel>, model);
    QFETCH(UiUtils::QaimDfsIterator, end);

    QStringList buf;
    auto it = UiUtils::QaimDfsIterator(model->index(0, 0, QModelIndex()));
    std::transform(it, end, std::back_inserter(buf),
                   [](const QModelIndex &what) -> QString {
        return what.data().toString();
    }
                   );
    QCOMPARE(buf.join(QLatin1Char(' ')), order);

    if (order.isEmpty()) {
        QCOMPARE(std::distance(it, end), 0);
    } else {
        QCOMPARE(std::distance(it, end), order.count(QLatin1Char(' ')) + 1);
    }
}

void TestQaimDfsIterator::testQaimDfsIterator_data()
{
    QTest::addColumn<QString>("order");
    QTest::addColumn<QSharedPointer<QStandardItemModel>>("model");
    QTest::addColumn<UiUtils::QaimDfsIterator>("end");
    QSharedPointer<QStandardItemModel> m;

    m.reset(new QStandardItemModel());
    QTest::newRow("empty-model") << QString() << m << UiUtils::QaimDfsIterator();

    m.reset(new QStandardItemModel());
    m->appendRow(new QStandardItem("a"));
    QTest::newRow("one-item") << "a" << m << UiUtils::QaimDfsIterator();

    m.reset(new QStandardItemModel());
    m->appendRow(new QStandardItem("a"));
    m->appendRow(new QStandardItem("b"));
    m->appendRow(new QStandardItem("c"));
    QTest::newRow("flat-list") << "a b c" << m << UiUtils::QaimDfsIterator();

    m.reset(new QStandardItemModel());
    auto item3 = new QStandardItem("a.A.1");
    auto item2 = new QStandardItem("a.A");
    item2->appendRow(item3);
    auto item1 = new QStandardItem("a");
    item1->appendRow(item2);
    m->appendRow(item1);
    item1 = item2 = item3 = nullptr;
    QTest::newRow("linear-hierarchy") << "a a.A a.A.1" << m << UiUtils::QaimDfsIterator();

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
    QTest::newRow("backtracking") << "a a.A a.A.1 a.B a.B.1 b" << m << UiUtils::QaimDfsIterator();

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
    auto end = UiUtils::QaimDfsIterator(m->index(1, 0, QModelIndex()));
    Q_ASSERT(end->data().toString() == "b");
    QTest::newRow("backtracking-until-top-level") << "a a.A a.A.1 a.B a.B.1" << m << end;
}

QTEST_GUILESS_MAIN(TestQaimDfsIterator)
