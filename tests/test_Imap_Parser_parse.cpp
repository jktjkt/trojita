/* Copyright (C) 2007 Jan Kundrát <jkt@gentoo.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <qtest_kde.h>
#include <QFile>

#include "test_Imap_Parser_parse.h"
#include "test_Imap_Parser_parse.moc"

Q_DECLARE_METATYPE(std::tr1::shared_ptr<Imap::Responses::AbstractResponse>)
Q_DECLARE_METATYPE(Imap::Responses::Status)

QTEST_KDEMAIN_CORE( ImapParserParseTest )

void ImapParserParseTest::initTestCase()
{
    array.reset( new QByteArray() );
    buf.reset( new QBuffer( array.get() ) );
    parser.reset( new Imap::Parser( 0, buf ) );
}

/** @short Test tagged response parsing */
void ImapParserParseTest::testParseTagged()
{
    QFETCH( QByteArray, line );
    QFETCH( std::tr1::shared_ptr<Imap::Responses::AbstractResponse>, response );

    Q_ASSERT( response );
    QCOMPARE( *(parser->parseTagged( line )), *response );
}

void ImapParserParseTest::testParseTagged_data()
{
    using namespace Imap::Responses;
    using std::tr1::shared_ptr;

    QTest::addColumn<QByteArray>("line");
    QTest::addColumn<std::tr1::shared_ptr<AbstractResponse> >("response");

    std::tr1::shared_ptr<AbstractRespCodeData> emptyList(
            new RespCodeData<QStringList>( QStringList() ) );

    QTest::newRow("tagged-ok-simple")
        << QByteArray("y01 OK Everything works, man!\r\n")
        << shared_ptr<AbstractResponse>( new Status("y01", OK, "Everything works, man!", NONE, emptyList ) );

#if 0
    QTest::newRow("tagged-no-simple")
        << QByteArray("12345 NO Nope, something is broken\r\n")
        << Response("12345", NO, NONE, QStringList(), "Nope, something is broken");
    QTest::newRow("tagged-bad-simple") 
        << QByteArray("ahoj BaD WTF?\r\n") 
        << Response("ahoj", BAD, NONE, QStringList(), "WTF?");

    QTest::newRow("tagged-ok-alert") 
        << QByteArray("y01 oK [ALERT] Server on fire\r\n") 
        << Response("y01", OK, ALERT, QStringList(), "Server on fire");
    QTest::newRow("tagged-no-alert") 
        << QByteArray("1337 no [ALeRT] Server on fire\r\n")
        << Response("1337", NO, ALERT, QStringList(), "Server on fire");
    QTest::newRow("tagged-ok-capability") 
        << QByteArray("y01 OK [CAPaBILITY blurdybloop IMAP4rev1 WTF] Capabilities updated\r\n") 
        << Response("y01", OK, CAPABILITIES, QStringList() << "blurdybloop" << "IMAP4rev1" << "WTF", "Capabilities updated");
    QTest::newRow("tagged-ok-parse") 
        << QByteArray("y01 OK [PArSE] Parse error. What do you feed me with?\r\n") 
        << Response("y01", OK, PARSE, QStringList(), "Parse error. What do you feed me with?");
    QTest::newRow("tagged-ok-permanentflags-empty") 
        << QByteArray("y01 OK [PERMANENTfLAGS] Behold, the flags!\r\n") 
        << Response("y01", OK, PERMANENTFLAGS, QStringList(), "Behold, the flags!");
    QTest::newRow("tagged-ok-permanentflags-flags") 
        << QByteArray("y01 OK [PErMANENTFLAGS \\Foo \\Bar SmrT] Behold, the flags!\r\n") 
        << Response("y01", OK, PERMANENTFLAGS, QStringList() << "\\Foo" << "\\Bar" << "SmrT", "Behold, the flags!");
    QTest::newRow("tagged-ok-readonly") 
        << QByteArray("y01 OK [ReAD-ONLY] No writing for you\r\n") 
        << Response("y01", OK, READ_ONLY, QStringList(), "No writing for you");
    QTest::newRow("tagged-ok-readwrite") 
        << QByteArray("y01 OK [REaD-WRITE] Write!!!\r\n") 
        << Response("y01", OK, READ_WRITE, QStringList(), "Write!!!");
    QTest::newRow("tagged-ok-trycreate") 
        << QByteArray("y01 OK [TryCreate] ...would be better :)\r\n") 
        << Response("y01", OK, TRYCREATE, QStringList(), "...would be better :)");
    QTest::newRow("tagged-ok-uidnext") 
        << QByteArray("y01 OK [uidNext 5] Next UID\r\n") 
        << Response("y01", OK, UIDNEXT, QStringList() << "5", "Next UID");
    QTest::newRow("tagged-ok-uidvalidity") 
        << QByteArray("y01 OK [UIDVALIDITY 17] UIDs valid\r\n") 
        << Response("y01", OK, UIDVALIDITY, QStringList() << "17", "UIDs valid");
    QTest::newRow("tagged-ok-unseen") 
        << QByteArray("y01 OK [unSeen 666] I need my glasses\r\n") 
        << Response("y01", OK, UNSEEN, QStringList() << "666", "I need my glasses");
#endif

}

#include "Imap/Parser.h"


