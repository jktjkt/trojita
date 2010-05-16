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
#ifndef TEST_IMAP_PARSER_PARSE
#define TEST_IMAP_PARSER_PARSE

#include <QtCore/QObject>
#include "Imap/Parser/LowLevelParser.h"


/** @short Unit tests for Imap::LowLevelParser */
class ImapLowLevelParserTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    /** @short test Imap::LowLevelParser::parseList() */
    void testParseList();
    /** @short test Imap::LowLevelParser::getString() */
    void testGetString();
    /** @short test Imap::LowLevelParser::getAString() */
    void testGetAString();
    /** @short test Imap::LowLevelParser::getUInt() */
    void testGetUInt();
    /** @short test Imap::LowLevelParser::getAtom() */
    void testGetAtom();
    /** @short test Imap::LowLevelParser::getAnything() */
    void testGetAnything();
    /** @short Test Imap::LowLevelParser::getRFC2822DateTime() */
    void testGetRFC2822DateTime();
    void testGetRFC2822DateTime_data();
};

#endif
