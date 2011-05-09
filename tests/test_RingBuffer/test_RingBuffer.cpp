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

#include <QDebug>
#include <QMetaType>
#include <QTest>
#include "test_RingBuffer.h"
#include "../headless_test.h"
#include "Imap/Model/RingBuffer.h"

using namespace Imap;

Q_DECLARE_METATYPE(QVector<int>);

void RingBufferTest::testOne()
{
    QFETCH(int, size);
    QFETCH(QVector<int>, sourceData);
    RingBuffer<int> rb(size);
    Q_FOREACH(const int item, sourceData) {
        rb.append(item);
    }
    QVector<int> output;
    for (RingBuffer<int>::const_iterator it = rb.begin(); it != rb.end(); ++it) {
        output << *it;
    }

    // Correct amount of data received?
    QCOMPARE(output.size(), sourceData.size());

    // Correct data?
    QCOMPARE(sourceData, output);

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

    QVector<int> data;
    QTest::newRow("empty") << 5 << data;

    data.clear();
    data << 333;
    QTest::newRow("one-value") << 5 << data;

    data.clear();
    data << 333 << 666;
    QTest::newRow("two-values") << 5 << data;

    data.clear();
    data << 333 << 666 << 7;
    QTest::newRow("three-values") << 5 << data;

    data.clear();
    data << 333 << 666 << 7 << 15;
    QTest::newRow("four-values") << 5 << data;

    /*data.clear();
    data << 333 << 666 << 7 << 15 << 9;
    QTest::newRow("five-values") << 5 << data;*/
}

TROJITA_HEADLESS_TEST( RingBufferTest )
