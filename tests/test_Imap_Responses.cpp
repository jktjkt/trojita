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

    QEXPECT_FAIL( "", "testing non-equality", Continue );
    QCOMPARE( *first, *second );
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

}

void ImapResponsesTest::testCompareNe_data()
{
    using namespace Imap::Responses;

    QTest::addColumn<respPtr>("first");
    QTest::addColumn<respPtr>("second");

    std::tr1::shared_ptr<AbstractRespCodeData> voidData( new RespCodeData<void>() );
    std::tr1::shared_ptr<AbstractRespCodeData> dumbList( new RespCodeData<QStringList>( QStringList() << "foo" << "bar" ) );
    std::tr1::shared_ptr<AbstractRespCodeData> anotherList( new RespCodeData<QStringList>( QStringList() << "bar" << "baz" ) );

    QTest::newRow( "different-tag" ) <<
        respPtr( new Status( "123", OK, "foobar 333", NONE, voidData ) ) <<
        respPtr( new Status( "321", OK, "foobar 333", NONE, voidData ) );

    QTest::newRow( "tagged-untagged" ) <<
        respPtr( new Status( "123", OK, "foobar 333", NONE, voidData ) ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", NONE, voidData ) );

    QTest::newRow( "different-kind" ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", NONE, voidData ) ) <<
        respPtr( new Status( QString::null, NO, "foobar 333", NONE, voidData ) );

    QTest::newRow( "different-response-code" ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", NONE, voidData ) ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", ALERT, voidData ) );

    QTest::newRow( "different-response-data-type" ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", CAPABILITIES, dumbList ) ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", CAPABILITIES, voidData ) );

    QTest::newRow( "different-response-data-data" ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", CAPABILITIES, dumbList ) ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", CAPABILITIES, anotherList ) );

    QTest::newRow( "different-message" ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", NONE, voidData ) ) <<
        respPtr( new Status( QString::null, OK, "foobar 666", NONE, voidData ) );

    QTest::newRow( "different-response" ) <<
        respPtr( new Status( QString::null, OK, "foobar 333", NONE, voidData ) ) <<
        respPtr( new Capability( QStringList() ) );

}

}


