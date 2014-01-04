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
#ifndef IMAP_LOWLEVELPARSER_H
#define IMAP_LOWLEVELPARSER_H

#include <QList>
#include <QPair>
#include <QVariant>

namespace Imap
{

/** @short Low-level parsing of IMAP data
 *
 * This namespace contains functions for extracting low-level stuff like atoms,
 * integers or strings from a raw data that server sent us.
 *
 * All functions share a similar API -- first argument, a QByteArray instance,
 * holds *complete* line including trailing CRLF, second argument is offset
 * where the parsing should start. It's a non-const reference to int as it gets
 * changed after the requested item is read.
 *
 * All functions assume that all {size}-based literals are already prefetched,
 * ie. there's no interaction with the Imap::Parser's methods for retrieving
 * further data.
 * */
namespace LowLevelParser
{

enum ParsedAs {
    ATOM /**< @short Parsed as RFC3501 "atom" data type */,
    QUOTED /**< @short Quoted string (enclosed in single pair of double quotes */,
    LITERAL /**< @short String literal, ie. the {size}-form */,
    LITERAL8 /**< @short The literal8 syntax from BINARY */,
    NIL /**< @short A special-case atom NIL */
};

/** @short Read an unsigned integer from input */
uint getUInt(const QByteArray &line, int &start);

/** @short Read a 64bit unsigned integer from input */
quint64 getUInt64(const QByteArray &line, int &start);

/** @short Read an ATOM */
QByteArray getAtom(const QByteArray &line, int &start);
QByteArray getPossiblyBackslashedAtom(const QByteArray &line, int &start);

/** @short Read a quoted string or literal */
QPair<QByteArray,ParsedAs> getString(const QByteArray &line, int &start);

/** @short Read atom or string */
QPair<QByteArray,ParsedAs> getAString(const QByteArray &line, int &start);

/** @short Read NIL or a string */
QPair<QByteArray,ParsedAs> getNString(const QByteArray &line, int &start);

/** @short Retrieve mailbox name */
QString getMailbox(const QByteArray &line, int &start);

/** @short Parse parenthesized list
 *
 * Parenthesized lists is defined as a sequence of space-separated strings
 * enclosed between "open" and "close" characters.
 *
 * @param open, close -- enclosing parentheses
 * @param line, start -- full line data and starting offset
 *
 * We need to support parsing of nested lists (as found in the envelope data
 * structure), that's why we deal with QVariant here.
 * */
QVariantList parseList(const char open, const char close,
                       const QByteArray &line, int &start);

/** @short Read one item from input, store it in a most-appropriate form */
QVariant getAnything(const QByteArray &line, int &start);

/** @short Parse a sequence set from the input */
QList<uint> getSequence(const QByteArray &line, int &start);

/** @short Parse RFC2822-like formatted date
 *
 * Code for this class was lobotomized from KDE's KDateTime.
 * */
QDateTime parseRFC2822DateTime(const QString &string);

/** @short Eat spaces as long as we can */
void eatSpaces(const QByteArray &line, int &start);
}
}

#endif /* IMAP_LOWLEVELPARSER_H */
