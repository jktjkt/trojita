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

#include <QTest>

#include "test_Imap_Responses.h"

Q_DECLARE_METATYPE(Imap::Responses::State)
Q_DECLARE_METATYPE(respPtr)

/** @short Make sure equal Responses are recognized as equal */
void ImapResponsesTest::testCompareEq()
{
    QFETCH( respPtr, first );
    QFETCH( respPtr, second );

    QCOMPARE( *first, *second );
}

/** @short Make sure different kind of Responses are recognized as different */
void ImapResponsesTest::testCompareNe()
{
    QFETCH( respPtr, first );
    QFETCH( respPtr, second );

    QVERIFY2( *first != *second, "responses equal" );
}

void ImapResponsesTest::testCompareEq_data()
{
    using namespace Imap::Responses;

    QTest::addColumn<respPtr>("first");
    QTest::addColumn<respPtr>("second");

    QSharedPointer<AbstractData> voidData( new RespData<void>() );
    QSharedPointer<AbstractData> dumbList( new RespData<QStringList>( QStringList() << QStringLiteral("foo") << QStringLiteral("bar") ) );

    QTest::newRow( "tagged-OK-nothing" ) <<
        respPtr( new State( "123", OK, QStringLiteral("foobar 333"), NONE, voidData ) ) <<
        respPtr( new State( "123", OK, QStringLiteral("foobar 333"), NONE, voidData ) );

    QTest::newRow( "tagged-OK-ALERT-void" ) <<
        respPtr( new State( "123", OK, QStringLiteral("foobar 666"), ALERT, voidData ) ) <<
        respPtr( new State( "123", OK, QStringLiteral("foobar 666"), ALERT, voidData ) );

    QTest::newRow( "untagged-NO-CAPABILITY-void" ) <<
        respPtr( new State( QByteArray(), NO, QStringLiteral("foobar 1337"), CAPABILITIES, dumbList ) ) <<
        respPtr( new State( QByteArray(), NO, QStringLiteral("foobar 1337"), CAPABILITIES, dumbList ) );

    QTest::newRow( "capability-caps" ) <<
        respPtr( new Capability( QStringList() << QStringLiteral("1337") << QStringLiteral("trojita") ) ) <<
        respPtr( new Capability( QStringList() << QStringLiteral("1337") << QStringLiteral("trojita") ) );

    QTest::newRow( "number" ) <<
        respPtr( new NumberResponse( EXISTS, 10 ) ) <<
        respPtr( new NumberResponse( EXISTS, 10 ) );

    QTest::newRow( "list" ) <<
        respPtr( new List( LIST, QStringList( QStringLiteral("\\Noselect") ), QStringLiteral("."), QStringLiteral("foOBar"), QMap<QByteArray,QVariant>() ) ) <<
        respPtr( new List( LIST, QStringList( QStringLiteral("\\Noselect") ), QStringLiteral("."), QStringLiteral("foOBar"), QMap<QByteArray,QVariant>() ) );

    QTest::newRow( "flags" ) <<
        respPtr( new Flags( QStringList( QStringLiteral("\\Seen") ) ) ) <<
        respPtr( new Flags( QStringList( QStringLiteral("\\Seen") ) ) );

    QTest::newRow( "search" ) <<
        respPtr( new Search(Imap::Uids() << 333)) <<
        respPtr( new Search(Imap::Uids() << 333));

    ESearch::ListData_t emptyEsearchList;
    QTest::newRow("esearch") <<
        respPtr(new ESearch("t1", ESearch::UIDS, emptyEsearchList)) <<
        respPtr(new ESearch("t1", ESearch::UIDS, emptyEsearchList));

    Status::stateDataType stateMap;
    QTest::newRow( "status-1" ) <<
        respPtr( new Status( QStringLiteral("ahoj"), stateMap ) ) <<
        respPtr( new Status( QStringLiteral("ahoj"), stateMap ) );

    stateMap[Status::MESSAGES] = 12;
    QTest::newRow( "status-2" ) <<
        respPtr( new Status( QStringLiteral("ahoj"), stateMap ) ) <<
        respPtr( new Status( QStringLiteral("ahoj"), stateMap ) );

    stateMap[Status::RECENT] = 0;
    QTest::newRow( "status-3" ) <<
        respPtr( new Status( QStringLiteral("ahoj"), stateMap ) ) <<
        respPtr( new Status( QStringLiteral("ahoj"), stateMap ) );

    QTest::newRow( "namespace-1" ) <<
        respPtr( new Namespace( QList<NamespaceData>() << NamespaceData( QStringLiteral("foo"), QStringLiteral("bar")), QList<NamespaceData>(), 
                    QList<NamespaceData>() ) ) <<
        respPtr( new Namespace( QList<NamespaceData>() << NamespaceData( QStringLiteral("foo"), QStringLiteral("bar")), QList<NamespaceData>(), 
                    QList<NamespaceData>() ) );

    QTest::newRow( "sort-empty" ) << respPtr(new Sort(Imap::Uids())) << respPtr(new Sort(Imap::Uids()));
    QTest::newRow( "sort-1" ) << respPtr(new Sort(Imap::Uids() << 3 << 6)) << respPtr(new Sort(Imap::Uids() << 3 << 6));

    ThreadingNode node;
    node.num = 666;
    QTest::newRow( "thread-1" ) <<
        respPtr( new Thread( QVector<ThreadingNode>() << node ) ) <<
        respPtr( new Thread( QVector<ThreadingNode>() << node ) );
    ThreadingNode node2;
    node.children += QVector<ThreadingNode>() << node2;
    QTest::newRow( "thread-2" ) <<
        respPtr( new Thread( QVector<ThreadingNode>() << node ) ) <<
        respPtr( new Thread( QVector<ThreadingNode>() << node ) );

    QTest::newRow("id-empty") <<
        respPtr(new Id(QMap<QByteArray,QByteArray>())) <<
        respPtr(new Id(QMap<QByteArray,QByteArray>()));

    QTest::newRow("enable") <<
        respPtr(new Enabled(QList<QByteArray>() << "foo")) <<
        respPtr(new Enabled(QList<QByteArray>() << "foo"));
}

void ImapResponsesTest::testCompareNe_data()
{
    using namespace Imap::Responses;

    QTest::addColumn<respPtr>("first");
    QTest::addColumn<respPtr>("second");

    QSharedPointer<AbstractData> voidData( new RespData<void>() );
    QSharedPointer<AbstractData> dumbList( new RespData<QStringList>( QStringList() << QStringLiteral("foo") << QStringLiteral("bar") ) );
    QSharedPointer<AbstractData> anotherList( new RespData<QStringList>( QStringList() << QStringLiteral("bar") << QStringLiteral("baz") ) );

    QTest::newRow( "status-tag" ) <<
        respPtr( new State( "123", OK, QStringLiteral("foobar 333"), NONE, voidData ) ) <<
        respPtr( new State( "321", OK, QStringLiteral("foobar 333"), NONE, voidData ) );

    QTest::newRow( "status-tagged-untagged" ) <<
        respPtr( new State( "123", OK, QStringLiteral("foobar 333"), NONE, voidData ) ) <<
        respPtr( new State( QByteArray(), OK, QStringLiteral("foobar 333"), NONE, voidData ) );

    QTest::newRow( "status-kind" ) <<
        respPtr( new State( QByteArray(), OK, QStringLiteral("foobar 333"), NONE, voidData ) ) <<
        respPtr( new State( QByteArray(), NO, QStringLiteral("foobar 333"), NONE, voidData ) );

    QTest::newRow( "status-response-code" ) <<
        respPtr( new State( QByteArray(), OK, QStringLiteral("foobar 333"), NONE, voidData ) ) <<
        respPtr( new State( QByteArray(), OK, QStringLiteral("foobar 333"), ALERT, voidData ) );

    QTest::newRow( "status-response-data-type" ) <<
        respPtr( new State( QByteArray(), OK, QStringLiteral("foobar 333"), CAPABILITIES, dumbList ) ) <<
        respPtr( new State( QByteArray(), OK, QStringLiteral("foobar 333"), CAPABILITIES, voidData ) );

    QTest::newRow( "status-response-data-data" ) <<
        respPtr( new State( QByteArray(), OK, QStringLiteral("foobar 333"), CAPABILITIES, dumbList ) ) <<
        respPtr( new State( QByteArray(), OK, QStringLiteral("foobar 333"), CAPABILITIES, anotherList ) );

    QTest::newRow( "status-message" ) <<
        respPtr( new State( QByteArray(), OK, QStringLiteral("foobar 333"), NONE, voidData ) ) <<
        respPtr( new State( QByteArray(), OK, QStringLiteral("foobar 666"), NONE, voidData ) );

    QTest::newRow( "kind-status-capability" ) <<
        respPtr( new State( QByteArray(), OK, QStringLiteral("foobar 333"), NONE, voidData ) ) <<
        respPtr( new Capability( QStringList() ) );

    QTest::newRow( "capability-caps" ) <<
        respPtr( new Capability( QStringList(QStringLiteral("333")) ) ) <<
        respPtr( new Capability( QStringList(QStringLiteral("666")) ) );

    QTest::newRow( "number-kind" ) <<
        respPtr( new NumberResponse( EXISTS, 10 ) ) <<
        respPtr( new NumberResponse( EXPUNGE, 10 ) );

    QTest::newRow( "number-number" ) <<
        respPtr( new NumberResponse( RECENT, 10 ) ) <<
        respPtr( new NumberResponse( RECENT, 11 ) );

    QTest::newRow( "list-kind" ) <<
        respPtr( new List( LIST, QStringList(), QStringLiteral("."), QStringLiteral("blesmrt"), QMap<QByteArray,QVariant>() ) ) <<
        respPtr( new List( LSUB, QStringList(), QStringLiteral("."), QStringLiteral("blesmrt"), QMap<QByteArray,QVariant>() ) );

    QTest::newRow( "list-flags" ) <<
        respPtr( new List( LIST, QStringList(), QStringLiteral("."), QStringLiteral("blesmrt"), QMap<QByteArray,QVariant>() ) ) <<
        respPtr( new List( LIST, QStringList(QStringLiteral("333")), QStringLiteral("."), QStringLiteral("blesmrt"), QMap<QByteArray,QVariant>() ) );

    QTest::newRow( "list-separator" ) <<
        respPtr( new List( LIST, QStringList(), QStringLiteral("."), QStringLiteral("blesmrt"), QMap<QByteArray,QVariant>() ) ) <<
        respPtr( new List( LIST, QStringList(), QStringLiteral("/"), QStringLiteral("blesmrt"), QMap<QByteArray,QVariant>() ) );

    QTest::newRow( "list-mailbox" ) <<
        respPtr( new List( LIST, QStringList(), QStringLiteral("."), QStringLiteral("blesmrt"), QMap<QByteArray,QVariant>() ) ) <<
        respPtr( new List( LIST, QStringList(), QStringLiteral("."), QStringLiteral("trojita"), QMap<QByteArray,QVariant>() ) );

    QTest::newRow( "list-mailbox-case" ) <<
        respPtr( new List( LIST, QStringList(), QStringLiteral("."), QStringLiteral("blesmrt"), QMap<QByteArray,QVariant>() ) ) <<
        respPtr( new List( LIST, QStringList(), QStringLiteral("."), QStringLiteral("blEsmrt"), QMap<QByteArray,QVariant>() ) );

    QMap<QByteArray,QVariant> listExtendedData;
    listExtendedData["blah"] = 10;
    QTest::newRow("list-extended-data") <<
        respPtr(new List(LIST, QStringList(), QStringLiteral("."), QStringLiteral("ble"), QMap<QByteArray,QVariant>())) <<
        respPtr(new List(LIST, QStringList(), QStringLiteral("."), QStringLiteral("ble"), listExtendedData));

    QTest::newRow( "flags" ) <<
        respPtr( new Flags( QStringList(QStringLiteral("333")) ) ) <<
        respPtr( new Flags( QStringList(QStringLiteral("666")) ) );

    QTest::newRow( "search" ) <<
        respPtr( new Search(Imap::Uids() << 333)) <<
        respPtr( new Search(Imap::Uids() << 666));

    ESearch::ListData_t emptyEsearchResp;
    QTest::newRow("esearch-tag") <<
        respPtr(new ESearch("t1", ESearch::UIDS, emptyEsearchResp)) <<
        respPtr(new ESearch(QByteArray(), ESearch::UIDS, emptyEsearchResp));

    QTest::newRow("esearch-uid-seq") <<
        respPtr(new ESearch("t1", ESearch::UIDS, emptyEsearchResp)) <<
        respPtr(new ESearch("t1", ESearch::SEQUENCE, emptyEsearchResp));

    ESearch::ListData_t dummyESearch1;
    dummyESearch1.push_back(qMakePair<>(QByteArray("foo"), Imap::Uids() << 666));

    QTest::newRow("esearch-listdata") <<
        respPtr(new ESearch("t1", ESearch::UIDS, dummyESearch1)) <<
        respPtr(new ESearch("t1", ESearch::UIDS, emptyEsearchResp));

    QTest::newRow( "status-mailbox" ) <<
        respPtr( new Status( QStringLiteral("ahoj"), Status::stateDataType() ) ) <<
        respPtr( new Status( QStringLiteral("ahOj"), Status::stateDataType() ) );

    Status::stateDataType stateMap1, stateMap2, stateMap3;
    stateMap1[Status::MESSAGES] = 12;
    stateMap1[Status::RECENT] = 0;

    stateMap2 = stateMap1;
    stateMap2[Status::UIDNEXT] = 1;

    stateMap3 = stateMap1;
    stateMap1[Status::RECENT] = 1;

    QTest::newRow( "status-mailbox-nonempty" ) <<
        respPtr( new Status( QStringLiteral("ahoj"), stateMap1 ) ) <<
        respPtr( new Status( QStringLiteral("ahOj"), stateMap1 ) );

    QTest::newRow( "status-1" ) <<
        respPtr( new Status( QStringLiteral("ahoj"), stateMap1 ) ) <<
        respPtr( new Status( QStringLiteral("ahoj"), stateMap2 ) );

    QTest::newRow( "status-2" ) <<
        respPtr( new Status( QStringLiteral("ahoj"), stateMap1 ) ) <<
        respPtr( new Status( QStringLiteral("ahoj"), stateMap3 ) );

    QTest::newRow( "namespace-1" ) <<
        respPtr( new Namespace( QList<NamespaceData>(), QList<NamespaceData>() << NamespaceData( QStringLiteral("foo"), QStringLiteral("bar") ), 
                    QList<NamespaceData>() ) ) <<
        respPtr( new Namespace( QList<NamespaceData>() << NamespaceData( QStringLiteral("foo"), QStringLiteral("bar")), QList<NamespaceData>(), 
                    QList<NamespaceData>() ) );

    QTest::newRow( "sort-empty-1" ) << respPtr(new Sort(Imap::Uids())) << respPtr(new Sort(Imap::Uids() << 3));
    QTest::newRow( "sort-empty-2" ) << respPtr(new Sort(Imap::Uids() << 6)) << respPtr(new Sort(Imap::Uids()));
    QTest::newRow( "sort-1" ) << respPtr(new Sort(Imap::Uids() << 33 << 6)) << respPtr(new Sort(Imap::Uids() << 3 << 6));

    ThreadingNode node( 666  );
    ThreadingNode node2( 333 );
    QTest::newRow( "thread-1" ) <<
        respPtr( new Thread( QVector<ThreadingNode>() ) ) <<
        respPtr( new Thread( QVector<ThreadingNode>() << node ) );
    QTest::newRow( "thread-2" ) <<
        respPtr( new Thread( QVector<ThreadingNode>() << node2 ) ) <<
        respPtr( new Thread( QVector<ThreadingNode>() << node ) );
    ThreadingNode node3;
    node.children += QVector<ThreadingNode>() << node3;
    QTest::newRow( "thread-3" ) <<
        respPtr( new Thread( QVector<ThreadingNode>() << node ) ) <<
        respPtr( new Thread( QVector<ThreadingNode>() << node << node3 ) );


    QMap<QByteArray,QByteArray> sampleId;
    sampleId["foo"] = "bar";

    QTest::newRow("id") <<
        respPtr(new Id(QMap<QByteArray,QByteArray>())) <<
        respPtr(new Id(sampleId));

    QTest::newRow("enable") <<
        respPtr(new Enabled(QList<QByteArray>())) <<
        respPtr(new Enabled(QList<QByteArray>() << "blah"));
}

QTEST_GUILESS_MAIN( ImapResponsesTest )

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
