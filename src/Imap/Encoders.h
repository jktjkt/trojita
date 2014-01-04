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
#ifndef IMAP_ENCODERS_H
#define IMAP_ENCODERS_H

#include <QMap>
#include <QString>

namespace Imap {

typedef enum {
    DoubleQuoted,
    SquareBrackets,
    Parentheses
} QuotedStringStyle;

typedef enum {
    RFC2047_STRING_ASCII,
    RFC2047_STRING_LATIN,
    RFC2047_STRING_UTF8
} Rfc2047StringCharacterSetType;

QString decodeByteArray(const QByteArray &encoded, const QString &charset);

QByteArray quotedString(const QByteArray &unquoted, QuotedStringStyle style = DoubleQuoted);
QByteArray encodeRFC2047Phrase(const QString &text);

QByteArray encodeRFC2047StringWithAsciiPrefix(const QString &text);
QString decodeRFC2047String(const QByteArray &raw);

QByteArray encodeImapFolderName(const QString &text);

QString decodeImapFolderName(const QByteArray &raw);

QByteArray quotedPrintableDecode(const QByteArray &raw);
QByteArray quotedPrintableEncode(const QByteArray &raw);

QString extractRfc2231Param(const QMap<QByteArray, QByteArray> &parameters, const QByteArray &key);
QByteArray encodeRfc2231Parameter(const QByteArray &key, const QString &value);

QString wrapFormatFlowed(const QString &input);

void decodeContentTransferEncoding(const QByteArray &rawData, const QByteArray &encoding, QByteArray *outputData);
}

#endif // IMAP_ENCODERS_H
