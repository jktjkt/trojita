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

#include <tr1/memory>
#include <qtest_kde.h>

#include "test_Imap_Responses.h"
#include "test_Imap_Responses.moc"

Q_DECLARE_METATYPE(Imap::Responses::Status)
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

    std::tr1::shared_ptr<AbstractRespCodeData> voidData( new RespCodeData<void>() );
    std::tr1::shared_ptr<AbstractRespCodeData> dumbList( new RespCodeData<QStringList>( QStringList() << "foo" << "bar" ) );

    QTest::newRow( "tagged-OK-nothing" ) <<
        respPtr( new Status( "123", OK, "foobar 333", NONE, voidData ) ) <<
        respPtr( new Status( "123", OK, "foobar 333", NONE, voidData ) );

    QTest::newRow( "tagged-OK-ALERT-void" ) <<
        respPtr( new Status( "123", OK, "foobar 666", ALERT, voidData ) ) <<
        respPtr( new Status( "123", OK, "foobar 666", ALERT, voidData ) );

    QTest::newRow( "untagged-NO-CAPABILITY-void" ) <<
        respPtr( new Status( QString::null, NO, "foobar 1337", CAPABILITIES, dumbList ) ) <<
        respPtr( new Status( QString::null, NO, "foobar 1337", CAPABILITIES, dumbList ) );

    QTest::newRow( "capability-caps" ) <<
        respPtr( new Capability( QStringList() << "1337" << "trojita" ) ) <<
        respPtr( new Capability( QStringList() << "1337" << "trojita" ) );

    QTest::newRow( "number" ) <<
        respPtr( new NumberResponse( EXISTS, 10 ) ) <<
        respPtr( new NumberResponse( EXISTS, 10 ) );

    QTest::newRow( "list" ) <<
        respPtr( new List( LIST, QStringList( "\\Noselect" ), ".", "foOBar" ) ) <<
        respPtr( new List( LIST, QStringList( "\\Noselect" ), ".", "foOBar" ) );

    QTest::newRow( "flags" ) <<
        respPtr( new Flags( QStringList( "\\Seen" ) ) ) <<
        respPtr( new Flags( QStringList( "\\Seen" ) ) );

}

void ImapResponsesTest::testCompareNe_data()
{
    using namespace Imap::Responses;

    QTest::addColumn<respPtr>("first");
    QTest::addColumn<respPtr>("second");

    std::tr1::shared_ptr<AbstractRespCodeData> voidData( new RespCodeData<void>() );
    std::tr1::shared_ptr<AbstractRespCodeData> dumbList( new RespCodeData<QStringList>( QStringList() << "foo" << "bar" ) );
    std::tr1::shared_ptr<AbstractRespCodeData> anotherList( new RespCodeData<QStringList>( QStringList() << "bar" << "baz" ) );

    QTest::newRow( "status-tag" ) <<
        respPtr( new Status( "123", OK, "foobar 333", NONE, voidData ) ) <<
        respPtr( new Status( "321", OK, "foobar 333", NONE, voidData ) );

    QTest::newRow( "status-tagged-untagged" ) <<
        respPtr( new Status( "123", OK, "foobar 333", NONE, voidData ) ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", NONE, voidData ) );

    QTest::newRow( "status-kind" ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", NONE, voidData ) ) <<
        respPtr( new Status( QString::null, NO, "foobar 333", NONE, voidData ) );

    QTest::newRow( "status-response-code" ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", NONE, voidData ) ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", ALERT, voidData ) );

    QTest::newRow( "status-response-data-type" ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", CAPABILITIES, dumbList ) ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", CAPABILITIES, voidData ) );

    QTest::newRow( "status-response-data-data" ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", CAPABILITIES, dumbList ) ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", CAPABILITIES, anotherList ) );

    QTest::newRow( "status-message" ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", NONE, voidData ) ) <<
        respPtr( new Status( QString::null, OK, "foobar 666", NONE, voidData ) );

    QTest::newRow( "kind-status-capability" ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", NONE, voidData ) ) <<
        respPtr( new Capability( QStringList() ) );

    QTest::newRow( "capability-caps" ) <<
        respPtr( new Capability( QStringList("333") ) ) <<
        respPtr( new Capability( QStringList("666") ) );

    QTest::newRow( "number-kind" ) <<
        respPtr( new NumberResponse( EXISTS, 10 ) ) <<
        respPtr( new NumberResponse( EXPUNGE, 10 ) );

    QTest::newRow( "number-number" ) <<
        respPtr( new NumberResponse( RECENT, 10 ) ) <<
        respPtr( new NumberResponse( RECENT, 11 ) );

    QTest::newRow( "list-kind" ) <<
        respPtr( new List( LIST, QStringList(), ".", "blesmrt" ) ) <<
        respPtr( new List( LSUB, QStringList(), ".", "blesmrt" ) );

    QTest::newRow( "list-flags" ) <<
        respPtr( new List( LIST, QStringList(), ".", "blesmrt" ) ) <<
        respPtr( new List( LIST, QStringList("333"), ".", "blesmrt" ) );

    QTest::newRow( "list-separator" ) <<
        respPtr( new List( LIST, QStringList(), ".", "blesmrt" ) ) <<
        respPtr( new List( LIST, QStringList(), "/", "blesmrt" ) );

    QTest::newRow( "list-mailbox" ) <<
        respPtr( new List( LIST, QStringList(), ".", "blesmrt" ) ) <<
        respPtr( new List( LIST, QStringList(), ".", "trojita" ) );

    QTest::newRow( "list-mailbox-case" ) <<
        respPtr( new List( LIST, QStringList(), ".", "blesmrt" ) ) <<
        respPtr( new List( LIST, QStringList(), ".", "blEsmrt" ) );

    QTest::newRow( "flags" ) <<
        respPtr( new Flags( QStringList("333") ) ) <<
        respPtr( new Flags( QStringList("666") ) );

}

QTEST_KDEMAIN_CORE( ImapResponsesTest )

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
