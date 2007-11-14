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

#include "testrfccodecs.h"
#include "testrfccodecs.moc"

QTEST_KDEMAIN_CORE( RFCCodecsTest )

#include "kimap/rfccodecs.h"
using namespace KIMAP;

void RFCCodecsTest::testIMAPEncoding()
{
  QString encoded, decoded;

  encoded = encodeImapFolderName( "Test.Frode Rønning" );
  QVERIFY( encoded == "Test.Frode R&APg-nning" );
  decoded = decodeImapFolderName( "Test.Frode R&APg-nning" );
  QVERIFY( decoded == "Test.Frode Rønning" );

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

