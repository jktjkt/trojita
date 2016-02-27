/* Copyright (C) 2016 Erik Quaeghebeur <trojita@equaeghe.nospammail.net>

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
#include "test_prettySize.h"

#include "UiUtils/Formatting.h"

/** @short Test correctness of pretty sizes generated */
void PrettySizeTest::testPrettySize()
{
    QFETCH(quint64, bytes);
    QFETCH(QString, formatted);

    QCOMPARE(UiUtils::Formatting::prettySize(bytes), formatted);
}

void PrettySizeTest::testPrettySize_data()
{
    QTest::addColumn<quint64>("bytes");
    QTest::addColumn<QString>("formatted");

    QTest::newRow("order 0, magnitude 0, zero") << Q_UINT64_C(0)
                                                << "0 B";
    QTest::newRow("order 0, magnitude 0") << Q_UINT64_C(1)
                                          << "1 B";
    QTest::newRow("order 0, magnitude 1") << Q_UINT64_C(12)
                                          << "12 B";
    QTest::newRow("order 0, magnitude 2") << Q_UINT64_C(123)
                                          << "123 B";

    QTest::newRow("order 1, magnitude 0, round down") << Q_UINT64_C(1234)
                                                      << "1.23 kB";
    QTest::newRow("order 1, magnitude 0, round up") << Q_UINT64_C(1236)
                                                    << "1.24 kB";
    QTest::newRow("order 1, magnitude 1, round down") << Q_UINT64_C(12345)
                                                      << "12.3 kB";
    QTest::newRow("order 1, magnitude 1, round up") << Q_UINT64_C(12365)
                                                    << "12.4 kB";
    QTest::newRow("order 1, magnitude 2, round down") << Q_UINT64_C(123456)
                                                      << "123 kB";
    QTest::newRow("order 1, magnitude 2, round up") << Q_UINT64_C(123654)
                                                    << "124 kB";
    QTest::newRow("order 1/2, magnitude 2, round down") << Q_UINT64_C(999456)
                                                        << "999 kB";
    QTest::newRow("order 1/2, magnitude 2, round up") << Q_UINT64_C(999654)
                                                      << "1.00 MB";

    QTest::newRow("order 2, magnitude 0, round down") << Q_UINT64_C(1234000)
                                                      << "1.23 MB";
    QTest::newRow("order 2, magnitude 0, round up") << Q_UINT64_C(1236000)
                                                    << "1.24 MB";
    QTest::newRow("order 2, magnitude 1, round down") << Q_UINT64_C(12345000)
                                                      << "12.3 MB";
    QTest::newRow("order 2, magnitude 1, round up") << Q_UINT64_C(12365000)
                                                    << "12.4 MB";
    QTest::newRow("order 2, magnitude 2, round down") << Q_UINT64_C(123456000)
                                                      << "123 MB";
    QTest::newRow("order 2, magnitude 2, round up") << Q_UINT64_C(123654000)
                                                    << "124 MB";
    QTest::newRow("order 2/3, magnitude 2, round down") << Q_UINT64_C(999456000)
                                                        << "999 MB";
    QTest::newRow("order 2/3, magnitude 2, round up") << Q_UINT64_C(999654000)
                                                      << "1.00 GB";

    QTest::newRow("order 3, magnitude 0, round down") << Q_UINT64_C(1234000000)
                                                      << "1.23 GB";
    QTest::newRow("order 3, magnitude 0, round up") << Q_UINT64_C(1236000000)
                                                    << "1.24 GB";
    QTest::newRow("order 3, magnitude 1, round down") << Q_UINT64_C(12345000000)
                                                      << "12.3 GB";
    QTest::newRow("order 3, magnitude 1, round up") << Q_UINT64_C(12365000000)
                                                    << "12.4 GB";
    QTest::newRow("order 3, magnitude 2, round down") << Q_UINT64_C(123456000000)
                                                      << "123 GB";
    QTest::newRow("order 3, magnitude 2, round up") << Q_UINT64_C(123654000000)
                                                    << "124 GB";
    QTest::newRow("order 3/4, magnitude 2, round down") << Q_UINT64_C(999456000000)
                                                        << "999 GB";
    QTest::newRow("order 3/4, magnitude 2, round up") << Q_UINT64_C(999654000000)
                                                      << "1.00 TB";

    QTest::newRow("order 4, magnitude 0, round down") << Q_UINT64_C(1234000000000)
                                                      << "1.23 TB";
    QTest::newRow("order 4, magnitude 0, round up") << Q_UINT64_C(1236000000000)
                                                    << "1.24 TB";
    QTest::newRow("order 4, magnitude 1, round down") << Q_UINT64_C(12345000000000)
                                                      << "12.3 TB";
    QTest::newRow("order 4, magnitude 1, round up") << Q_UINT64_C(12365000000000)
                                                    << "12.4 TB";
    QTest::newRow("order 4, magnitude 2, round down") << Q_UINT64_C(123456000000000)
                                                      << "123 TB";
    QTest::newRow("order 4, magnitude 2, round up") << Q_UINT64_C(123654000000000)
                                                    << "124 TB";
    QTest::newRow("order 4/5, magnitude 2, round down") << Q_UINT64_C(999456000000000)
                                                        << "999 TB";
    QTest::newRow("order 4/5, magnitude 2, round up") << Q_UINT64_C(999654000000000)
                                                      << "1000 TB";

    QTest::newRow("order 5, magnitude 0, round down") << Q_UINT64_C(1234000000000000)
                                                      << "1230 TB";
    QTest::newRow("order 5, magnitude 0, round up") << Q_UINT64_C(1236000000000000)
                                                    << "1240 TB";
    QTest::newRow("order 5, magnitude 1, round down") << Q_UINT64_C(12345000000000000)
                                                      << "12300 TB";
    QTest::newRow("order 5, magnitude 1, round up") << Q_UINT64_C(12365000000000000)
                                                    << "12400 TB";
    QTest::newRow("order 5, magnitude 2, round down") << Q_UINT64_C(123456000000000000)
                                                      << "123000 TB";
    QTest::newRow("order 5, magnitude 2, round up") << Q_UINT64_C(123654000000000000)
                                                    << "124000 TB";
    QTest::newRow("order 5/6, magnitude 2, round down") << Q_UINT64_C(999456000000000000)
                                                        << "999000 TB";
    QTest::newRow("order 5/6, magnitude 2, round up") << Q_UINT64_C(999654000000000000)
                                                      << "1000000 TB";
}

QTEST_GUILESS_MAIN(PrettySizeTest)
