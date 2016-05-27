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

#include "test_Imap_Parser_write.h"
#include "Utils/FakeCapabilitiesInjector.h"
#include "Streams/FakeSocket.h"

#define APPEND_PREFIX "APPEND a \"\\d{2}-[a-zA-Z]{3}-\\d{4} \\d{2}:\\d{2}:\\d{2} [+-]?\\d{4}\" "

QByteArray plaintext10 = QByteArray(10, 'x');
QByteArray plaintext4096 = QByteArray(4096, 'y');
QByteArray plaintext4097 = QByteArray(4097, 'z');

using namespace Imap::Mailbox;

void ImapParserWriteTest::testNoLiteralPlus()
{
    model->appendIntoMailbox(QStringLiteral("a"), plaintext10, QStringList(), QDateTime::currentDateTime());
    cClientRegExp(t.mk(APPEND_PREFIX) + "\\{" + QByteArray::number(plaintext10.size()) + "\\}");
    cServer("+ OK send your literal\r\n");
    cClient(plaintext10 + "\r\n");
    cServer(t.last("OK stored\r\n"));
    cEmpty();
}

void ImapParserWriteTest::testLiteralPlus()
{
    FakeCapabilitiesInjector caps(model);
    caps.injectCapability(QStringLiteral("LITERAL+"));
    model->appendIntoMailbox(QStringLiteral("a"), plaintext4097, QStringList(), QDateTime::currentDateTime());
    // cClientRegExp doesn't support multiline data
    for (int i = 0; i < 10; ++i) {
        QCoreApplication::processEvents();
    }
    auto buf = SOCK->writtenStuff();
    QVERIFY(buf.startsWith(t.mk("APPEND a")));
    QVERIFY(buf.endsWith(QByteArrayLiteral(" {4097+}\r\n") + plaintext4097 + QByteArrayLiteral("\r\n")));
    cServer(t.last("OK stored\r\n"));
    cEmpty();
}

void ImapParserWriteTest::testLiteralMinus()
{
    FakeCapabilitiesInjector caps(model);
    caps.injectCapability(QStringLiteral("LITERAL-"));

    model->appendIntoMailbox(QStringLiteral("a"), plaintext4096, QStringList(), QDateTime::currentDateTime());
    for (int i = 0; i < 10; ++i) {
        QCoreApplication::processEvents();
    }
    auto buf = SOCK->writtenStuff();
    QVERIFY(buf.startsWith(t.mk("APPEND a")));
    QVERIFY(buf.endsWith(QByteArrayLiteral(" {4096+}\r\n") + plaintext4096 + QByteArrayLiteral("\r\n")));
    cServer(t.last("OK stored\r\n"));
    cEmpty();

    model->appendIntoMailbox(QStringLiteral("a"), plaintext4097, QStringList(), QDateTime::currentDateTime());
    cClientRegExp(t.mk(APPEND_PREFIX) + "\\{" + QByteArray::number(plaintext4097.size()) + "\\}");
    cServer("+ OK send your literal\r\n");
    cClient(plaintext4097 + "\r\n");
    cServer(t.last("OK stored\r\n"));
    cEmpty();
}

QTEST_GUILESS_MAIN(ImapParserWriteTest)
