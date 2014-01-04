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
#ifndef TEST_IMAP_PARSER_PARSE
#define TEST_IMAP_PARSER_PARSE

#include <memory>
#include <QtCore/QObject>
#include "Imap/Parser/Parser.h"

class QByteArray;
class QBuffer;

/** @short Unit tests for Imap::Parser */
class ImapParserParseTest : public QObject
{
    Q_OBJECT
    std::unique_ptr<QByteArray> array;
    Imap::Parser* parser;
private Q_SLOTS:
    /** @short Test parsing of various tagged responses */
    void testParseTagged();
    void testParseTagged_data();
    /** @short Test parsing of untagged responses */
    void testParseUntagged();
    void testParseUntagged_data();
    /** @short Test that we can parse that garbage without resorting to a fatal error */
    void testParseFetchGarbageWithoutExceptions();
    void testParseFetchGarbageWithoutExceptions_data();

    /** @short Test sequence output */
    void testSequences();
    void testSequences_data();
    /** @short Test for parsing errors */
    void testThrow();
    void testThrow_data();

    void initTestCase();
    void cleanupTestCase();

    void benchmark();
    void benchmarkInitialChat();
};

#endif
