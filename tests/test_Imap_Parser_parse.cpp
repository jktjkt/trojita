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

Q_DECLARE_METATYPE(Imap::Responses::Response)

QTEST_KDEMAIN_CORE( ImapParserParseTest )

void ImapParserParseTest::initTestCase()
{
    array = new QByteArray();
    buf = new QBuffer( array );
    parser = new Imap::Parser( 0, std::auto_ptr<QIODevice>( buf ) );
}

void ImapParserParseTest::cleanupTestCase()
{
    delete parser;
    // buf is deleted by Imap::Parser's destructor
    delete array;
}

void ImapParserParseTest::testParseTagged()
{

}

void ImapParserParseTest::testParseTagged_data()
{
    using namespace Imap::Responses;

    QTest::addColumn<QByteArray>("line");
    QTest::addColumn<Response>("response");

    QTest::newRow("demo") << QByteArray("y01 OK Everything works, man!") << Response("y01", OK, NONE, QList<QByteArray>(), "OK Everything works, man!");
}

#include "Imap/Parser.h"


