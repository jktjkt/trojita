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

#include <QBuffer>
#include <QFile>
#include <QTest>
#include "Imap/Parser/Message.h"
#include "Streams/FakeSocket.h"

#include "test_Imap_Parser_parse.h"
#include "Utils/headless_test.h"

Q_DECLARE_METATYPE(QSharedPointer<Imap::Responses::AbstractResponse>)
Q_DECLARE_METATYPE(Imap::Responses::State)
Q_DECLARE_METATYPE(Imap::Sequence)

void ImapParserParseTest::initTestCase()
{
    array.reset( new QByteArray() );
    Streams::Socket* sock(new Streams::FakeSocket(Imap::CONN_STATE_CONNECTED_PRETLS_PRECAPS));
    parser = new Imap::Parser( this, sock, 666 );
}

void ImapParserParseTest::cleanupTestCase()
{
    delete parser;
    parser = 0;
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
}

/** @short Test tagged response parsing */
void ImapParserParseTest::testParseTagged()
{
    QFETCH( QByteArray, line );
    QFETCH( QSharedPointer<Imap::Responses::AbstractResponse>, response );

    Q_ASSERT( response );
    QCOMPARE( *(parser->parseTagged( line )), *response );
}

void ImapParserParseTest::testParseTagged_data()
{
    using namespace Imap::Responses;

    QTest::addColumn<QByteArray>("line");
    QTest::addColumn<QSharedPointer<AbstractResponse> >("response");

    QSharedPointer<AbstractData> voidData( new RespData<void>() );
    QSharedPointer<AbstractData> emptyList(
            new RespData<QStringList>( QStringList() ) );

    QTest::newRow("tagged-ok-simple")
        << QByteArray("y01 OK Everything works, man!\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "Everything works, man!", NONE, voidData ) );

    QTest::newRow("tagged-no-simple")
        << QByteArray("12345 NO Nope, something is broken\r\n")
        << QSharedPointer<AbstractResponse>( new State("12345", NO, "Nope, something is broken", NONE, voidData ) );

    QTest::newRow("tagged-bad-simple")
        << QByteArray("ahoj BaD WTF?\r\n")
        << QSharedPointer<AbstractResponse>( new State("ahoj", BAD, "WTF?", NONE, voidData ) );

    QTest::newRow("tagged-ok-alert")
        << QByteArray("y01 oK [ALERT] Server on fire\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "Server on fire", ALERT, voidData ) );
    QTest::newRow("tagged-no-alert")
        << QByteArray("1337 no [ALeRT] Server on fire\r\n")
        << QSharedPointer<AbstractResponse>( new State("1337", NO, "Server on fire", ALERT, voidData ) );

    QTest::newRow("tagged-ok-capability")
        << QByteArray("y01 OK [CAPaBILITY blurdybloop IMAP4rev1 WTF] Capabilities updated\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "Capabilities updated", CAPABILITIES,
                        QSharedPointer<AbstractData>(
                            new RespData<QStringList>( QStringList() << "blurdybloop" << "IMAP4rev1" << "WTF") ) ) );

    QTest::newRow("tagged-ok-parse")
        << QByteArray("y01 OK [PArSE] Parse error. What do you feed me with?\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "Parse error. What do you feed me with?",
                    PARSE, voidData) );
    QTest::newRow("tagged-ok-permanentflags-empty")
        << QByteArray("y01 OK [PERMANENTfLAGS] Behold, the flags!\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "Behold, the flags!", PERMANENTFLAGS, emptyList) );
    QTest::newRow("tagged-ok-permanentflags-flags")
        << QByteArray("y01 OK [PErMANENTFLAGS (\\Foo \\Bar SmrT)] Behold, the flags!\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "Behold, the flags!", PERMANENTFLAGS,
                    QSharedPointer<AbstractData>( new RespData<QStringList>(
                            QStringList() << "\\Foo" << "\\Bar" << "SmrT" ))) );
    QTest::newRow("tagged-ok-permanentflags-flags-not-enclosed")
        << QByteArray("y01 OK [PErMANENTFLAGS \\Foo \\Bar SmrT] Behold, the flags!\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "Behold, the flags!", PERMANENTFLAGS,
                    QSharedPointer<AbstractData>( new RespData<QStringList>(
                            QStringList() << "\\Foo" << "\\Bar" << "SmrT" ))) );
    QTest::newRow("tagged-ok-badcharset-empty")
        << QByteArray("y01 OK [BadCharset] foo\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "foo", BADCHARSET, emptyList) );
    QTest::newRow("tagged-ok-badcharset-something")
        << QByteArray("y01 OK [BADCHARSET (utf8)] wrong charset\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "wrong charset", BADCHARSET,
                    QSharedPointer<AbstractData>( new RespData<QStringList>( QStringList() << "utf8" ))) );
    QTest::newRow("tagged-ok-badcharset-not-enclosed")
        << QByteArray("y01 OK [badcharset a ba cc] Behold, the charset!\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "Behold, the charset!", BADCHARSET,
                    QSharedPointer<AbstractData>( new RespData<QStringList>(
                            QStringList() << "a" << "ba" << "cc" ))) );
    QTest::newRow("tagged-ok-readonly")
        << QByteArray("333 OK [ReAD-ONLY] No writing for you\r\n")
        << QSharedPointer<AbstractResponse>( new State("333", OK, "No writing for you", READ_ONLY, voidData));
    QTest::newRow("tagged-ok-readwrite")
        << QByteArray("y01 OK [REaD-WRITE] Write!!!\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "Write!!!", READ_WRITE, voidData));
    QTest::newRow("tagged-ok-trycreate")
        << QByteArray("y01 OK [TryCreate] ...would be better :)\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "...would be better :)", TRYCREATE, voidData));
    QTest::newRow("tagged-ok-uidnext")
        << QByteArray("y01 OK [uidNext 5] Next UID\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "Next UID", UIDNEXT,
                    QSharedPointer<AbstractData>( new RespData<uint>( 5 ) )));
    QTest::newRow("tagged-ok-uidvalidity")
        << QByteArray("y01 OK [UIDVALIDITY 17] UIDs valid\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "UIDs valid", UIDVALIDITY,
                    QSharedPointer<AbstractData>( new RespData<uint>( 17 ) )));
    QTest::newRow("tagged-ok-unseen")
        << QByteArray("y01 OK [unSeen 666] I need my glasses\r\n")
        << QSharedPointer<AbstractResponse>( new State("y01", OK, "I need my glasses", UNSEEN,
                    QSharedPointer<AbstractData>( new RespData<uint>( 666 ) )));

    QTest::newRow("appenduid-simple")
            << QByteArray("A003 OK [APPENDUID 38505 3955] APPEND completed\r\n")
            << QSharedPointer<AbstractResponse>(new State("A003", OK, "APPEND completed", APPENDUID,
                                                          QSharedPointer<AbstractData>(
                                                              new RespData<QPair<uint,Imap::Sequence> >(
                                                                  qMakePair(38505u, Imap::Sequence(3955)))
                                                              )));
    /*
    QTest::newRow("appenduid-seq")
            << QByteArray("A003 OK [APPENDUID 38505 3955,333666] APPEND completed\r\n")
            << QSharedPointer<AbstractResponse>(new State("A003", OK, "APPEND completed", APPENDUID,
                                                          QSharedPointer<AbstractData>(
                                                              new RespData<QPair<uint,Imap::Sequence> >(
                                                                  qMakePair(38505u, Imap::Sequence(3955)))
                                                              )));

    QTest::newRow("copyuid-simple")
            << QByteArray("A004 OK [COPYUID 38505 304 3956] Done\r\n")
            << QSharedPointer<AbstractResponse>(new State("A004", OK, "Done", COPYUID,
                                                          QSharedPointer<AbstractData>(
                                                              new RespData<QPair<uint,QPair<Imap::Sequence, Imap::Sequence> > >(
                                                                  qMakePair(38505u,
                                                                            qMakePair(Imap::Sequence(304), Imap::Sequence(3956))
                                                                            ))
                                                              )));


    QTest::newRow("copyuid-sequence")
            << QByteArray("A004 OK [COPYUID 38505 304,319:320 3956:3958] Done\r\n")
            << QSharedPointer<AbstractResponse>(new State("A003", OK, "Done", APPENDUID,
                                                          QSharedPointer<AbstractData>(
                                                              new RespData<QPair<uint,Imap::Sequence> >(
                                                                  qMakePair(38505u, Imap::Sequence(3955)))
                                                              )));
    */
}

/** @short Test untagged response parsing */
void ImapParserParseTest::testParseUntagged()
{
    QFETCH( QByteArray, line );
    QFETCH( QSharedPointer<Imap::Responses::AbstractResponse>, response );

    Q_ASSERT( response );
    QSharedPointer<Imap::Responses::AbstractResponse> r = parser->parseUntagged( line );
    if ( Imap::Responses::Fetch* fetchResult = dynamic_cast<Imap::Responses::Fetch*>( r.data() ) ) {
        fetchResult->data.remove( "x-trojita-bodystructure" );
    }
#if 0// qDebug()'s internal buffer is too small to be useful here, that's why QCOMPARE's normal dumping is not enough
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
    using Imap::Responses::Fetch; // needed for gcc-3.4

    QTest::addColumn<QByteArray>("line");
    QTest::addColumn<QSharedPointer<AbstractResponse> >("response");

    QSharedPointer<AbstractData> voidData( new RespData<void>() );
    QSharedPointer<AbstractData> emptyList(
            new RespData<QStringList>( QStringList() ) );

    QTest::newRow("untagged-ok-simple")
        << QByteArray("* OK Everything works, man!\r\n")
        << QSharedPointer<AbstractResponse>( new State(QByteArray(), OK, "Everything works, man!", NONE, voidData ) );

    QTest::newRow("untagged-no-simple")
        << QByteArray("* NO Nope, something is broken\r\n")
        << QSharedPointer<AbstractResponse>( new State(QByteArray(), NO, "Nope, something is broken", NONE, voidData ) );
    QTest::newRow("untagged-ok-uidvalidity")
        << QByteArray("* OK [UIDVALIDITY 17] UIDs valid\r\n")
        << QSharedPointer<AbstractResponse>( new State(QByteArray(), OK, "UIDs valid", UIDVALIDITY,
                    QSharedPointer<AbstractData>( new RespData<uint>( 17 ) )));
    QTest::newRow("untagged-bye")
        << QByteArray("* BYE go away\r\n")
        << QSharedPointer<AbstractResponse>( new State(QByteArray(), BYE, "go away", NONE, voidData ) );
    QTest::newRow("untagged-bye-empty")
        << QByteArray("* BYE\r\n")
        << QSharedPointer<AbstractResponse>( new State(QByteArray(), BYE, QString(), NONE, voidData ) );
    QTest::newRow("untagged-no-somerespcode-empty")
        << QByteArray("* NO [ALERT]\r\n")
        << QSharedPointer<AbstractResponse>( new State(QByteArray(), NO, QString(), ALERT, voidData ) );

    QTest::newRow("untagged-expunge")
        << QByteArray("* 1337 Expunge\r\n")
        << QSharedPointer<AbstractResponse>( new NumberResponse( EXPUNGE, 1337 ) );
    QTest::newRow("untagged-exists")
        << QByteArray("* 3 exIsts\r\n")
        << QSharedPointer<AbstractResponse>( new NumberResponse( EXISTS, 3 ) );
    QTest::newRow("untagged-recent")
        << QByteArray("* 666 recenT\r\n")
        << QSharedPointer<AbstractResponse>( new NumberResponse( RECENT, 666 ) );

    QTest::newRow("untagged-capability")
        << QByteArray("* CAPABILITY fooBar IMAP4rev1 blah\r\n")
        << QSharedPointer<AbstractResponse>( new Capability( QStringList() << "fooBar" << "IMAP4rev1" << "blah" ) );

    QTest::newRow("untagged-list")
        << QByteArray("* LIST (\\Noselect) \".\" \"\"\r\n")
        << QSharedPointer<AbstractResponse>( new List( LIST, QStringList() << "\\Noselect", ".", "", QMap<QByteArray,QVariant>()) );
    QTest::newRow("untagged-lsub")
        << QByteArray("* LSUB (\\Noselect) \".\" \"\"\r\n")
        << QSharedPointer<AbstractResponse>( new List( LSUB, QStringList() << "\\Noselect", ".", "", QMap<QByteArray,QVariant>() ) );
    QTest::newRow("untagged-list-moreflags")
        << QByteArray("* LIST (\\Noselect Blesmrt) \".\" \"\"\r\n")
        << QSharedPointer<AbstractResponse>( new List( LIST, QStringList() << "\\Noselect" << "Blesmrt", ".", "", QMap<QByteArray,QVariant>() ) );
    QTest::newRow("untagged-list-mailbox")
        << QByteArray("* LIST () \".\" \"someName\"\r\n")
        << QSharedPointer<AbstractResponse>( new List( LIST, QStringList(), ".", "someName", QMap<QByteArray,QVariant>() ) );
    QTest::newRow("untagged-list-mailbox-atom")
        << QByteArray("* LIST () \".\" someName\r\n")
        << QSharedPointer<AbstractResponse>( new List( LIST, QStringList(), ".", "someName", QMap<QByteArray,QVariant>() ) );
    QTest::newRow("untagged-list-separator-nil")
        << QByteArray("* LIST () NiL someName\r\n")
        << QSharedPointer<AbstractResponse>( new List( LIST, QStringList(), QByteArray(), "someName", QMap<QByteArray,QVariant>() ) );
    QTest::newRow("untagged-list-mailbox-quote")
        << QByteArray("* LIST () \".\" \"some\\\"Name\"\r\n")
        << QSharedPointer<AbstractResponse>( new List( LIST, QStringList(), ".", "some\"Name", QMap<QByteArray,QVariant>() ) );
    QTest::newRow("untagged-list-unicode")
        << QByteArray("* LIST () \"/\" \"~Peter/mail/&U,BTFw-/&ZeVnLIqe-\"\r\n")
        << QSharedPointer<AbstractResponse>( new List( LIST, QStringList(), "/", QString::fromUtf8("~Peter/mail/台北/日本語"), QMap<QByteArray,QVariant>() ) );
    QTest::newRow("untagged-list-inbox-atom")
        << QByteArray("* LIST () \"/\" inBox\r\n")
        << QSharedPointer<AbstractResponse>( new List( LIST, QStringList(), "/", "INBOX", QMap<QByteArray,QVariant>()) );
    QTest::newRow("untagged-list-inbox-quoted")
        << QByteArray("* LIST () \"/\" \"inBox\"\r\n")
        << QSharedPointer<AbstractResponse>( new List( LIST, QStringList(), "/", "INBOX", QMap<QByteArray,QVariant>()) );
    QTest::newRow("untagged-list-inbox-literal")
            << QByteArray("* LIST () \"/\" {5}\r\ninBox\r\n")
        << QSharedPointer<AbstractResponse>( new List( LIST, QStringList(), "/", "INBOX", QMap<QByteArray,QVariant>()) );

    QMap<QByteArray,QVariant> listExtData;
    listExtData["CHILDINFO"] = QStringList() << "SUBSCRIBED";
    QTest::newRow("untagged-list-ext-childinfo")
        << QByteArray("* LIST () \"/\" \"Foo\" (\"CHILDINFO\" (\"SUBSCRIBED\"))\r\n")
        << QSharedPointer<AbstractResponse>(new List( LIST, QStringList(), "/", "Foo", listExtData));
    listExtData["OLDNAME"] = QStringList() << "blesmrt";
    QTest::newRow("untagged-list-ext-childinfo-2")
        << QByteArray("* LIST () \"/\" \"Foo\" (\"CHILDINFO\" (\"SUBSCRIBED\") \"OLDNAME\" (\"blesmrt\"))\r\n")
        << QSharedPointer<AbstractResponse>(new List( LIST, QStringList(), "/", "Foo", listExtData));

    QTest::newRow("untagged-flags")
        << QByteArray("* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n")
        << QSharedPointer<AbstractResponse>(
                new Flags( QStringList() << "\\Answered" << "\\Flagged" <<
                    "\\Deleted" << "\\Seen" << "\\Draft" ) );
    QTest::newRow("search-empty")
        << QByteArray("* SEARCH\r\n")
        << QSharedPointer<AbstractResponse>( new Search( QList<uint>() ) );
    QTest::newRow("search-messages")
        << QByteArray("* SEARCH 1 33 666\r\n")
        << QSharedPointer<AbstractResponse>( new Search( QList<uint>() << 1 << 33 << 666 ) );

    ESearch::ListData_t esearchData;
    QTest::newRow("esearch-empty")
        << QByteArray("* ESEARCH\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::SEQUENCE, esearchData));

    QTest::newRow("esearch-empty-tag")
        << QByteArray("* ESEARCH (TAG x)\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch("x", ESearch::SEQUENCE, esearchData));

    QTest::newRow("esearch-empty-uid")
        << QByteArray("* ESEARCH UiD\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::UIDS, esearchData));

    QTest::newRow("esearch-empty-uid-tag")
        << QByteArray("* ESEARCH (TAG \"1\") UiD\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch("1", ESearch::UIDS, esearchData));

    esearchData.push_back(qMakePair<QByteArray, QList<uint> >("BLAH", QList<uint>() << 10));
    QTest::newRow("esearch-one-number")
        << QByteArray("* ESEARCH BLaH 10\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::SEQUENCE, esearchData));

    QTest::newRow("esearch-uid-one-number")
        << QByteArray("* ESEArCH UiD BLaH 10\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::UIDS, esearchData));

    QTest::newRow("esearch-tag-one-number")
        << QByteArray("* ESEArCH (TaG x) BLaH 10\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch("x", ESearch::SEQUENCE, esearchData));

    QTest::newRow("esearch-uid-tag-one-number")
        << QByteArray("* ESEArCH (TaG x) Uid BLaH 10\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch("x", ESearch::UIDS, esearchData));

    esearchData.push_front(qMakePair<QByteArray, QList<uint> >("FOO", QList<uint>() << 666));
    esearchData.push_front(qMakePair<QByteArray, QList<uint> >("FOO", QList<uint>() << 333));
    QTest::newRow("esearch-two-numbers")
        << QByteArray("* ESEARCH fOO 333 foo 666   BLaH 10\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::SEQUENCE, esearchData));

    esearchData.clear();
    esearchData.push_back(qMakePair<QByteArray, QList<uint> >("FOO", QList<uint>() << 333));
    esearchData.push_back(qMakePair<QByteArray, QList<uint> >("BLAH", QList<uint>() << 10));
    QTest::newRow("esearch-uid-two-numbers")
        << QByteArray("* ESEArCH UiD foo    333 BLaH  10\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::UIDS, esearchData));

    QTest::newRow("esearch-tag-two-numbers")
        << QByteArray("* ESEArCH (TaG x)    foo 333 BLaH 10  \r\n")
        << QSharedPointer<AbstractResponse>(new ESearch("x", ESearch::SEQUENCE, esearchData));

    QTest::newRow("esearch-uid-tag-two-numbers")
        << QByteArray("* ESEArCH    (TaG   x)   Uid  foo 333 BLaH 10\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch("x", ESearch::UIDS, esearchData));

    esearchData.clear();
    esearchData.push_back(qMakePair<QByteArray, QList<uint> >("BLAH", QList<uint>() << 10 << 11 << 13 << 14 << 15 << 16 << 17));
    QTest::newRow("esearch-one-list-1")
        << QByteArray("* ESEARCH BLaH 10,11,13:17\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::SEQUENCE, esearchData));

    esearchData.clear();
    esearchData.push_back(qMakePair<QByteArray, QList<uint> >("BLAH", QList<uint>() << 1 << 2));
    QTest::newRow("esearch-one-list-2")
        << QByteArray("* ESEARCH BLaH 1:2\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::SEQUENCE, esearchData));

    QTest::newRow("esearch-one-list-3")
        << QByteArray("* ESEARCH BLaH    1,2\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::SEQUENCE, esearchData));

    esearchData.clear();
    esearchData.push_back(qMakePair<QByteArray, QList<uint> >("BLAH", QList<uint>() << 1 << 2 << 3 << 4 << 5));
    QTest::newRow("esearch-one-list-4")
        << QByteArray("* ESEARCH BLaH 1,2:4,5\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::SEQUENCE, esearchData));

    QTest::newRow("esearch-one-list-extra-space-at-end")
        << QByteArray("* ESEARCH BLaH 1,2:4,5   \r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::SEQUENCE, esearchData));

    esearchData.push_back(qMakePair<QByteArray, QList<uint> >("FOO", QList<uint>() << 6));
    QTest::newRow("esearch-mixed-1")
        << QByteArray("* ESEARCH BLaH 1,2:4,5 FOO 6\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::SEQUENCE, esearchData));

    esearchData.swap(0, 1);
    QTest::newRow("esearch-mixed-2")
        << QByteArray("* ESEARCH FOO 6 BLaH 1,2:4,5\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::SEQUENCE, esearchData));

    esearchData.push_back(qMakePair<QByteArray, QList<uint> >("BAZ", QList<uint>() << 33));
    QTest::newRow("esearch-mixed-3")
        << QByteArray("* ESEARCH FOO 6   BLaH 1,2:4,5   baz 33  \r\n")
        << QSharedPointer<AbstractResponse>(new ESearch(QByteArray(), ESearch::SEQUENCE, esearchData));

    ESearch::IncrementalContextData_t incrementalEsearchData;
    incrementalEsearchData.push_back(ESearch::ContextIncrementalItem(ESearch::ContextIncrementalItem::ADDTO, 1, QList<uint>() << 2733));
    incrementalEsearchData.push_back(ESearch::ContextIncrementalItem(ESearch::ContextIncrementalItem::ADDTO, 1, QList<uint>() << 2731 << 2732));
    QTest::newRow("esearch-context-sort-1")
        << QByteArray("* ESEARCH (TAG \"C01\") UID ADDTO (1 2733) ADDTO (1 2731:2732)\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch("C01", ESearch::UIDS, incrementalEsearchData));

    QTest::newRow("esearch-context-sort-2")
        << QByteArray("* ESEARCH (TAG \"C01\") UID ADDTO (1 2733 1 2731:2732)\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch("C01", ESearch::UIDS, incrementalEsearchData));

    incrementalEsearchData.clear();
    incrementalEsearchData.push_back(ESearch::ContextIncrementalItem(ESearch::ContextIncrementalItem::ADDTO, 1, QList<uint>() << 2733));
    incrementalEsearchData.push_back(ESearch::ContextIncrementalItem(ESearch::ContextIncrementalItem::ADDTO, 1, QList<uint>() << 2732));
    incrementalEsearchData.push_back(ESearch::ContextIncrementalItem(ESearch::ContextIncrementalItem::ADDTO, 1, QList<uint>() << 2731));
    QTest::newRow("esearch-context-sort-3")
        << QByteArray("* ESEARCH (TAG \"C01\") UID ADDTO (1 2733 1 2732 1 2731)\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch("C01", ESearch::UIDS, incrementalEsearchData));

    incrementalEsearchData.clear();
    incrementalEsearchData.push_back(ESearch::ContextIncrementalItem(ESearch::ContextIncrementalItem::ADDTO, 1, QList<uint>() << 2731 << 2732 << 2733));
    QTest::newRow("esearch-context-sort-4")
        << QByteArray("* ESEARCH (TAG \"C01\") UID ADDTO (1 2731:2733)\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch("C01", ESearch::UIDS, incrementalEsearchData));

    incrementalEsearchData.clear();
    incrementalEsearchData.push_back(ESearch::ContextIncrementalItem(ESearch::ContextIncrementalItem::REMOVEFROM, 0, QList<uint>() << 32768));
    QTest::newRow("esearch-context-sort-5")
        << QByteArray("* ESEARCH (TAG \"B01\") UID REMOVEFROM (0 32768)\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch("B01", ESearch::UIDS, incrementalEsearchData));

    Status::stateDataType states;
    states[Status::MESSAGES] = 231;
    states[Status::UIDNEXT] = 44292;
    QTest::newRow("status-1")
        << QByteArray("* STATUS blurdybloop (MESSAGES 231 UIDNEXT 44292)\r\n")
        << QSharedPointer<AbstractResponse>( new Status( "blurdybloop", states ) );
    states[Status::UIDVALIDITY] = 1337;
    states[Status::RECENT] = 3234567890u;
    QTest::newRow("status-2")
        << QByteArray("* STATUS blurdybloop (MESSAGES 231 UIDNEXT 44292 UIDVALIDITY 1337 RECENT 3234567890)\r\n")
        << QSharedPointer<AbstractResponse>( new Status( "blurdybloop", states ) );

    // notice the trailing whitespace
    QTest::newRow("status-exchange-botched-1")
        << QByteArray("* STATUS blurdybloop (MESSAGES 231 UIDNEXT 44292 UIDVALIDITY 1337 RECENT 3234567890) \r\n")
        << QSharedPointer<AbstractResponse>( new Status( "blurdybloop", states ) );

    states.clear();
    states[Status::MESSAGES] = 113;
    QTest::newRow("status-extra-whitespace-around-atoms")
            << QByteArray("* STATUS blurdybloop ( MESSAGES 113 )\r\n")
            << QSharedPointer<AbstractResponse>( new Status( "blurdybloop", states ) );


    QTest::newRow("namespace-1")
        << QByteArray("* Namespace nil NIL nil\r\n")
        << QSharedPointer<AbstractResponse>( new Namespace( QList<NamespaceData>(),
                    QList<NamespaceData>(), QList<NamespaceData>() ) );

    QTest::newRow("namespace-2")
        << QByteArray("* Namespace () NIL nil\r\n")
        << QSharedPointer<AbstractResponse>( new Namespace( QList<NamespaceData>(),
                    QList<NamespaceData>(), QList<NamespaceData>() ) );

    QTest::newRow("namespace-3")
        << QByteArray("* Namespace ((\"\" \"/\")) NIL nil\r\n")
        << QSharedPointer<AbstractResponse>( new Namespace( QList<NamespaceData>() << NamespaceData( "", "/" ),
                    QList<NamespaceData>(), QList<NamespaceData>() ) );

    QTest::newRow("namespace-4")
        << QByteArray("* Namespace ((\"prefix\" \".\")(\"\" \"/\")) ((\"blesmrt\" \"trojita\")) ((\"foo\" \"bar\"))\r\n")
        << QSharedPointer<AbstractResponse>( new Namespace(
                    QList<NamespaceData>() << NamespaceData( "prefix", "." ) << NamespaceData( "", "/" ),
                    QList<NamespaceData>() << NamespaceData( "blesmrt", "trojita" ),
                    QList<NamespaceData>() << NamespaceData( "foo", "bar" ) ) );

    QTest::newRow("namespace-5")
        << QByteArray("* NAMESPACE ((\"\" \"/\")(\"#mhinbox\" NIL)(\"#mh/\" \"/\")) ((\"~\" \"/\")) "
                "((\"#shared/\" \"/\")(\"#ftp/\" \"/\")(\"#news.\" \".\")(\"#public/\" \"/\"))\r\n")
        << QSharedPointer<AbstractResponse>( new Namespace(
                    QList<NamespaceData>() << NamespaceData( "", "/" ) << NamespaceData( "#mhinbox", QByteArray() )
                        << NamespaceData( "#mh/", "/" ),
                    QList<NamespaceData>() << NamespaceData( "~", "/" ),
                    QList<NamespaceData>() << NamespaceData( "#shared/", "/" ) << NamespaceData( "#ftp/", "/" )
                        << NamespaceData( "#news.", "." ) << NamespaceData( "#public/", "/" )
                    ) );

    QTest::newRow("sort-1")
        << QByteArray("* SORT") << QSharedPointer<AbstractResponse>( new Sort( QList<uint>() ) );
    QTest::newRow("sort-2")
        << QByteArray("* SORT ") << QSharedPointer<AbstractResponse>( new Sort( QList<uint>() ) );
    QTest::newRow("sort-3")
        << QByteArray("* SORT 13 1 6 5 7 9 10 11 12 4 3 2 8\r\n")
        << QSharedPointer<AbstractResponse>( new Sort( QList<uint>() << 13 << 1 << 6 << 5 << 7 << 9 << 10 << 11 << 12 << 4 << 3 << 2 << 8 ) );

    ThreadingNode node2(2), node3(3), node6(6), node4(4), node23(23), node44(44), node7(7), node96(96);
    QVector<ThreadingNode> rootNodes;
    node4.children << node23;
    node7.children << node96;
    node44.children << node7;
    node6.children << node4 << node44;
    node3.children << node6;
    rootNodes << node2 << node3;
    QTest::newRow("thread-1")
        << QByteArray("* THREAD (2)(3 6 (4 23)(44 7 96))\r\n")
        << QSharedPointer<AbstractResponse>( new Thread( rootNodes ) );

    rootNodes.clear();
    ThreadingNode node203(3), node205(5), anonymousNode201;
    anonymousNode201.children << node203 << node205;
    rootNodes << anonymousNode201;
    QTest::newRow("thread-2")
        << QByteArray("* THREAD ((3)(5))\r\n")
        << QSharedPointer<AbstractResponse>( new Thread( rootNodes ) );

    rootNodes.clear();
    ThreadingNode node301(1), node302(2), node303(3);
    rootNodes << node301 << node302 << node303;
    QTest::newRow("thread-3")
        << QByteArray("* THREAD (1)(2)(3)\r\n")
        << QSharedPointer<AbstractResponse>( new Thread( rootNodes ) );

    rootNodes.clear();
    ThreadingNode node401(1), node402(2), node403(3);
    node401.children << node402 << node403;
    rootNodes << node401;
    QTest::newRow("thread-4")
        << QByteArray("* THREAD (1(2)(3))\r\n")
        << QSharedPointer<AbstractResponse>( new Thread( rootNodes ) );

    rootNodes.clear();
    ThreadingNode node502(2), node503(3), node506(6), node504(4), node523(23),
        node544(44), node507(7), node596(96), node513(13), node566(66), anon501, anon502;
    node504.children << node523;
    node507.children << node596;
    node544.children << node507;
    anon502.children << node566;
    anon501.children << anon502;
    node506.children << node504 << node544 << node513 << anon501;
    node503.children << node506;
    rootNodes << node502 << node503;
    QTest::newRow("thread-5")
        << QByteArray("* THREAD (2)(3 6 (4 23)(44 7 96) (13) (((66))))\r\n")
        << QSharedPointer<AbstractResponse>( new Thread( rootNodes ) );

    rootNodes.clear();
    ThreadingNode node608(8), node602(2), node603(3), node604(4), node607(7), node609(9),
        node610(10), node611(11), node612(12), node605(5), node601(1), node606(6), node613(13);
    node603.children << node604;
    node602.children << node603;
    rootNodes << node608 << node602 << node607 << node609 << node610;
    rootNodes << ThreadingNode(0, QVector<ThreadingNode>() << node611 << node612 );
    rootNodes << node605;
    rootNodes << ThreadingNode(0, QVector<ThreadingNode>() << node601 << node606 );
    rootNodes << node613;
    QTest::newRow("thread-6")
        << QByteArray("* THREAD (8)(2 3 4)(7)(9)(10)((11)(12))(5)((1)(6))(13)\r\n")
        << QSharedPointer<AbstractResponse>( new Thread( rootNodes ) );

    esearchData.clear();
    ESearch::IncrementalThreadingData_t esearchThreading;
    esearchThreading.push_back(ESearch::IncrementalThreadingItem_t(666, rootNodes));
    QTest::newRow("esearch-incthread-single")
        << QByteArray("* ESEARCH (TAG \"x\") UID INCTHREAD 666 (8)(2 3 4)(7)(9)(10)((11)(12))(5)((1)(6))(13)\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch("x", ESearch::UIDS, esearchThreading));
    rootNodes.clear();
    rootNodes << ThreadingNode(123, QVector<ThreadingNode>());
    esearchThreading.push_back(ESearch::IncrementalThreadingItem_t(333, rootNodes));
    QTest::newRow("esearch-incthread-two")
        << QByteArray("* ESEARCH (TAG \"x\") UID INCTHREAD 666 (8)(2 3 4)(7)(9)(10)((11)(12))(5)((1)(6))(13) "
                      "INCTHREAD 333 (123)\r\n")
        << QSharedPointer<AbstractResponse>(new ESearch("x", ESearch::UIDS, esearchThreading));

    Fetch::dataType fetchData;

    QTest::newRow("fetch-empty")
        << QByteArray("* 66 FETCH ()\r\n")
        << QSharedPointer<AbstractResponse>( new Fetch( 66, fetchData ) );

    fetchData.clear();
    fetchData[ "RFC822.SIZE" ] = QSharedPointer<AbstractData>( new RespData<uint>( 1337 ) );
    QTest::newRow("fetch-rfc822-size")
        << QByteArray("* 123 FETCH (rfc822.size 1337)\r\n")
        << QSharedPointer<AbstractResponse>( new Fetch( 123, fetchData ) );

    fetchData.clear();
    fetchData[ "RFC822.SIZE" ] = QSharedPointer<AbstractData>( new RespData<uint>( 1337 ) );
    fetchData[ "UID" ] = QSharedPointer<AbstractData>( new RespData<uint>( 666 ) );
    QTest::newRow("fetch-rfc822-size-uid")
        << QByteArray("* 123 FETCH (uID 666 rfc822.size 1337)\r\n")
        << QSharedPointer<AbstractResponse>( new Fetch( 123, fetchData ) );
    QTest::newRow("fetch-rfc822-size-uid-swapped")
        << QByteArray("* 123 FETCH (rfc822.size 1337 uId 666)\r\n")
        << QSharedPointer<AbstractResponse>( new Fetch( 123, fetchData ) );

    fetchData.clear();
    fetchData[ "RFC822.HEADER" ] = QSharedPointer<AbstractData>( new RespData<QByteArray>( "123456789012" ) );
    QTest::newRow("fetch-rfc822-header")
        << QByteArray("* 123 FETCH (rfc822.header {12}\r\n123456789012)\r\n")
        << QSharedPointer<AbstractResponse>( new Fetch( 123, fetchData ) );

    fetchData.clear();
    fetchData[ "RFC822.TEXT" ] = QSharedPointer<AbstractData>( new RespData<QByteArray>( "abcdEf" ) );
    QTest::newRow("fetch-rfc822-text")
        << QByteArray("* 123 FETCH (rfc822.tExt abcdEf)\r\n")
        << QSharedPointer<AbstractResponse>( new Fetch( 123, fetchData ) );

    fetchData.clear();
    fetchData[ "INTERNALDATE" ] = QSharedPointer<AbstractData>(
            new RespData<QDateTime>( QDateTime( QDate(2007, 3, 7), QTime( 14, 3, 32 ), Qt::UTC ) ) );
    QTest::newRow("fetch-rfc822-internaldate")
        << QByteArray("* 123 FETCH (InternalDate \"07-Mar-2007 15:03:32 +0100\")\r\n")
        << QSharedPointer<AbstractResponse>( new Fetch( 123, fetchData ) );

    fetchData.clear();
    fetchData[ "INTERNALDATE" ] = QSharedPointer<AbstractData>(
            new RespData<QDateTime>( QDateTime( QDate(1981, 4, 6), QTime( 18, 33, 32 ), Qt::UTC ) ) );
    QTest::newRow("fetch-rfc822-internaldate-shorter")
        << QByteArray("* 123 FETCH (InternalDate \"6-Apr-1981 12:03:32 -0630\")\r\n")
        << QSharedPointer<AbstractResponse>( new Fetch( 123, fetchData ) );

    fetchData.clear();
    QDateTime date( QDate( 1996, 7, 17 ), QTime( 9, 23, 25 ), Qt::UTC );
    QString subject( "IMAP4rev1 WG mtg summary and minutes");
    QList<MailAddress> from, sender, replyTo, to, cc, bcc;
    from.append( MailAddress( "Terry Gray", QByteArray(), "gray", "cac.washington.edu") );
    sender = replyTo = from;
    to.append( MailAddress( QByteArray(), QByteArray(), "imap", "cac.washington.edu") );
    cc.append( MailAddress( QByteArray(), QByteArray(), "minutes", "CNRI.Reston.VA.US") );
    cc.append( MailAddress( "John Klensin", QByteArray(), "KLENSIN", "MIT.EDU") );
    QByteArray messageId( "B27397-0100000@cac.washington.edu" );
    fetchData[ "ENVELOPE" ] = QSharedPointer<AbstractData>(
            new RespData<Envelope>(
                Envelope( date, subject, from, sender, replyTo, to, cc, bcc, QList<QByteArray>(), messageId )
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
        << QSharedPointer<AbstractResponse>( new Fetch( 12, fetchData ) );

    fetchData.clear();
    fetchData[ "ENVELOPE" ] = QSharedPointer<AbstractData>(
            new RespData<Envelope>(
                Envelope( QDateTime(), subject, from, sender, replyTo, to, cc, bcc, QList<QByteArray>(), messageId )
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
        << QSharedPointer<AbstractResponse>( new Fetch( 13, fetchData ) );

    // FIXME: more unit tests for ENVELOPE and BODY[

    fetchData.clear();
    AbstractMessage::bodyFldParam_t bodyFldParam;
    AbstractMessage::bodyFldDsp_t bodyFldDsp;
    bodyFldParam[ "CHARSET" ] = "UTF-8";
    bodyFldParam[ "FORMAT" ] = "flowed";
    fetchData[ "BODYSTRUCTURE" ] = QSharedPointer<AbstractData>(
            new TextMessage( "text", "plain", bodyFldParam, QByteArray(), QByteArray(),
                "8bit", 362, QByteArray(), bodyFldDsp, QList<QByteArray>(),
                QByteArray(), QVariant(), 15 )
            );
    QTest::newRow("fetch-bodystructure-plain") <<
        QByteArray( "* 3 FETCH (BODYSTRUCTURE (\"text\" \"plain\" (\"chaRset\" \"UTF-8\" \"format\" \"flowed\") NIL NIL \"8bit\" 362 15 NIL NIL NIL))\r\n" ) <<
        QSharedPointer<AbstractResponse>( new Fetch( 3, fetchData ) );


    fetchData.clear();
    QList<QSharedPointer<AbstractMessage> > msgList;
    // 1.1
    bodyFldParam.clear();
    bodyFldDsp = AbstractMessage::bodyFldDsp_t();
    bodyFldParam[ "CHARSET" ] = "US-ASCII";
    bodyFldParam[ "DELSP" ] = "yes";
    bodyFldParam[ "FORMAT" ] = "flowed";
    msgList.append( QSharedPointer<AbstractMessage>(
                new TextMessage( "text", "plain", bodyFldParam, QByteArray(),
                    QByteArray(), "7bit", 990, QByteArray(), bodyFldDsp,
                    QList<QByteArray>(), QByteArray(), QVariant(), 27 )
                ) );
    // 1.2
    bodyFldParam.clear();
    bodyFldParam[ "X-MAC-TYPE" ] = "70674453";
    bodyFldParam[ "NAME" ] = "PGP.sig";
    bodyFldDsp = AbstractMessage::bodyFldDsp_t();
    bodyFldDsp.first = "inline";
    bodyFldDsp.second[ "FILENAME" ] = "PGP.sig";
    msgList.append( QSharedPointer<AbstractMessage>(
                new TextMessage( "application", "pgp-signature", bodyFldParam,
                    QByteArray(), "This is a digitally signed message part",
                    "7bit", 193, QByteArray(), bodyFldDsp, QList<QByteArray>(),
                    QByteArray(), QVariant(), 0 )
                ) );
    // 1
    bodyFldParam.clear();
    bodyFldParam[ "PROTOCOL" ] = "application/pgp-signature";
    bodyFldParam[ "MICALG" ] = "pgp-sha1";
    bodyFldParam[ "BOUNDARY" ] = "Apple-Mail-10--856231115";
    bodyFldDsp = AbstractMessage::bodyFldDsp_t();
    fetchData[ "BODYSTRUCTURE" ] = QSharedPointer<AbstractData>(
            new MultiMessage( msgList, "signed", bodyFldParam, bodyFldDsp, QList<QByteArray>(), QByteArray(), QVariant() )
            );
    QTest::newRow("fetch-bodystructure-signed") <<
        QByteArray("* 1 FETCH (BODYSTRUCTURE ((\"text\" \"plain\" (\"charset\" \"US-ASCII\" \"delsp\" \"yes\" \"format\" \"flowed\") NIL NIL \"7bit\" 990 27 NIL NIL NIL)(\"application\" \"pgp-signature\" (\"x-mac-type\" \"70674453\" \"name\" \"PGP.sig\") NIL \"This is a digitally signed message part\" \"7bit\" 193 NIL (\"inline\" (\"filename\" \"PGP.sig\")) NIL) \"signed\" (\"protocol\" \"application/pgp-signature\" \"micalg\" \"pgp-sha1\" \"boundary\" \"Apple-Mail-10--856231115\") NIL NIL))\r\n") <<
        QSharedPointer<AbstractResponse>( new Fetch( 1, fetchData ) );

    fetchData.clear();
    bodyFldParam.clear();
    bodyFldParam[ "CHARSET" ] = "UTF-8";
    bodyFldParam[ "FORMAT" ] = "flowed";
    fetchData[ "BODYSTRUCTURE" ] = QSharedPointer<AbstractData>(
            new TextMessage( "text", "plain", bodyFldParam, QByteArray(), QByteArray(),
                             "quoted-printable", 0, QByteArray(), bodyFldDsp, QList<QByteArray>(), QByteArray(), QVariant(), 0 ) );
    QTest::newRow("fetch-exchange-screwup-1") <<
            QByteArray("* 61 FETCH "
                       "(BODYSTRUCTURE (\"text\" \"plain\" (\"charset\" \"UTF-8\" \"format\" \"flowed\") "
                       "NIL NIL \"quoted-printable\" -1 -1 NIL NIL NIL NIL))\r\n") <<
            QSharedPointer<AbstractResponse>( new Fetch( 61, fetchData ) );


    // GMail and its flawed representation of a nested message/rfc822
    fetchData.clear();
    from.clear(); from << MailAddress(QLatin1String("somebody"), QString(), QLatin1String("info"), QLatin1String("example.com"));
    sender = replyTo = from;
    to.clear(); to << MailAddress(QLatin1String("destination"), QString(), QLatin1String("foobar"), QLatin1String("gmail.com"));
    cc.clear();
    bcc.clear();
    fetchData["ENVELOPE"] = QSharedPointer<AbstractData>(
            new RespData<Envelope>(
                    Envelope( QDateTime(QDate(2011, 1, 11), QTime(9, 21, 42), Qt::UTC), QLatin1String("blablabla"), from, sender, replyTo, to, cc, bcc, QList<QByteArray>(), QByteArray() )
                    ));
    fetchData["UID"] = QSharedPointer<AbstractData>(new RespData<uint>(8803));
    fetchData["RFC822.SIZE"] = QSharedPointer<AbstractData>(new RespData<uint>(56144));

    msgList.clear();
    bodyFldParam.clear();
    bodyFldParam["CHARSET"] = "iso-8859-2";
    msgList.append(QSharedPointer<AbstractMessage>(
            new TextMessage("text", "plain", bodyFldParam, QByteArray(), QByteArray(), "QUOTED-PRINTABLE", 52, QByteArray(),
                            AbstractMessage::bodyFldDsp_t(), QList<QByteArray>(), QByteArray(), QVariant(), 2)));
    msgList.append(QSharedPointer<AbstractMessage>(
            new TextMessage("text", "html", bodyFldParam, QByteArray(), QByteArray(), "QUOTED-PRINTABLE", 1739, QByteArray(),
                            AbstractMessage::bodyFldDsp_t(), QList<QByteArray>(), QByteArray(), QVariant(), 66)));
    bodyFldParam.clear();
    bodyFldParam["BOUNDARY"] = "----=_NextPart_001_0078_01CBB179.57530990";
    msgList = QList<QSharedPointer<AbstractMessage> >() << QSharedPointer<AbstractMessage>(
            new MultiMessage( msgList, "alternative", bodyFldParam, AbstractMessage::bodyFldDsp_t(), QList<QByteArray>(), QByteArray(), QVariant() ) );
    msgList << QSharedPointer<AbstractMessage>(
            new MsgMessage("message", "rfc822", AbstractMessage::bodyFldParam_t(), QByteArray(), QByteArray(), "7BIT", 836,
                           QByteArray(), AbstractMessage::bodyFldDsp_t(), QList<QByteArray>(), QByteArray(), QVariant(),
                           Envelope(), QSharedPointer<AbstractMessage>(
                                   new BasicMessage("attachment", QString(), AbstractMessage::bodyFldParam_t(), QByteArray(), QByteArray(),
                                                    QByteArray(), 0, QByteArray(), AbstractMessage::bodyFldDsp_t(), QList<QByteArray>(), QByteArray(), QVariant())
                                   ), 0 ));
    msgList << QSharedPointer<AbstractMessage>(
            new MsgMessage("message", "rfc822", AbstractMessage::bodyFldParam_t(), QByteArray(), QByteArray(), "7BIT", 50785,
                           QByteArray(), AbstractMessage::bodyFldDsp_t(), QList<QByteArray>(), QByteArray(), QVariant(),
                           Envelope(), QSharedPointer<AbstractMessage>(
                                   new BasicMessage("attachment", QString(), AbstractMessage::bodyFldParam_t(), QByteArray(), QByteArray(),
                                                    QByteArray(), 0, QByteArray(), AbstractMessage::bodyFldDsp_t(), QList<QByteArray>(), QByteArray(), QVariant())
                                   ), 0 ));
    bodyFldParam.clear();
    bodyFldParam.clear();
    bodyFldParam["BOUNDARY"] = "----=_NextPart_000_0077_01CBB179.57530990";
    bodyFldDsp = AbstractMessage::bodyFldDsp_t();
    fetchData["BODYSTRUCTURE"] = QSharedPointer<AbstractData>(
            new MultiMessage( msgList, QLatin1String("mixed"), bodyFldParam, bodyFldDsp, QList<QByteArray>(), QByteArray(), QVariant()));
    QTest::newRow("fetch-envelope-blupix-gmail")
            << QByteArray("* 6116 FETCH (UID 8803 RFC822.SIZE 56144 ENVELOPE (\"Tue, 11 Jan 2011 10:21:42 +0100\" "
                          "\"blablabla\" ((\"somebody\" NIL \"info\" \"example.com\")) "
                          "((\"somebody\" NIL \"info\" \"example.com\")) "
                          "((\"somebody\" NIL \"info\" \"example.com\")) "
                          "((\"destination\" NIL \"foobar\" \"gmail.com\")) "
                          "NIL NIL NIL \"\") "
                          "BODYSTRUCTURE (((\"TEXT\" \"PLAIN\" (\"CHARSET\" \"iso-8859-2\") NIL NIL "
                          "\"QUOTED-PRINTABLE\" 52 2 NIL NIL NIL)(\"TEXT\" \"HTML\" (\"CHARSET\" \"iso-8859-2\") "
                          "NIL NIL \"QUOTED-PRINTABLE\" 1739 66 NIL NIL NIL) \"ALTERNATIVE\" "
                          "(\"BOUNDARY\" \"----=_NextPart_001_0078_01CBB179.57530990\") NIL NIL)"
                          "(\"MESSAGE\" \"RFC822\" NIL NIL NIL \"7BIT\" 836 NIL (\"ATTACHMENT\" NIL) NIL)"
                          "(\"MESSAGE\" \"RFC822\" NIL NIL NIL \"7BIT\" 50785 NIL (\"ATTACHMENT\" NIL) NIL) "
                          "\"MIXED\" (\"BOUNDARY\" \"----=_NextPart_000_0077_01CBB179.57530990\") NIL NIL))\r\n")
            << QSharedPointer<AbstractResponse>( new Fetch( 6116, fetchData ) );
#if 0

#endif

    fetchData.clear();
    fetchData["UID"] = QSharedPointer<AbstractData>(new RespData<uint>(42463));
    fetchData["FLAGS"] = QSharedPointer<AbstractData>(new RespData<QStringList>(QStringList() << QLatin1String("\\Seen")));
    fetchData["MODSEQ"] = QSharedPointer<AbstractData>(new RespData<quint64>(quint64(45278)));
    QTest::newRow("fetch-flags-modseq")
            << QByteArray("* 11235 FETCH (UID 42463 MODSEQ (45278) FLAGS (\\Seen))\r\n")
            << QSharedPointer<AbstractResponse>(new Fetch(11235, fetchData));

    fetchData.clear();
    fetchData["FLAGS"] = QSharedPointer<AbstractData>(
                new RespData<QStringList>(QStringList() << QLatin1String("\\Seen") << QLatin1String("\\Answered")));
    QTest::newRow("fetch-two-flags")
            << QByteArray("* 2 FETCH (FLAGS (\\Seen \\Answered))\r\n")
            << QSharedPointer<AbstractResponse>(new Fetch(2, fetchData));

    fetchData.clear();
    fetchData["UID"] = QSharedPointer<AbstractData>(new RespData<uint>(81));
    fetchData["BODY[HEADER.FIELDS (MESSAGE-ID IN-REPLY-TO REFERENCES DATE)]"] = QSharedPointer<AbstractData>(
                new RespData<QByteArray>("01234567\r\n"));
    QTest::newRow("fetch-header-fields-1")
            << QByteArray("* 81 FETCH (UID 81 BODY[HEADER.FIELDS (MESSAGE-ID In-REPLY-TO REFERENCES DATE)] {10}\r\n01234567\r\n)\r\n")
            << QSharedPointer<AbstractResponse>(new Fetch(81, fetchData));

    fetchData.clear();
    fetchData["UID"] = QSharedPointer<AbstractData>(new RespData<uint>(81));
    fetchData["BODY[HEADER.FIELDS (MESSAGE-ID)]"] = QSharedPointer<AbstractData>(
                new RespData<QByteArray>("01234567\r\n"));
    QTest::newRow("fetch-header-fields-2")
            << QByteArray("* 81 FETCH (UID 81 BODY[HEADER.FIELDS (MESSAgE-Id)]{10}\r\n01234567\r\n)\r\n")
            << QSharedPointer<AbstractResponse>(new Fetch(81, fetchData));

    QTest::newRow("id-nil")
            << QByteArray("* ID nIl\r\n")
            << QSharedPointer<AbstractResponse>(new Id(QMap<QByteArray,QByteArray>()));

    QTest::newRow("id-empty")
            << QByteArray("* ID ()\r\n")
            << QSharedPointer<AbstractResponse>(new Id(QMap<QByteArray,QByteArray>()));

    QMap<QByteArray,QByteArray> sampleId;
    sampleId["foo "] = "bar";
    QTest::newRow("id-something")
            << QByteArray("* ID (\"foo \" \"bar\")\r\n")
            << QSharedPointer<AbstractResponse>(new Id(sampleId));

    QTest::newRow("enabled-empty")
            << QByteArray("* ENABLED\r\n")
            << QSharedPointer<AbstractResponse>(new Enabled(QList<QByteArray>()));

    QTest::newRow("enabled-empty-space")
            << QByteArray("* ENABLED \r\n")
            << QSharedPointer<AbstractResponse>(new Enabled(QList<QByteArray>()));

    QTest::newRow("enabled-condstore")
            << QByteArray("* ENABLED CONDSToRE\r\n")
            << QSharedPointer<AbstractResponse>(new Enabled(QList<QByteArray>() << "CONDSToRE"));

    QTest::newRow("enabled-condstore-qresync")
            << QByteArray("* ENABLED CONDSToRE qresync\r\n")
            << QSharedPointer<AbstractResponse>(new Enabled(QList<QByteArray>() << "CONDSToRE" << "qresync"));

    QTest::newRow("vanished-one")
            << QByteArray("* VANIShED 1\r\n")
            << QSharedPointer<AbstractResponse>(new Vanished(Vanished::NOT_EARLIER, QList<uint>() << 1));

    QTest::newRow("vanished-earlier-one")
            << QByteArray("* VANIShED (EARlIER) 1\r\n")
            << QSharedPointer<AbstractResponse>(new Vanished(Vanished::EARLIER, QList<uint>() << 1));

    QTest::newRow("vanished-earlier-set")
            << QByteArray("* VANISHED (EARLIER) 300:303,405,411\r\n")
            << QSharedPointer<AbstractResponse>(new Vanished(Vanished::EARLIER, QList<uint>() << 300 << 301 << 302 << 303 << 405 << 411));

    QTest::newRow("genurlauth-1")
            << QByteArray("* GENURLAUTH \"imap://joe@example.com/INBOX/;uid=20/;section=1.2;urlauth=submit+fred:internal:91354a473744909de610943775f92038\"\r\n")
            << QSharedPointer<AbstractResponse>(new GenUrlAuth(QLatin1String("imap://joe@example.com/INBOX/;uid=20/;section=1.2;urlauth=submit+fred:internal:91354a473744909de610943775f92038")));

    QTest::newRow("genurlauth-2")
            << QByteArray("* GENURLAUTH meh\r\n")
            << QSharedPointer<AbstractResponse>(new GenUrlAuth(QLatin1String("meh")));

    QSharedPointer<AbstractData> modSeqData(new RespData<quint64>(5875136264581852368ULL));
    QTest::newRow("highestmodseq-64bit")
            << QByteArray("* OK [HIGHESTMODSEQ 5875136264581852368] x\r\n")
            << QSharedPointer<AbstractResponse>(new State("", OK, QLatin1String("x"), HIGHESTMODSEQ, modSeqData));

    fetchData.clear();
    fetchData["UID"] = QSharedPointer<AbstractData>(new RespData<uint>(123));
    fetchData["MODSEQ"] = QSharedPointer<AbstractData>(new RespData<quint64>(5875136264581852368ULL));
    QTest::newRow("fetch-highestmodseq-64bit")
            << QByteArray("* 33 FETCH (UID 123 MODSEQ (5875136264581852368))\r\n")
            << QSharedPointer<AbstractResponse>(new Fetch(33, fetchData));

}

/** @short Test that parsing this garbage doesn't result in an expceiton

In a erfect world with unlimited resources, it would be nice to verify the result of the parsing. However, I'm trying to feed
real-world data to the test cases and this might mean having to construct tens of lines using code which is just tedious to create.
This is better than nothing.
*/
void ImapParserParseTest::testParseFetchGarbageWithoutExceptions()
{
    QFETCH(QByteArray, line);
    try {
        QSharedPointer<Imap::Responses::AbstractResponse> r = parser->parseUntagged(line);
        QVERIFY(r.dynamicCast<Imap::Responses::Fetch>());
    } catch (const Imap::ParseError &err) {
        qDebug() << err.what();
        QFAIL("Parsing resulted in an exception");
    }
}

void ImapParserParseTest::testParseFetchGarbageWithoutExceptions_data()
{
    QTest::addColumn<QByteArray>("line");

    QTest::newRow("fetch-seznam.cz-quirk-minus-two-as-uint")
        << QByteArray("* 997 FETCH (UID 8636 ENVELOPE (NIL \"Merkantilismus\" ((\"\" NIL \"tony.szturc\" \"seznam.cz\")) "
                      "((\"\" NIL \"tony.szturc\" \"seznam.cz\")) ((\"\" NIL \"tony.szturc\" \"seznam.cz\")) "
                      "((\"\" NIL \"xxx\" \"seznam.cz\")) NIL NIL NIL \"\") INTERNALDATE \"15-Jan-2013 12:17:06 +0000\" "
                      "BODYSTRUCTURE ((\"text\" \"plain\" (\"charset\" \"us- ascii\") NIL NIL \"7bit\" -2 1 NIL NIL NIL)"
                      "(\"application\" \"msword\" (\"name\" "
                      "\"=?utf-8?B?NC10cmFkaWNuaV9ob3Nwb2RhcnNrZV9teXNsZW5pLG5vdm92ZWtlX2Vrb25vbWlja2VfdGVvcmllLmRvY2==?=\") "
                      "NIL NIL \"base64\" 51146 NIL NIL NIL)"
                      "(\"application\" \"msword\" (\"name\" \"=?utf-8?B?NS1ob3Nwb2RhcnNreXZ5dm9qLmRvY2==?=\") NIL NIL "
                      "\"base64\" 205984 NIL NIL NIL)"
                      "(\"application\" \"vnd.ms-powerpoint\" (\"name\" \"=?utf-8?B?Mi5faG9zcG9kYXJza3lfdnl2b2pfY2Vza3ljaF96ZW1pLnBwdH==?=\") "
                      "NIL NIL \"base64\" 3986596 NIL NIL NIL) \"mixed\" NIL NIL NIL) RFC822.SIZE 4245505)\r\n");

    // Test data from http://thread.gmane.org/gmane.mail.squirrelmail.user/26433
    QTest::newRow("random-squirrelmail-report")
            << QByteArray("* 1 FETCH (BODYSTRUCTURE "
                          "(((\"text\" \"plain\" (\"charset\" \"US-ASCII\") NIL NIL \"7bit\" 2 1 NIL NIL "
                          "NIL)(\"text\" \"html\" (\"charset\" \"US-ASCII\") NIL NIL \"quoted-printable\" 486 "
                          "10 NIL NIL NIL) \"alternative\" (\"boundary\" "
                          "\"-----------------------------1131387468\") NIL NIL)(\"message\" \"rfc822\" NIL "
                          "NIL NIL NIL 191761 (\"Thu, 3 Nov 2005 14:19:49 EST\" \"Fwd: Fw: Fw:\" ((NIL "
                          "NIL \"Ernestgerp12\" \"aol.com\")) NIL NIL ((NIL NIL \"BagLady1933\" "
                          "\"aol.com\")(NIL NIL \"BOBPEGRUKS\" \"cs.com\")(NIL NIL \"bren804\" "
                          "\"webtv.net\")(NIL NIL \"khgsk\" \"comcast.net\")(NIL NIL \"KOKOGOSIK\" "
                          "\"COMCAST.NET\")(NIL NIL \"MAG2215\" \"aol.com\")(NIL NIL \"Mamabenc\" "
                          "\"aol.com\")(NIL NIL \"Ifix2th\" \"aol.com\")(NIL NIL \"Ooosal\" \"aol.com\")(NIL "
                          "NIL \"rtalalaj\" \"comcast.net\")) NIL NIL NIL \"e3.1fdeccbc.309bbcd5 <at> aol.com\") "
                          "(((\"text\" \"plain\" (\"charset\" \"US-ASCII\") NIL NIL \"7bit\" 2 1 NIL NIL "
                          "NIL)(\"text\" \"html\" (\"charset\" \"US-ASCII\") NIL NIL \"quoted-printable\" 486 "
                          "10 NIL NIL NIL) \"alternative\" (\"boundary\" "
                          "\"-----------------------------1131045588\") NIL NIL)(\"message\" \"rfc822\" NIL "
                          "NIL NIL NIL 190153 NIL (((\"text\" \"plain\" (\"charset\" \"iso-8859-1\") NIL NIL "
                          "\"quoted-printable\" 6584 293 NIL NIL NIL)(\"text\" \"html\" (\"charset\" "
                          "\"iso-8859-1\") NIL NIL \"quoted-printable\" 61190 948 NIL NIL NIL) "
                          "\"alternative\" (\"boundary\" \"----=_NextPart_001_004C_01C5E044.F95592A0\") NIL "
                          "NIL)(\"image\" \"gif\" (\"name\" \"ATT0000712221112121.gif\") NIL NIL \"base64\" "
                          "68496 NIL NIL NIL)(\"image\" \"gif\" (\"name\" \"ATT0001023332223232.gif\") NIL "
                          "NIL \"base64\" 20038 NIL NIL NIL)(\"image\" \"gif\" (\"name\" "
                          "\"image001334343.gif\") NIL NIL \"base64\" 10834 NIL NIL NIL)(\"image\" \"gif\" "
                          "(\"name\" \"ATT0001646665545455.gif\") NIL NIL \"base64\" 7146 NIL NIL "
                          "NIL)(\"audio\" \"mid\" (\"name\" \"ATT0001951116651516.mid\") NIL NIL \"base64\" "
                          "12938 NIL NIL NIL) \"related\" (\"boundary\" "
                          "\"----=_NextPart_000_004B_01C5E044.F95592A0\" \"type\" "
                          "\"multipart/alternative\") NIL NIL) NIL NIL (\"INLINE\" NIL) NIL) \"mixed\" "
                          "(\"boundary\" \"part2_d7.319e0427.309bbcd5_boundary\") NIL NIL) NIL NIL "
                          "(\"INLINE\" NIL) NIL) \"mixed\" (\"boundary\" "
                          "\"part1_d7.319e0427.30a0f44c_boundary\") NIL NIL)"
                          " )\r\n");

    QTest::newRow("davmail-by-ashp")
            << QByteArray("* 1125 FETCH (BODYSTRUCTURE ((\"TEXT\" \"HTML\" (\"CHARSET\" \"ISO-8859-1\") NIL NIL \"QUOTED-PRINTABLE\" 562 7)"
                          "(\"APPLICATION\" \"OCTET-STREAM\" (\"NAME\" \"zzz.xml\") NIL "
                          "\"ZZZ.XML\" \"BASE64\" NIL NIL) \"MIXED\"))\r\n");
}

void ImapParserParseTest::benchmark()
{
    QByteArray line1 = "* 1 FETCH (BODYSTRUCTURE ((\"text\" \"plain\" "
        "(\"charset\" \"US-ASCII\" \"delsp\" \"yes\" \"format\" \"flowed\") "
        "NIL NIL \"7bit\" 990 27 NIL NIL NIL)"
        "(\"application\" \"pgp-signature\" (\"x-mac-type\" \"70674453\" \"name\" \"PGP.sig\") NIL "
        "\"This is a digitally signed message part\" \"7bit\" 193 NIL (\"inline\" "
        "(\"filename\" \"PGP.sig\")) NIL) \"signed\" (\"protocol\" "
        "\"application/pgp-signature\" \"micalg\" \"pgp-sha1\" \"boundary\" "
        "\"Apple-Mail-10--856231115\") NIL NIL))\r\n";
    QByteArray line2 = "* 13 FETCH (ENVELOPE (NIL "
            "\"IMAP4rev1 WG mtg summary and minutes\" "
            "((\"Terry Gray\" NIL \"gray\" \"cac.washington.edu\")) "
            "((\"Terry Gray\" NIL \"gray\" \"cac.washington.edu\")) "
            "((\"Terry Gray\" NIL \"gray\" \"cac.washington.edu\")) "
            "((NIL NIL \"imap\" \"cac.washington.edu\")) "
            "((NIL NIL \"minutes\" \"CNRI.Reston.VA.US\") "
            "(\"John Klensin\" NIL \"KLENSIN\" \"MIT.EDU\")) NIL NIL "
            "\"<B27397-0100000@cac.washington.edu>\"))\r\n";
    QByteArray line3 = "* 123 FETCH (InternalDate \"6-Apr-1981 12:03:32 -0630\")\r\n";

    QBENCHMARK {
        parser->processLine( line1 );
        parser->processLine( line2 );
        parser->processLine( line3 );

        while (parser->hasResponse())
            parser->getResponse();
    }
}

void ImapParserParseTest::benchmarkInitialChat()
{
    QByteArray line4 = "* OK [CAPABILITY IMAP4rev1 LITERAL+ SASL-IR LOGIN-REFERRALS ID ENABLE IDLE STARTTLS AUTH=PLAIN] Dovecot ready.\r\n";
    QByteArray line5 = "* CAPABILITY IMAP4rev1 LITERAL+ SASL-IR LOGIN-REFERRALS ID ENABLE IDLE STARTTLS AUTH=PLAIN\r\n";
    QByteArray line6 = "1 OK Pre-login capabilities listed, post-login capabilities have more.\r\n";
    QByteArray line7 = "* CAPABILITY IMAP4rev1 LITERAL+ SASL-IR LOGIN-REFERRALS ID ENABLE IDLE SORT SORT=DISPLAY "
        "THREAD=REFERENCES THREAD=REFS MULTIAPPEND UNSELECT CHILDREN NAMESPACE UIDPLUS LIST-EXTENDED I18NLEVEL=1 "
        "CONDSTORE QRESYNC ESEARCH ESORT SEARCHRES WITHIN CONTEXT=SEARCH LIST-STATUS SPECIAL-USE\r\n";
    QByteArray line8 = "2 OK Logged in\r\n";
    QByteArray line9 = "3 OK Capability completed.\r\n";
    QByteArray line10 = "* ENABLED QRESYNC\r\n";
    QByteArray line11 = "4 OK Enabled.\r\n";
    QByteArray line12 = "* NAMESPACE ((\"\" \".\")) NIL NIL\r\n";
    QByteArray line13 = "5 OK Namespace completed.\r\n";

    QBENCHMARK {
        parser->processLine(line4);
        parser->processLine(line5);
        parser->processLine(line6);
        parser->processLine(line7);
        parser->processLine(line8);
        parser->processLine(line9);
        parser->processLine(line10);
        parser->processLine(line11);
        parser->processLine(line12);
        parser->processLine(line13);
        while (parser->hasResponse())
            parser->getResponse();
    }
}

void ImapParserParseTest::testSequences()
{
    QFETCH( Imap::Sequence, sequence );
    QFETCH( QByteArray, muster );
    QCOMPARE( sequence.toByteArray(), muster );
}

void ImapParserParseTest::testSequences_data()
{
    QTest::addColumn<Imap::Sequence>("sequence");
    QTest::addColumn<QByteArray>("muster");

    QTest::newRow("sequence-one") <<
            Imap::Sequence( 33 ) << QByteArray("33");

    QTest::newRow("sequence-unlimited") <<
            Imap::Sequence::startingAt(666) << QByteArray("666:*");

    QTest::newRow("sequence-range") <<
            Imap::Sequence( 333, 666 ) << QByteArray("333:666");

    QTest::newRow("sequence-distinct") <<
            Imap::Sequence( 20 ).add( 10 ).add( 30 ) << QByteArray("10,20,30");

    QTest::newRow("sequence-collapsed-2") <<
            Imap::Sequence( 10 ).add( 11 ) << QByteArray("10:11");

    QTest::newRow("sequence-collapsed-3") <<
            Imap::Sequence( 10 ).add( 11 ).add( 12 ) << QByteArray("10:12");

    QTest::newRow("sequence-head-and-collapsed") <<
            Imap::Sequence( 3 ).add( 31 ).add( 32 ).add( 33 ) << QByteArray("3,31:33");

    QTest::newRow("sequence-collapsed-and-tail") <<
            Imap::Sequence( 666 ).add( 31 ).add( 32 ).add( 33 ) << QByteArray("31:33,666");

    QTest::newRow("sequence-head-collapsed-tail") <<
            Imap::Sequence( 666 ).add( 31 ).add( 32 ).add( 1 ).add( 33 ) << QByteArray("1,31:33,666");

    QTest::newRow("sequence-same") <<
            Imap::Sequence( 2 ).add( 2 ) << QByteArray("2");

    QTest::newRow("sequence-multiple-consequent") <<
            Imap::Sequence( 2 ).add( 3 ).add( 4 ).add( 6 ).add( 7 ) << QByteArray("2:4,6:7");

    QTest::newRow("sequence-complex") <<
            Imap::Sequence( 2 ).add( 3 ).add( 4 ).add( 6 ).add( 7 ).add( 1 )
            .add( 100 ).add( 101 ).add( 102 ).add( 99 ).add( 666 ).add( 333 ).add( 666 ) <<
            QByteArray("1:4,6:7,99:102,333,666");

    QTest::newRow("sequence-from-list-1") <<
            Imap::Sequence::fromList( QList<uint>() << 2 << 3 << 4 << 6 << 7 << 1 << 100 << 101 << 102 << 99 << 666 << 333 << 666) <<
            QByteArray("1:4,6:7,99:102,333,666");
}

/** @short Test responses which fail to parse */
void ImapParserParseTest::testThrow()
{
    QFETCH(QByteArray, line);
    QFETCH(QString, exceptionClass);
    QFETCH(QString, error);

    try {
        QSharedPointer<Imap::Responses::AbstractResponse> ptr = parser->parseUntagged(line);
        QVERIFY2(!ptr, "should have thrown");
    } catch (Imap::ImapException &e) {
        QCOMPARE(QString::fromUtf8(e.exceptionClass().c_str()), exceptionClass);
        QCOMPARE(QString::fromUtf8(e.msg().c_str()), error);
    }
}

/** @short Test data for testThrow() */
void ImapParserParseTest::testThrow_data()
{
    QTest::addColumn<QByteArray>("line");
    QTest::addColumn<QString>("exceptionClass");
    QTest::addColumn<QString>("error");

    QTest::newRow("garbage")
            << QByteArray("blah")
            << QString("UnrecognizedResponseKind")
            << QString("AH"); // uppercase of "blah" after cutting the two leading characters

    QTest::newRow("garbage-well-formed")
            << QByteArray("* blah\r\n")
            << QString("UnrecognizedResponseKind")
            << QString("BLAH");

    QTest::newRow("expunge-number-at-the-end")
            << QByteArray("* expunge 666\r\n") << QString("UnexpectedHere") << QString("Malformed response: the number should go first");
}

TROJITA_HEADLESS_TEST( ImapParserParseTest )

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
