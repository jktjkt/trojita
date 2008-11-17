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
#include "Imap/Parser/Message.h"
#include "Imap/Streams/IODeviceSocket.h"

#include "test_Imap_Parser_parse.h"
#include "test_Imap_Parser_parse.moc"

Q_DECLARE_METATYPE(std::tr1::shared_ptr<Imap::Responses::AbstractResponse>)
Q_DECLARE_METATYPE(Imap::Responses::State)

void ImapParserParseTest::initTestCase()
{
    array.reset( new QByteArray() );
    buf.reset( new Imap::IODeviceSocket( new QBuffer( array.get() ) ) );
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
    std::tr1::shared_ptr<Imap::Responses::AbstractResponse> r = parser->parseUntagged( line );
#if 0 // qDebug()'s internal buffer is too small to be useful here, that's why QCOMPARE's normal dumping is not enough
    if ( *r != *response ) {
        QTextStream s( stderr );
        s << "\nPARSED:\n" << *r;
        s << "\nEXPETCED:\n";
        s << *response;
    }
#endif
    QCOMPARE( *r, *response );
}

void ImapParserParseTest::testParseUntagged_data()
{
    using namespace Imap::Responses;
    using namespace Imap::Message;
    using std::tr1::shared_ptr;
    using Imap::Responses::Fetch; // needed for gcc-3.4

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

    QTest::newRow("namespace-1")
        << QByteArray("* Namespace nil NIL nil\r\n")
        << shared_ptr<AbstractResponse>( new Namespace( QList<NamespaceData>(), 
                    QList<NamespaceData>(), QList<NamespaceData>() ) );

    QTest::newRow("namespace-2")
        << QByteArray("* Namespace () NIL nil\r\n")
        << shared_ptr<AbstractResponse>( new Namespace( QList<NamespaceData>(), 
                    QList<NamespaceData>(), QList<NamespaceData>() ) );

    QTest::newRow("namespace-3")
        << QByteArray("* Namespace ((\"\" \"/\")) NIL nil\r\n")
        << shared_ptr<AbstractResponse>( new Namespace( QList<NamespaceData>() << NamespaceData( "", "/" ),
                    QList<NamespaceData>(), QList<NamespaceData>() ) );

    QTest::newRow("namespace-4")
        << QByteArray("* Namespace ((\"prefix\" \".\")(\"\" \"/\")) ((\"blesmrt\" \"trojita\")) ((\"foo\" \"bar\"))\r\n")
        << shared_ptr<AbstractResponse>( new Namespace( 
                    QList<NamespaceData>() << NamespaceData( "prefix", "." ) << NamespaceData( "", "/" ),
                    QList<NamespaceData>() << NamespaceData( "blesmrt", "trojita" ), 
                    QList<NamespaceData>() << NamespaceData( "foo", "bar" ) ) );

    QTest::newRow("namespace-5")
        << QByteArray("* NAMESPACE ((\"\" \"/\")(\"#mhinbox\" NIL)(\"#mh/\" \"/\")) ((\"~\" \"/\")) "
                "((\"#shared/\" \"/\")(\"#ftp/\" \"/\")(\"#news.\" \".\")(\"#public/\" \"/\"))\r\n")
        << shared_ptr<AbstractResponse>( new Namespace( 
                    QList<NamespaceData>() << NamespaceData( "", "/" ) << NamespaceData( "#mhinbox", QByteArray() ) 
                        << NamespaceData( "#mh/", "/" ),
                    QList<NamespaceData>() << NamespaceData( "~", "/" ),
                    QList<NamespaceData>() << NamespaceData( "#shared/", "/" ) << NamespaceData( "#ftp/", "/" ) 
                        << NamespaceData( "#news.", "." ) << NamespaceData( "#public/", "/" )
                    ) );



    Fetch::dataType fetchData;

    QTest::newRow("fetch-empty")
        << QByteArray("* 66 FETCH ()\r\n")
        << shared_ptr<AbstractResponse>( new Fetch( 66, fetchData ) );

    fetchData.clear();
    fetchData[ "RFC822.SIZE" ] = std::tr1::shared_ptr<AbstractData>( new RespData<uint>( 1337 ) );
    QTest::newRow("fetch-rfc822-size")
        << QByteArray("* 123 FETCH (rfc822.size 1337)\r\n")
        << shared_ptr<AbstractResponse>( new Fetch( 123, fetchData ) );

    fetchData.clear();
    fetchData[ "RFC822.SIZE" ] = std::tr1::shared_ptr<AbstractData>( new RespData<uint>( 1337 ) );
    fetchData[ "UID" ] = std::tr1::shared_ptr<AbstractData>( new RespData<uint>( 666 ) );
    QTest::newRow("fetch-rfc822-size-uid")
        << QByteArray("* 123 FETCH (uID 666 rfc822.size 1337)\r\n")
        << shared_ptr<AbstractResponse>( new Fetch( 123, fetchData ) );
    QTest::newRow("fetch-rfc822-size-uid-swapped")
        << QByteArray("* 123 FETCH (rfc822.size 1337 uId 666)\r\n")
        << shared_ptr<AbstractResponse>( new Fetch( 123, fetchData ) );

    fetchData.clear();
    fetchData[ "RFC822.HEADER" ] = std::tr1::shared_ptr<AbstractData>( new RespData<QByteArray>( "123456789012" ) );
    QTest::newRow("fetch-rfc822-header")
        << QByteArray("* 123 FETCH (rfc822.header {12}\r\n123456789012)\r\n")
        << shared_ptr<AbstractResponse>( new Fetch( 123, fetchData ) );

    fetchData.clear();
    fetchData[ "RFC822.TEXT" ] = std::tr1::shared_ptr<AbstractData>( new RespData<QByteArray>( "abcdEf" ) );
    QTest::newRow("fetch-rfc822-text")
        << QByteArray("* 123 FETCH (rfc822.tExt abcdEf)\r\n")
        << shared_ptr<AbstractResponse>( new Fetch( 123, fetchData ) );

    fetchData.clear();
    fetchData[ "INTERNALDATE" ] = std::tr1::shared_ptr<AbstractData>(
            new RespData<QDateTime>( QDateTime( QDate(2007, 3, 7), QTime( 14, 3, 32 ), Qt::UTC ) ) );
    QTest::newRow("fetch-rfc822-internaldate")
        << QByteArray("* 123 FETCH (InternalDate \"07-Mar-2007 15:03:32 +0100\")\r\n")
        << shared_ptr<AbstractResponse>( new Fetch( 123, fetchData ) );

    fetchData.clear();
    fetchData[ "INTERNALDATE" ] = std::tr1::shared_ptr<AbstractData>(
            new RespData<QDateTime>( QDateTime( QDate(1981, 4, 6), QTime( 18, 33, 32 ), Qt::UTC ) ) );
    QTest::newRow("fetch-rfc822-internaldate-shorter")
        << QByteArray("* 123 FETCH (InternalDate \"6-Apr-1981 12:03:32 -0630\")\r\n")
        << shared_ptr<AbstractResponse>( new Fetch( 123, fetchData ) );

    fetchData.clear();
    QDateTime date( QDate( 1996, 7, 17 ), QTime( 9, 23, 25 ), Qt::UTC );
    QString subject( "IMAP4rev1 WG mtg summary and minutes");
    QList<MailAddress> from, sender, replyTo, to, cc, bcc;
    from.append( MailAddress( "Terry Gray", QByteArray(), "gray", "cac.washington.edu") );
    sender = replyTo = from;
    to.append( MailAddress( QByteArray(), QByteArray(), "imap", "cac.washington.edu") );
    cc.append( MailAddress( QByteArray(), QByteArray(), "minutes", "CNRI.Reston.VA.US") );
    cc.append( MailAddress( "John Klensin", QByteArray(), "KLENSIN", "MIT.EDU") );
    QByteArray messageId( "<B27397-0100000@cac.washington.edu>" );
    fetchData[ "ENVELOPE" ] = std::tr1::shared_ptr<AbstractData>(
            new RespData<Envelope>(
                Envelope( date, subject, from, sender, replyTo, to, cc, bcc, QByteArray(), messageId )
                ) );
    QTest::newRow("fetch-envelope")
        << QByteArray( "* 12 FETCH (ENVELOPE (\"Wed, 17 Jul 1996 02:23:25 -0700 (PDT)\" "
            "\"IMAP4rev1 WG mtg summary and minutes\" "
            "((\"Terry Gray\" NIL \"gray\" \"cac.washington.edu\")) "
            "((\"Terry Gray\" NIL \"gray\" \"cac.washington.edu\")) "
            "((\"Terry Gray\" NIL \"gray\" \"cac.washington.edu\")) "
            "((NIL NIL \"imap\" \"cac.washington.edu\")) "
            "((NIL NIL \"minutes\" \"CNRI.Reston.VA.US\") "
            "(\"John Klensin\" NIL \"KLENSIN\" \"MIT.EDU\")) NIL NIL "
            "\"<B27397-0100000@cac.washington.edu>\"))\r\n" )
        << shared_ptr<AbstractResponse>( new Fetch( 12, fetchData ) );

    fetchData.clear();
    fetchData[ "ENVELOPE" ] = std::tr1::shared_ptr<AbstractData>(
            new RespData<Envelope>(
                Envelope( QDateTime(), subject, from, sender, replyTo, to, cc, bcc, QByteArray(), messageId )
                ) );
    QTest::newRow("fetch-envelope-nildate")
        << QByteArray( "* 13 FETCH (ENVELOPE (NIL "
            "\"IMAP4rev1 WG mtg summary and minutes\" "
            "((\"Terry Gray\" NIL \"gray\" \"cac.washington.edu\")) "
            "((\"Terry Gray\" NIL \"gray\" \"cac.washington.edu\")) "
            "((\"Terry Gray\" NIL \"gray\" \"cac.washington.edu\")) "
            "((NIL NIL \"imap\" \"cac.washington.edu\")) "
            "((NIL NIL \"minutes\" \"CNRI.Reston.VA.US\") "
            "(\"John Klensin\" NIL \"KLENSIN\" \"MIT.EDU\")) NIL NIL "
            "\"<B27397-0100000@cac.washington.edu>\"))\r\n" )
        << shared_ptr<AbstractResponse>( new Fetch( 13, fetchData ) );

    // FIXME: more unit tests for ENVELOPE and BODY[

    fetchData.clear();
    AbstractMessage::bodyFldParam_t bodyFldParam;
    AbstractMessage::bodyFldDsp_t bodyFldDsp;
    bodyFldParam[ "charset" ] = "UTF-8";
    bodyFldParam[ "format" ] = "flowed";
    fetchData[ "BODYSTRUCTURE" ] = std::tr1::shared_ptr<AbstractData>(
            new TextMessage( "text", "plain", bodyFldParam, QByteArray(), QByteArray(),
                "8bit", 362, QByteArray(), bodyFldDsp, QList<QByteArray>(),
                QByteArray(), QVariant(), 15 )
            );
    QTest::newRow("fetch-bodystructure-plain") <<
        QByteArray( "* 3 FETCH (BODYSTRUCTURE (\"text\" \"plain\" (\"charset\" \"UTF-8\" \"format\" \"flowed\") NIL NIL \"8bit\" 362 15 NIL NIL NIL))\r\n" ) <<
        shared_ptr<AbstractResponse>( new Fetch( 3, fetchData ) );


    fetchData.clear();
    QList<std::tr1::shared_ptr<AbstractMessage> > msgList;
    // 1.1
    bodyFldParam.clear();
    bodyFldDsp = AbstractMessage::bodyFldDsp_t();
    bodyFldParam[ "charset" ] = "US-ASCII";
    bodyFldParam[ "delsp" ] = "yes";
    bodyFldParam[ "format" ] = "flowed";
    msgList.append( std::tr1::shared_ptr<AbstractMessage>(
                new TextMessage( "text", "plain", bodyFldParam, QByteArray(), 
                    QByteArray(), "7bit", 990, QByteArray(), bodyFldDsp,
                    QList<QByteArray>(), QByteArray(), QVariant(), 27 )
                ) );
    // 1.2
    bodyFldParam.clear();
    bodyFldParam[ "x-mac-type" ] = "70674453";
    bodyFldParam[ "name" ] = "PGP.sig";
    bodyFldDsp = AbstractMessage::bodyFldDsp_t();
    bodyFldDsp.first = "inline";
    bodyFldDsp.second[ "filename" ] = "PGP.sig";
    msgList.append( std::tr1::shared_ptr<AbstractMessage>(
                new TextMessage( "application", "pgp-signature", bodyFldParam,
                    QByteArray(), "This is a digitally signed message part",
                    "7bit", 193, QByteArray(), bodyFldDsp, QList<QByteArray>(),
                    QByteArray(), QVariant(), 0 )
                ) );
    // 1
    bodyFldParam.clear();
    bodyFldParam[ "protocol" ] = "application/pgp-signature";
    bodyFldParam[ "micalg" ] = "pgp-sha1";
    bodyFldParam[ "boundary" ] = "Apple-Mail-10--856231115";
    bodyFldDsp = AbstractMessage::bodyFldDsp_t();
    fetchData[ "BODYSTRUCTURE" ] = std::tr1::shared_ptr<AbstractData>(
            new MultiMessage( msgList, "signed", bodyFldParam, bodyFldDsp, QList<QByteArray>(), QByteArray(), QVariant() )
            );
    QTest::newRow("fetch-bodystructure-signed") <<
        QByteArray("* 1 FETCH (BODYSTRUCTURE ((\"text\" \"plain\" (\"charset\" \"US-ASCII\" \"delsp\" \"yes\" \"format\" \"flowed\") NIL NIL \"7bit\" 990 27 NIL NIL NIL)(\"application\" \"pgp-signature\" (\"x-mac-type\" \"70674453\" \"name\" \"PGP.sig\") NIL \"This is a digitally signed message part\" \"7bit\" 193 NIL (\"inline\" (\"filename\" \"PGP.sig\")) NIL) \"signed\" (\"protocol\" \"application/pgp-signature\" \"micalg\" \"pgp-sha1\" \"boundary\" \"Apple-Mail-10--856231115\") NIL NIL))\r\n") <<
        shared_ptr<AbstractResponse>( new Fetch( 1, fetchData ) );

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
