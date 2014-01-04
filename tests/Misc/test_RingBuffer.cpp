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

#include <QDebug>
#include <QMetaType>
#include <QTest>
#include "test_RingBuffer.h"
#include "Utils/headless_test.h"
#include "Common/RingBuffer.h"

using namespace Common;

Q_DECLARE_METATYPE(QVector<int>);

void RingBufferTest::testOne()
{
    QFETCH(int, size);
    QFETCH(QVector<int>, sourceData);
    QFETCH(QVector<int>, expectedData);
    RingBuffer<int> rb(size);
    Q_FOREACH(const int item, sourceData) {
        rb.append(item);
    }
    QVector<int> output;

    for (RingBuffer<int>::const_iterator it = rb.begin(); it != rb.end(); ++it) {
        output << *it;
        if (output.size() >= size * 2) {
            QFAIL("Iterated way too many times");
            break;
        }
    }

    // Correct amount of data received?
    QCOMPARE(output.size(), expectedData.size());

    // Correct data?
    QCOMPARE(expectedData, output);

    // Did it overwrite a correct number of items?
    QCOMPARE(static_cast<uint>(sourceData.size() - expectedData.size()), rb.skippedCount());

    // Try to nuke them
    rb.clear();
    // Is it really empty?
    // Yes, QVERIFY instead of QCOMPARE -- they can't be printed
    QVERIFY(rb.begin() == rb.end());
}

void RingBufferTest::testOne_data()
{
    QTest::addColumn<int>("size");
    QTest::addColumn<QVector<int> >("sourceData");
    QTest::addColumn<QVector<int> >("expectedData");

    QVector<int> data;
    QTest::newRow("empty") << 5 << data << data;

    data.clear();
    data << 333;
    QTest::newRow("one-value") << 5 << data << data;

    data.clear();
    data << 333 << 666;
    QTest::newRow("two-values") << 5 << data << data;

    data.clear();
    data << 333 << 666 << 7;
    QTest::newRow("three-values") << 5 << data << data;

    data.clear();
    data << 333 << 666 << 7 << 15;
    QTest::newRow("four-values") << 5 << data << data;

    data.clear();
    data << 333 << 666 << 7 << 15 << 9;
    QTest::newRow("five-values") << 5 << data << data;

    data.clear();
    data << 333 << 666 << 7 << 15 << 9 << 13;
    QVector<int> expected;
    expected << 666 << 7 << 15 << 9 << 13;
    QTest::newRow("six-wrapped") << 5 << data << expected;

    data.clear();
    data << 333 << 666 << 7 << 15 << 9 << 13 << 0;
    expected.clear();
    expected << 7 << 15 << 9 << 13 << 0;
    QTest::newRow("seven-wrapped") << 5 << data << expected;

    data.clear();
    data << 333 << 666 << 7 << 15 << 9 << 13 << 0 << 2;
    expected.clear();
    expected << 15 << 9 << 13 << 0 << 2;
    QTest::newRow("eight-wrapped") << 5 << data << expected;

    data.clear();
    data << 333 << 666 << 7 << 15 << 9 << 13 << 0 << 2 << 1;
    expected.clear();
    expected << 9 << 13 << 0 << 2 << 1;
    QTest::newRow("nine-wrapped") << 5 << data << expected;

    data.clear();
    data << 333 << 666 << 7 << 15 << 9 << 13 << 0 << 2 << 1 << 800500;
    expected.clear();
    expected << 13 << 0 << 2 << 1 << 800500;
    QTest::newRow("ten-wrapped") << 5 << data << expected;

    data.clear();
    data << 333 << 666 << 7 << 15 << 9 << 13 << 0 << 2 << 1 << 800500 << 11;
    expected.clear();
    expected << 0 << 2 << 1 << 800500 << 11;
    QTest::newRow("eleven-wrapped") << 5 << data << expected;
}

TROJITA_HEADLESS_TEST( RingBufferTest )
