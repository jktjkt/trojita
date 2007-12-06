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
Q_DECLARE_METATYPE(Imap::Responses::State)

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
    QTest::addColumn<shared_ptr<AbstractResponse> >("response");

    shared_ptr<AbstractData> voidData( new RespData<void>() );
    std::tr1::shared_ptr<AbstractData> emptyList(
            new RespData<QStringList>( QStringList() ) );

    QTest::newRow("tagged-ok-simple")
        << QByteArray("y01 OK Everything works, man!\r\n")
        << shared_ptr<AbstractResponse>( new State("y01", OK, "Everything works, man!", NONE, voidData ) );

    QTest::newRow("tagged-no-simple")
        << QByteArray("12345 NO Nope, something is broken\r\n")
        << shared_ptr<AbstractResponse>( new State("12345", NO, "Nope, something is broken", NONE, voidData ) );

    QTest::newRow("tagged-bad-simple") 
        << QByteArray("ahoj BaD WTF?\r\n") 
        << shared_ptr<AbstractResponse>( new State("ahoj", BAD, "WTF?", NONE, voidData ) );

    QTest::newRow("tagged-ok-alert") 
        << QByteArray("y01 oK [ALERT] Server on fire\r\n") 
        << shared_ptr<AbstractResponse>( new State("y01", OK, "Server on fire", ALERT, voidData ) );
    QTest::newRow("tagged-no-alert") 
        << QByteArray("1337 no [ALeRT] Server on fire\r\n")
        << shared_ptr<AbstractResponse>( new State("1337", NO, "Server on fire", ALERT, voidData ) );

    QTest::newRow("tagged-ok-capability") 
        << QByteArray("y01 OK [CAPaBILITY blurdybloop IMAP4rev1 WTF] Capabilities updated\r\n") 
        << shared_ptr<AbstractResponse>( new State("y01", OK, "Capabilities updated", CAPABILITIES, 
                        shared_ptr<AbstractData>(
                            new RespData<QStringList>( QStringList() << "blurdybloop" << "IMAP4rev1" << "WTF") ) ) );

    QTest::newRow("tagged-ok-parse") 
        << QByteArray("y01 OK [PArSE] Parse error. What do you feed me with?\r\n") 
        << shared_ptr<AbstractResponse>( new State("y01", OK, "Parse error. What do you feed me with?",
                    PARSE, voidData) );
    QTest::newRow("tagged-ok-permanentflags-empty") 
        << QByteArray("y01 OK [PERMANENTfLAGS] Behold, the flags!\r\n") 
        << shared_ptr<AbstractResponse>( new State("y01", OK, "Behold, the flags!", PERMANENTFLAGS, emptyList) );
    QTest::newRow("tagged-ok-permanentflags-flags") 
        << QByteArray("y01 OK [PErMANENTFLAGS \\Foo \\Bar SmrT] Behold, the flags!\r\n") 
        << shared_ptr<AbstractResponse>( new State("y01", OK, "Behold, the flags!", PERMANENTFLAGS,
                    shared_ptr<AbstractData>( new RespData<QStringList>(
                            QStringList() << "\\Foo" << "\\Bar" << "SmrT" ))) );
    QTest::newRow("tagged-ok-readonly") 
        << QByteArray("333 OK [ReAD-ONLY] No writing for you\r\n") 
        << shared_ptr<AbstractResponse>( new State("333", OK, "No writing for you", READ_ONLY, voidData));
    QTest::newRow("tagged-ok-readwrite") 
        << QByteArray("y01 OK [REaD-WRITE] Write!!!\r\n") 
        << shared_ptr<AbstractResponse>( new State("y01", OK, "Write!!!", READ_WRITE, voidData));
    QTest::newRow("tagged-ok-trycreate") 
        << QByteArray("y01 OK [TryCreate] ...would be better :)\r\n") 
        << shared_ptr<AbstractResponse>( new State("y01", OK, "...would be better :)", TRYCREATE, voidData));
    QTest::newRow("tagged-ok-uidnext") 
        << QByteArray("y01 OK [uidNext 5] Next UID\r\n") 
        << shared_ptr<AbstractResponse>( new State("y01", OK, "Next UID", UIDNEXT,
                    shared_ptr<AbstractData>( new RespData<uint>( 5 ) )));
    QTest::newRow("tagged-ok-uidvalidity") 
        << QByteArray("y01 OK [UIDVALIDITY 17] UIDs valid\r\n") 
        << shared_ptr<AbstractResponse>( new State("y01", OK, "UIDs valid", UIDVALIDITY,
                    shared_ptr<AbstractData>( new RespData<uint>( 17 ) )));
    QTest::newRow("tagged-ok-unseen") 
        << QByteArray("y01 OK [unSeen 666] I need my glasses\r\n") 
        << shared_ptr<AbstractResponse>( new State("y01", OK, "I need my glasses", UNSEEN,
                    shared_ptr<AbstractData>( new RespData<uint>( 666 ) )));

}

/** @short Test untagged response parsing */
void ImapParserParseTest::testParseUntagged()
{
    QFETCH( QByteArray, line );
    QFETCH( std::tr1::shared_ptr<Imap::Responses::AbstractResponse>, response );

    Q_ASSERT( response );
    QCOMPARE( *(parser->parseUntagged( line )), *response );
}

void ImapParserParseTest::testParseUntagged_data()
{
    using namespace Imap::Responses;
    using std::tr1::shared_ptr;

    QTest::addColumn<QByteArray>("line");
    QTest::addColumn<shared_ptr<AbstractResponse> >("response");

    shared_ptr<AbstractData> voidData( new RespData<void>() );
    std::tr1::shared_ptr<AbstractData> emptyList(
            new RespData<QStringList>( QStringList() ) );

    QTest::newRow("untagged-ok-simple")
        << QByteArray("* OK Everything works, man!\r\n")
        << shared_ptr<AbstractResponse>( new State(QString::null, OK, "Everything works, man!", NONE, voidData ) );

    QTest::newRow("untagged-no-simple")
        << QByteArray("* NO Nope, something is broken\r\n")
        << shared_ptr<AbstractResponse>( new State(QString::null, NO, "Nope, something is broken", NONE, voidData ) );
    QTest::newRow("untagged-ok-uidvalidity") 
        << QByteArray("* OK [UIDVALIDITY 17] UIDs valid\r\n") 
        << shared_ptr<AbstractResponse>( new State(QString::null, OK, "UIDs valid", UIDVALIDITY,
                    shared_ptr<AbstractData>( new RespData<uint>( 17 ) )));
    QTest::newRow("untagged-bye")
        << QByteArray("* BYE go away\r\n")
        << shared_ptr<AbstractResponse>( new State(QString::null, BYE, "go away", NONE, voidData ) );

    QTest::newRow("untagged-expunge")
        << QByteArray("* 1337 Expunge\r\n")
        << shared_ptr<AbstractResponse>( new NumberResponse( EXPUNGE, 1337 ) );
    QTest::newRow("untagged-exists")
        << QByteArray("* 3 exIsts\r\n")
        << shared_ptr<AbstractResponse>( new NumberResponse( EXISTS, 3 ) );
    QTest::newRow("untagged-recent")
        << QByteArray("* 666 recenT\r\n")
        << shared_ptr<AbstractResponse>( new NumberResponse( RECENT, 666 ) );

    QTest::newRow("untagged-capability")
        << QByteArray("* CAPABILITY fooBar IMAP4rev1 blah\r\n")
        << shared_ptr<AbstractResponse>( new Capability( QStringList() << "fooBar" << "IMAP4rev1" << "blah" ) );

    QTest::newRow("untagged-list")
        << QByteArray("* LIST (\\Noselect) \".\" \"\"\r\n")
        << shared_ptr<AbstractResponse>( new List( LIST, QStringList() << "\\Noselect", ".", "" ) );
    QTest::newRow("untagged-lsub")
        << QByteArray("* LSUB (\\Noselect) \".\" \"\"\r\n")
        << shared_ptr<AbstractResponse>( new List( LSUB, QStringList() << "\\Noselect", ".", "" ) );
    QTest::newRow("untagged-list-moreflags")
        << QByteArray("* LIST (\\Noselect Blesmrt) \".\" \"\"\r\n")
        << shared_ptr<AbstractResponse>( new List( LIST, QStringList() << "\\Noselect" << "Blesmrt", ".", "" ) );
    QTest::newRow("untagged-list-mailbox")
        << QByteArray("* LIST () \".\" \"someName\"\r\n")
        << shared_ptr<AbstractResponse>( new List( LIST, QStringList(), ".", "someName" ) );
    QTest::newRow("untagged-list-mailbox-atom")
        << QByteArray("* LIST () \".\" someName\r\n")
        << shared_ptr<AbstractResponse>( new List( LIST, QStringList(), ".", "someName" ) );
    QTest::newRow("untagged-list-separator-nil")
        << QByteArray("* LIST () NiL someName\r\n")
        << shared_ptr<AbstractResponse>( new List( LIST, QStringList(), QString::null, "someName" ) );
    QTest::newRow("untagged-list-mailbox-quote")
        << QByteArray("* LIST () \".\" \"some\\\"Name\"\r\n")
        << shared_ptr<AbstractResponse>( new List( LIST, QStringList(), ".", "some\"Name" ) );
#include "test_Imap_Parser_parse-Chinese.include"

    QTest::newRow("untagged-flags")
        << QByteArray("* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n")
        << shared_ptr<AbstractResponse>(
                new Flags( QStringList() << "\\Answered" << "\\Flagged" <<
                    "\\Deleted" << "\\Seen" << "\\Draft" ) );
    QTest::newRow("search-empty")
        << QByteArray("* SEARCH\r\n")
        << shared_ptr<AbstractResponse>( new Search( QList<uint>() ) );
    QTest::newRow("search-messages")
        << QByteArray("* SEARCH 1 33 666\r\n")
        << shared_ptr<AbstractResponse>( new Search( QList<uint>() << 1 << 33 << 666 ) );

    Status::stateDataType states;
    states[Status::MESSAGES] = 231;
    states[Status::UIDNEXT] = 44292;
    QTest::newRow("status-1")
        << QByteArray("* STATUS blurdybloop (MESSAGES 231 UIDNEXT 44292)\r\n")
        << shared_ptr<AbstractResponse>( new Status( "blurdybloop", states ) );
    states[Status::UIDVALIDITY] = 1337;
    states[Status::RECENT] = 3234567890u;
    QTest::newRow("status-2")
        << QByteArray("* STATUS blurdybloop (MESSAGES 231 UIDNEXT 44292 UIDVALIDITY 1337 RECENT 3234567890)\r\n")
        << shared_ptr<AbstractResponse>( new Status( "blurdybloop", states ) );

}

QTEST_KDEMAIN_CORE( ImapParserParseTest )

namespace QTest {

template<> char * toString( const Imap::Responses::AbstractResponse& resp )
{
    QByteArray buf;
    QTextStream stream( &buf );
    stream << resp;
    stream.flush();
    return qstrdup( buf.data() );
}

}
