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
#ifndef IMAP_RESPONSE_H
#define IMAP_RESPONSE_H

#include <QTextStream>
#include <QString>
#include <QByteArray>
#include <QList>
#include "Imap/Command.h"

/**
 * @file
 * Various data structures related to IMAP responses
 *
 * @author Jan Kundrát <jkt@gentoo.org>
 */

/** Namespace for IMAP interaction */
namespace Imap {

/** IMAP server responses */
namespace Responses {

    /** Result of a command */
    enum CommandResult {
        OK /**< OK */,
        NO /**< NO */,
        BAD /**< BAD */
    }; // aren't those comments just sexy? :)

    /** Response Code */
    enum Code {
        NONE /**< No response code specified */,
        ATOM /**< Not recognized */,
        ALERT /**< ALERT */,
        BADCHARSET /**< BADCHARSET */,
        CAPABILITY /**< CAPABILITY */,
        PARSE /**< PARSE */,
        PERMANENTFLAGS /**< PERMANENTFLAGS */,
        READ_ONLY /**< READ-ONLY */, 
        READ_WRITE /**< READ-WRITE */,
        TRYCREATE /**< TRYCREATE */,
        UIDNEXT /**< UIDNEXT */,
        UIDVALIDITY /**< UIDVALIDITY */,
        UNSEEN /**< UNSEEN */
    }; // luvly comments, huh? :)

    /** Parsed IMAP server response */
    class Response {
        QString _tag;
        CommandResult _result;
        Code _code;
        QList<QByteArray> _codeList;
        QByteArray _data;
    public:
        Response( const QString& tag, const CommandResult result,
                const Code code, const QList<QByteArray>& codeList,
                const QByteArray& data ) : _tag(tag), _result(result),
                    _code(code), _codeList(codeList), _data(data) {};
        Response() : _result(BAD), _code(NONE) {};
        const QString& tag() const { return _tag; };
        const CommandResult& result() const { return _result; };
        const Code& code() const { return _code; };
        const QList<QByteArray>& codeList() const { return _codeList; };
        const QByteArray& data() const { return _data; };
    };

    QTextStream& operator<<( QTextStream& stream, const Code& r );
    QTextStream& operator<<( QTextStream& stream, const Response& r );
    QTextStream& operator<<( QTextStream& stream, const CommandResult& res );
    bool operator==( const Response& r1, const Response& r2 );

}

}

#endif // IMAP_RESPONSE_H
