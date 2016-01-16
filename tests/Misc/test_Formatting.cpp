/* Copyright (C) 2016 Thomas Lübking <thomas.luebking@gmail.com>

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
#include "test_Formatting.h"

#include "UiUtils/Formatting.h"

void TestFormatting::testAddressEliding()
{
    QFETCH(QString, original);
    QFETCH(QString, expected);
    QFETCH(bool, wasElided);

    QCOMPARE(UiUtils::elideAddress(original), wasElided);
    QCOMPARE(original /* now elided */, expected);
}

void TestFormatting::testAddressEliding_data()
{
    QTest::addColumn<QString>("original");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<bool>("wasElided");

    QTest::newRow("normal-address")
        << QStringLiteral("homer@springfield.us")
        << QStringLiteral("homer@springfield.us")
        << false;

    // address is too long, but we don't want to touch the domain and the local part is too short
    QTest::newRow("long-domain")
        << QStringLiteral("homer@burns-nucular-plant-near-moes-tavern-in-springfield-usa.com")
        << QStringLiteral("homer@burns-nucular-plant-near-moes-tavern-in-springfield-usa.com")
        << false;

    // elide local part, but leave 4 heading and tailing chars intact
    QTest::newRow("eliding-local-preserve-head-tail")
        << QStringLiteral("homer.simpson@burns-nucular-plant-near-moes-tavern-in-springfield-usa.com")
        << QStringLiteral("home\u2026pson@burns-nucular-plant-near-moes-tavern-in-springfield-usa.com")
        << true;

    // elide overly long local part, leave domain intact, result shall have 60 chars (+ellipsis)
    QTest::newRow("eliding-in-local-part")
        << QStringLiteral("homer_marge_bart_lisa_and_margaret_who_is_margaret.simpson@springfield.us")
        << QStringLiteral("homer_marge_bart_lisa_\u2026ho_is_margaret.simpson@springfield.us")
        << true;

    // in random long strings, leave 30 leading and tailing chars
    QTest::newRow("elide-non-mail")
        << QStringLiteral("012345678901234567890123456789 0123456789 012345678901234567890123456789")
        << QStringLiteral("012345678901234567890123456789\u2026012345678901234567890123456789")
        << true;

}

QTEST_GUILESS_MAIN(TestFormatting)
