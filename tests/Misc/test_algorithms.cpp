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
#include "test_algorithms.h"
#include "Utils/headless_test.h"

#include "Common/FindWithUnknown.h"

Q_DECLARE_METATYPE(QList<int>)

bool isZero(const int num)
{
    return num == 0;
}

void TestCommonAlgorithms::testLowerBoundWithUnknown()
{
    QFETCH(QList<int>, list);
    QFETCH(int, needle);
    QFETCH(int, offset);

    QList<int>::const_iterator it = Common::linearLowerBoundWithUnknownElements(list.constBegin(), list.constEnd(), needle, isZero, qLess<int>());
    QCOMPARE(it - list.constBegin(), offset);
    it = Common::lowerBoundWithUnknownElements(list.constBegin(), list.constEnd(), needle, isZero, qLess<int>());
    QCOMPARE(it - list.constBegin(), offset);
}

void TestCommonAlgorithms::testLowerBoundWithUnknown_data()
{
    QTest::addColumn<QList<int> >("list");
    QTest::addColumn<int>("needle");
    QTest::addColumn<int>("offset");

    // The basic variant where there are no dummy items
    QTest::newRow("empty-list") << QList<int>() << 10 << 0;
    QTest::newRow("one-item-which-is-bigger") << (QList<int>() << 5) << 1 << 0;
    QTest::newRow("one-item-which-is-lower") << (QList<int>() << 5) << 10 << 1;
    QTest::newRow("one-item-exact") << (QList<int>() << 5) << 5 << 0;
    QTest::newRow("three-items-before-first") << (QList<int>() << 5 << 10 << 15) << 4 << 0;
    QTest::newRow("three-items-first") << (QList<int>() << 5 << 10 << 15) << 5 << 0;
    QTest::newRow("three-items-after-first") << (QList<int>() << 5 << 10 << 15) << 6 << 1;
    QTest::newRow("three-items-before-last") << (QList<int>() << 5 << 10 << 15) << 14 << 2;
    QTest::newRow("three-items-last") << (QList<int>() << 5 << 10 << 15) << 15 << 2;
    QTest::newRow("three-items-after-last") << (QList<int>() << 5 << 10 << 15) << 16 << 3;

    // Add some fake items to the mix
    QTest::newRow("all-fakes") << (QList<int>() << 0 << 0 << 0) << 1 << 3;
    QTest::newRow("one-fake") << (QList<int>() << 0) << 1 << 1;
    QTest::newRow("fake-match-fake") << (QList<int>() << 0 << 1 << 0) << 1 << 1;
    QTest::newRow("fake-fake-match-fake") << (QList<int>() << 0 << 0 << 1 << 0) << 1 << 2;
    QTest::newRow("fake-lower-fake-fake") << (QList<int>() << 0 << 1 << 0 << 0) << 2 << 4;

    QList<int> list;
    list << 1 << 2 << 3 << 4 << 5 << 6 << 7 << 8 << 9 << 10 << 11 << 0 << 13;
    QTest::newRow("many-items-just-one-fake") << list << 2 << 1;
    QTest::newRow("many-items-just-one-fake") << list << 11 << 10;
    QTest::newRow("many-items-just-one-fake") << list << 12 << 12;
    QTest::newRow("many-items-just-one-fake") << list << 13 << 12;
}

TROJITA_HEADLESS_TEST(TestCommonAlgorithms)
