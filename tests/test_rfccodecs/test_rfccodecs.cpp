/*
   This file is part of the kimap library.
   Copyright (C) 2007 Tom Albers <tomalbers@kde.nl>
   Copyright (c) 2007 Allen Winter <winter@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#include <qtest_kde.h>

#include "test_rfccodecs.h"

QTEST_KDEMAIN_CORE( RFCCodecsTest )

#include "Imap/Parser/3rdparty/rfccodecs.h"
#include "Imap/Encoders.h"
using namespace KIMAP;

void RFCCodecsTest::testIMAPEncoding()
{
  QString encoded, decoded;

  encoded = encodeImapFolderName( QString::fromUtf8("Test.Frode Rønning") );
  QVERIFY( encoded == "Test.Frode R&APg-nning" );
  decoded = decodeImapFolderName( "Test.Frode R&APg-nning" );
  QVERIFY( decoded == QString::fromUtf8("Test.Frode Rønning") );

  encoded = encodeImapFolderName( "Test.tom & jerry" );
  QVERIFY( encoded == "Test.tom &- jerry" );
  decoded = decodeImapFolderName( "Test.tom &- jerry" );
  QVERIFY( decoded == "Test.tom & jerry" );

  // Try to feed already encoded
  encoded = encodeImapFolderName( "Test.Cl&AOE-udio" );
  QVERIFY( encoded == "Test.Cl&-AOE-udio" );
  decoded = decodeImapFolderName( "Test.Cl&-AOE-udio" );
  QVERIFY( decoded == "Test.Cl&AOE-udio" );
}

void RFCCodecsTest::testQuotes()
{
  QVERIFY( quoteIMAP( "tom\"allen" ) == "tom\\\"allen" );
  QVERIFY( quoteIMAP( "tom\'allen" ) == "tom\'allen" );
  QVERIFY( quoteIMAP( "tom\\allen" ) == "tom\\\\allen" );
}

void RFCCodecsTest::testDecodeRFC2047String()
{
    QFETCH( QByteArray, raw );
    QFETCH( QString, decoded );

    QString res = Imap::decodeRFC2047String( raw );

    if ( res != decoded ) {
        if ( res.size() != decoded.size() ) {
            qDebug() << "Different size:" << res.size() << decoded.size();
        }
        int size = qMin( res.size(), decoded.size() );
        for ( int i = 0; i < size; ++i ) {
            QChar c1 = res.at(i);
            QChar c2 = decoded.at(i);
            if ( c1 == c2 ) {
                qDebug() << "OK" << i << QString::number( c1.unicode(), 16 ).prepend("0x") << c1;
            } else {
                qDebug() << "Offset" << i << QString::number( c1.unicode(), 16 ).prepend("0x")
                        << QString::number( c2.unicode(), 16 ).prepend("0x") << c1 << c2;
            }
        }
    }

    QCOMPARE( res, decoded );
}

void RFCCodecsTest::testDecodeRFC2047String_data()
{
    QTest::addColumn<QByteArray>("raw");
    QTest::addColumn<QString>("decoded");

    QTest::newRow("katuska-suject")
        << QByteArray("=?UTF-8?Q?moc=20pros=C3=ADm,=20mohl=20by=20ses=20na=20to=20kouk?= =?UTF-8?Q?nout=3F=20cht=C4=9Bla=20bych=20to=20m=C3=ADt=20spr=C3=A1vn?= =?UTF-8?Q?=C4=9B:,)?=")
        << QString::fromUtf8("moc prosím, mohl by ses na to kouknout? chtěla bych to mít správně:,)");

    QTest::newRow("jirka-prives")
        << QByteArray("=?UTF-8?Q?P=C5=AFj=C4=8Den=C3=AD=20p=C5=99=C3=ADv=C4=9Bsu=20na=20lod?=\r\n"
                      "=?UTF-8?Q?=C4=9B?=")
        << QString::fromUtf8("Půjčení přívěsu na lodě");

    QTest::newRow("second-word-encoded")
        << QByteArray("Domen =?UTF-8?Q?Ko=C5=BEar?=")
        << QString::fromUtf8("Domen Kožar");

    QTest::newRow("csa")
        << QByteArray("=?utf-8?Q?HOLIDAYS Czech Airlines?=")
        << QString::fromUtf8("HOLIDAYS Czech Airlines");
}
