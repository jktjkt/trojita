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

#include "test_Imap_LowLevelParser.h"
#include "test_Imap_LowLevelParser.moc"

#include "Imap/Exceptions.h"

typedef QPair<QByteArray,Imap::LowLevelParser::ParsedAs> StringWithKind;

void ImapLowLevelParserTest::testParseList()
{
    QByteArray line = "()";
    QList<QByteArray> splitted = line.split(' ');
    QList<QByteArray>::const_iterator begin = splitted.begin();
    QList<QByteArray>::const_iterator end = splitted.end();

    QCOMPARE( Imap::LowLevelParser::parseList( '(', ')', begin, end, "dummy", false, true),
            QStringList() );
    QCOMPARE( begin, end );
    begin = splitted.begin();

    try {
        Imap::LowLevelParser::parseList( '(', ')', begin, end, "dummy", false, false);
        QFAIL( "parseList() with empty list should have thrown an expception" );
    } catch ( Imap::NoData& ) {
        QVERIFY( true );
    }

    try {
        QByteArray line = "ahoj";
        QList<QByteArray> splitted = line.split(' ');
        QList<QByteArray>::const_iterator begin = splitted.begin();
        QList<QByteArray>::const_iterator end = splitted.end();

        Imap::LowLevelParser::parseList( '(', ')', begin, end, "dummy", false, false);
        QFAIL( "parseList() with no list should have thrown an expception" );
    } catch ( Imap::NoData& ) {
        QVERIFY( true );
    }

    line = "(ahOj)";
    splitted = line.split(' ');
    begin = splitted.begin();
    end = splitted.end();
    QCOMPARE( Imap::LowLevelParser::parseList( '(', ')', begin, end, "dummy", false, false),
            QStringList( "ahOj" ) );
    QCOMPARE( begin, end );

    line = "(ahOj)\r\n";
    splitted = line.split(' ');
    begin = splitted.begin();
    end = splitted.end();
    QCOMPARE( Imap::LowLevelParser::parseList( '(', ')', begin, end, "dummy", false, false),
            QStringList( "ahOj" ) );
    QCOMPARE( begin, end );

    line = "(ahoJ nAzdar)";
    splitted = line.split(' ');
    begin = splitted.begin();
    end = splitted.end();
    QCOMPARE( Imap::LowLevelParser::parseList( '(', ')', begin, end, "dummy", false, false),
            QStringList() << "ahoJ" << "nAzdar" );
    QCOMPARE( begin, end );

    line = "(ahoJ nAzdar)\r\n";
    splitted = line.split(' ');
    begin = splitted.begin();
    end = splitted.end();
    QCOMPARE( Imap::LowLevelParser::parseList( '(', ')', begin, end, "dummy", false, false),
            QStringList() << "ahoJ" << "nAzdar" );
    QCOMPARE( begin, end );

    line = "(ahoJ nAzdar) 333";
    splitted = line.split(' ');
    begin = splitted.begin();
    end = splitted.end();
    QCOMPARE( Imap::LowLevelParser::parseList( '(', ')', begin, end, "dummy", false, false),
            QStringList() << "ahoJ" << "nAzdar" );
    QCOMPARE( *begin, QByteArray("333") );

    line = "[ahoJ} Blesmrt trojita\r\n";
    splitted = line.split(' ');
    begin = splitted.begin();
    end = splitted.end();
    QCOMPARE( Imap::LowLevelParser::parseList( '[', '}', begin, end, "dummy", false, false),
            QStringList( "ahoJ" ) );
    QCOMPARE( *begin, QByteArray("Blesmrt") );

}

void ImapLowLevelParserTest::testGetString()
{
    using namespace Imap::LowLevelParser;

    QByteArray line = "ahOj";
    QList<QByteArray> splitted = line.split(' ');
    QList<QByteArray>::const_iterator begin = splitted.begin();
    QList<QByteArray>::const_iterator end = splitted.end();

    try {
        line = "ah0j 333"; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
        StringWithKind res = getString( begin, begin, line.constData());
        QFAIL( "getString() should scream on empty line" );
    } catch ( Imap::NoData& ) {
        QVERIFY( true );
    }

    try {
        line = ""; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
        StringWithKind res = getString( begin, end, line.constData());
        QFAIL( "getString() should scream on empty line" );
    } catch ( Imap::NoData& ) {
        QVERIFY( true );
    }

    try {
        line = "ah0j 333"; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
        StringWithKind res = getString( begin, end, line.constData());
        QFAIL( "getString() should ignore atoms" );
    } catch ( Imap::UnexpectedHere& ) {
        QVERIFY( true );
    }

    try {
        line = "ah0j"; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
        StringWithKind res = getString( begin, end, line.constData());
        QFAIL( "getString() should ignore atoms" );
    } catch ( Imap::UnexpectedHere& ) {
        QVERIFY( true );
    }

}

void ImapLowLevelParserTest::testGetAString()
{
    using namespace Imap::LowLevelParser;

    QByteArray line = "ahOj";
    QList<QByteArray> splitted = line.split(' ');
    QList<QByteArray>::const_iterator begin = splitted.begin();
    QList<QByteArray>::const_iterator end = splitted.end();

    StringWithKind res = getAString( begin, end, line.constData());
    QCOMPARE( res.first, QByteArray("ahOj") );
    QCOMPARE( res.second, ATOM );
    QCOMPARE( begin, end );

    line = "ah0j 333"; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("ah0j") );
    QCOMPARE( res.second, ATOM );
    QCOMPARE( *begin, QByteArray("333") );

    line = "ah0j\r\n"; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("ah0j") );
    QCOMPARE( res.second, ATOM );
    QCOMPARE( begin, end );

    line = "\"ah0j\""; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("ah0j") );
    QCOMPARE( res.second, QUOTED );
    QCOMPARE( begin, end );

    line = "\"ah0j\"\r\n"; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("ah0j") );
    QCOMPARE( res.second, QUOTED );
    QCOMPARE( begin, end );

    line = "\"ah0j\" blesmrt"; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("ah0j") );
    QCOMPARE( res.second, QUOTED );
    QCOMPARE( *begin, QByteArray("blesmrt") );

    line = "\"ah0\\\\j\" blesmrt"; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("ah0\\j") );
    QCOMPARE( res.second, QUOTED );
    QCOMPARE( *begin, QByteArray("blesmrt") );

    line = "\"ah0\\\"j\" blesmrt"; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("ah0\"j") );
    QCOMPARE( res.second, QUOTED );
    QCOMPARE( *begin, QByteArray("blesmrt") );

    line = "\"ahoj blesmrt\""; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("ahoj blesmrt") );
    QCOMPARE( res.second, QUOTED );
    QCOMPARE( begin, end );

    line = "\"ahoj blesmrt\" trojita"; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("ahoj blesmrt") );
    QCOMPARE( res.second, QUOTED );
    QCOMPARE( *begin, QByteArray("trojita") );

    line = "\"ahoj\\\" blesmrt\" trojita"; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("ahoj\" blesmrt") );
    QCOMPARE( res.second, QUOTED );
    QCOMPARE( *begin, QByteArray("trojita") );

    line = "\"\""; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("") );
    QCOMPARE( res.second, QUOTED );
    QCOMPARE( begin, end );

    line = "\"\" trojita"; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("") );
    QCOMPARE( res.second, QUOTED );
    QCOMPARE( *begin, QByteArray("trojita") );

    line = "\"\\\"\""; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("\"") );
    QCOMPARE( res.second, QUOTED );
    QCOMPARE( begin, end );

    line = "\"\\\\\""; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    res = getAString( begin, end, line.constData() );
    QCOMPARE( res.first, QByteArray("\\") );
    QCOMPARE( res.second, QUOTED );
    QCOMPARE( begin, end );

    line = ""; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    try {
        res = getAString( begin, begin, line.constData() );
        QFAIL( "empty string should have raised an exception" );
    } catch ( Imap::NoData& ) {
        QVERIFY( true );
    }

    line = ""; splitted = line.split(' '); begin = splitted.begin(); end = splitted.end();
    try {
        res = getAString( begin, end, line.constData() );
        QFAIL( "empty string should have raised an exception" );
    } catch ( Imap::NoData& ) {
        QVERIFY( true );
    }

    // FIXME: literals
}

QTEST_KDEMAIN_CORE( ImapLowLevelParserTest )

