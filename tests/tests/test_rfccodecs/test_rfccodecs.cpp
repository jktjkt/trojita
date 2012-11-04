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

#include <QDebug>
#include <QTest>
#include "test_rfccodecs.h"
#include "../headless_test.h"
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

    QTest::newRow("B-iso-1-jkt")
        << QByteArray("=?ISO-8859-1?B?SmFuIEt1bmRy4XQ=?=")
        << QString::fromUtf8("Jan Kundrát");

    QTest::newRow("Q-iso-2-jkt")
        << QByteArray("=?ISO-8859-2?Q?Jan_Kundr=E1t?=")
        << QString::fromUtf8("Jan Kundrát");

    QTest::newRow("buggy-no-space-between-encoded-words")
        << QByteArray("=?ISO-8859-2?Q?Jan_Kundr=E1t?=XX=?ISO-8859-2?Q?Jan_Kundr=E1t?=")
        << QString::fromUtf8("Jan KundrátXXJan Kundrát");

    QTest::newRow("B-utf8-vodakove")
        << QByteArray("=?UTF-8?B?W3ZvZF0gUmU6IGthemltaXIgdnlyYXplbiB6ZSB6YXNpbGFuaSBza3VwaW55?= "
                      "=?UTF-8?B?IChqZXN0bGkgbmUsIHRhayB0byBuZWN0aSBrYXppbWlyZSBhIHByaXpuZWog?= "
                      "=?UTF-8?B?c2UgOm8p?=")
        << QString::fromUtf8("[vod] Re: kazimir vyrazen ze zasilani skupiny (jestli ne, tak to necti kazimire a priznej se :o)");

    QTest::newRow("Q-iso-2-ceskosaske")
        << QByteArray("=?ISO-8859-2?Q?=C8eskosask=E9_=A9v=FDcarsko=3A_podzimn=ED_?= "
                      "=?ISO-8859-2?Q?nostalgie?=")
        << QString::fromUtf8("Českosaské Švýcarsko: podzimní nostalgie");

    QTest::newRow("B-utf8-empty")
        // careful to prevent the compiler from interpreting this is a trigraph/
        << QByteArray("=?UTF-8?B?" "?=")
        << QString::fromUtf8("");

    // This is in violation from RFC2047, but some mailers do produce this
    QTest::newRow("Q-utf8-multiword-upc")
        << QByteArray("=?utf-8?q?Studie pro podnikov=C3=A9 z=C3=A1kazn=C3=ADky spole=C4=8Dnosti UPC Business?=")
        << QString::fromUtf8("Studie pro podnikové zákazníky společnosti UPC Business");

    // Again, this violates RFC2047
    QTest::newRow("Q-utf8-multiword-csa")
        << QByteArray("=?utf-8?Q?HOLIDAYS Czech Airlines?=")
        << QString::fromUtf8("HOLIDAYS Czech Airlines");

    QTest::newRow("unescaped")
        << QByteArray("blesmrt")
        << QString::fromUtf8("blesmrt");

    QTest::newRow("rfc2047-ex-1")
        << QByteArray("(=?ISO-8859-1?Q?a?=)")
        << QString::fromUtf8("(a)");

    QTest::newRow("rfc2047-ex-2")
        << QByteArray("(=?ISO-8859-1?Q?a?= b)")
        << QString::fromUtf8("(a b)");

    QTest::newRow("rfc2047-ex-3")
        << QByteArray("(=?ISO-8859-1?Q?a?=  =?ISO-8859-1?Q?b?=)")
        << QString::fromUtf8("(ab)");

    QTest::newRow("rfc2047-ex-4")
        << QByteArray("(=?ISO-8859-1?Q?a?= \n \t =?ISO-8859-1?Q?b?=)")
        << QString::fromUtf8("(ab)");

    QTest::newRow("rfc2047-ex-5")
        << QByteArray("(=?ISO-8859-1?Q?a_b?=)")
        << QString::fromUtf8("(a b)");

    QTest::newRow("rfc2047-ex-6")
        << QByteArray("(=?ISO-8859-1?Q?a?= =?ISO-8859-2?Q?_b?=)")
        << QString::fromUtf8("(a b)");

    QTest::newRow("ascii")
        << QByteArray("foo bar baz  blah ble")
        << QString::fromUtf8("foo bar baz  blah ble");

    QTest::newRow("tb-ascii-then-unicode")
        << QByteArray("[foo] johoho tohlencto je ale pekne =?UTF-8?B?YmzEmyBzbXJ0IHRyb2o=?=\n"
                      " =?UTF-8?B?aXRhIHMgbWF0b3ZvdSBvbWFja291?=")
        << QString::fromUtf8("[foo] johoho tohlencto je ale pekne blě smrt trojita s matovou omackou");

    QTest::newRow("ascii-then-unicode-then-ascii")
        << QByteArray("[foo] johoho tohlencto je ale pekne =?UTF-8?B?YmzEmyBzbXJ0IHRyb2o=?=\n"
                      " =?UTF-8?B?aXRhIHMgbWF0b3ZvdSBvbWFja291?= blabla")
        << QString::fromUtf8("[foo] johoho tohlencto je ale pekne blě smrt trojita s matovou omackou blabla");

}

void RFCCodecsTest::testEncodeRFC2047String()
{
    QFETCH(QString, input);
    QFETCH(QByteArray, encoded);

    QCOMPARE(Imap::encodeRFC2047String(input), encoded);
    QCOMPARE(Imap::decodeRFC2047String(Imap::encodeRFC2047String(input)), input);
}

void RFCCodecsTest::testEncodeRFC2047String_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QByteArray>("encoded");

    QTest::newRow("empty") << QString() << QByteArray();
    QTest::newRow("simple-ascii") << QString::fromUtf8("ahoj") << QByteArray("ahoj");
    QTest::newRow("jan-kundrat") << QString::fromUtf8("Jan Kundrát") << QByteArray("=?iso-8859-1?Q?Jan_Kundr=E1t?=");
    QTest::newRow("czech") << QString::fromUtf8("ě") << QByteArray("=?utf-8?B?xJs=?=");
}

void RFCCodecsTest::testEncodeRFC2047StringAsciiPrefix()
{
    QFETCH(QString, input);
    QFETCH(QByteArray, encoded);

    QCOMPARE(Imap::encodeRFC2047StringWithAsciiPrefix(input), encoded);
    QCOMPARE(Imap::decodeRFC2047String(Imap::encodeRFC2047StringWithAsciiPrefix(input)), input);
}

void RFCCodecsTest::testEncodeRFC2047StringAsciiPrefix_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QByteArray>("encoded");

    QTest::newRow("empty") << QString() << QByteArray();
    QTest::newRow("simple-ascii") << QString::fromUtf8("ahoj") << QByteArray("ahoj");
    QTest::newRow("jan-kundrat") << QString::fromUtf8("Jan Kundrát") << QByteArray("Jan =?iso-8859-1?Q?Kundr=E1t?=");
    QTest::newRow("jan-kundrat-e") << QString::fromUtf8("Jan Kundrát ě") << QByteArray("Jan =?utf-8?B?S3VuZHLDoXQgxJs=?=");
    QTest::newRow("czech") << QString::fromUtf8("ě") << QByteArray("=?utf-8?B?xJs=?=");
    QTest::newRow("trojita-subjects") << QString::fromUtf8("[trojita] foo bar blesmrt") << QByteArray("[trojita] foo bar blesmrt");
    QTest::newRow("trojita-subjects-utf") << QString::fromUtf8("[trojita] foo bar ěščřžýáíé")
        << QByteArray("[trojita] foo bar =?utf-8?B?xJvFocSNxZnFvsO9w6HDrcOp?=");

    QTest::newRow("crlf") << QString::fromUtf8("\r\n")
        << QByteArray("=?iso-8859-1?Q?=0D=0A?=");

#if 0
    QTest::newRow("long-text-with-latin")
        // again, be careful with that trigraph
        << QString::fromUtf8("[Trojitá - Bug #553] (New) Subject \"=?UTF-8?B?" "?=\" not decoded")
        << QByteArray();
#endif

    QTest::newRow("long-text-with-utf")
        // again, be careful with that trigraph
        << QString::fromUtf8("[Trojitá - Bug #553] (New) Subject \"=?UTF-8?B?" "?=\" not decoded ěščřžýáíé")
        << QByteArray("=?utf-8?B?W1Ryb2ppdMOhIC0gQnVnICM1NTNdIChOZXcpIFN1YmplY3QgIj0/VVRGLTg/Qj8/PSIgbm90IGRlY29kZWQg?=\r\n"
                      " =?utf-8?B?xJvFocSNxZnFvsO9w6HDrcOp?=");
}

TROJITA_HEADLESS_TEST( RFCCodecsTest )
