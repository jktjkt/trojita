/*
   This file is part of the kimap library.
   Copyright (C) 2007 Tom Albers <tomalbers@kde.nl>
   Copyright (c) 2007 Allen Winter <winter@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <QDebug>
#include <QTest>
#include "test_Rfc5322.h"
#include "../headless_test.h"
#include "Imap/Parser/Rfc5322HeaderParser.h"

Q_DECLARE_METATYPE(QList<QByteArray>)

void Rfc5322Test::initTestCase()
{
    qRegisterMetaType<QList<QByteArray> >("QList<QByteArray>");
}

void Rfc5322Test::testReferences()
{
    QFETCH(QByteArray, input);
    QFETCH(bool, ok);
    QFETCH(QList<QByteArray>, references);

    Imap::LowLevelParser::Rfc5322HeaderParser parser;
    bool res = parser.parse(input);
    QCOMPARE(res, ok);
    if (parser.references != references) {
        qDebug() << "Actual:" << parser.references;
        qDebug() << "Expected:" << references;
    }
    QCOMPARE(parser.references, references);
}

void Rfc5322Test::testReferences_data()
{
    QTest::addColumn<QByteArray>("input");
    QTest::addColumn<bool>("ok");
    QTest::addColumn<QList<QByteArray> >("references");

    QList<QByteArray> refs;

    QTest::newRow("empty-1") << QByteArray() << true << refs;
    QTest::newRow("empty-2") << QByteArray("  ") << true << refs;
    QTest::newRow("empty-3") << QByteArray("\r\n  \r\n") << true << refs;

    refs << "foo@bar";
    QTest::newRow("trivial") << QByteArray("reFerences: <foo@bar>\r\n") << true << refs;

    refs.clear();
    refs << "a@b" << "x@[aaaa]" << "bar@baz";
    QTest::newRow("folding-squares-phrases-other-headers-etc")
        << QByteArray("references: <a@b>   <x@[aaaa]> foo <bar@\r\n baz>\r\nfail: foo\r\n\r\nsmrt")
        << true << refs;

    QTest::newRow("broken-following-headers")
        << QByteArray("references: <a@b>   <x@[aaaa]> foo <bar@\r\n baz>\r\nfail: foo\r")
        << false << refs;
}


TROJITA_HEADLESS_TEST( Rfc5322Test )
