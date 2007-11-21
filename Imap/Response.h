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
#include <QStringList>
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
    enum Kind {
        OK /**< OK */,
        NO /**< NO */,
        BAD /**< BAD */,
        BYE,
        PREAUTH,
        EXPUNGE,
        FETCH,
        EXISTS,
        RECENT,
        CAPABILITY,
        LIST
    }; // aren't those comments just sexy? :)

    /** Response Code */
    enum Code {
        NONE /**< No response code specified */,
        ATOM /**< Not recognized */,
        ALERT /**< ALERT */,
        BADCHARSET /**< BADCHARSET */,
        CAPABILITIES /**< CAPABILITY. Yeah, it's different than the RFC3501 name for it. Responses::Kind already defines a CAPABILITY. */,
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
        Kind _kind;
        Code _code;
        QStringList _codeList;
        QByteArray _data;
        uint _number;
        friend QTextStream& operator<<( QTextStream& stream, const Response& r );
        friend bool operator==( const Response& r1, const Response& r2 );
    public:
        Response( const QString& tag, const Kind kind,
                const Code code, const QStringList& codeList,
                const QByteArray& data ) : _tag(tag), _kind(kind),
                    _code(code), _codeList(codeList), _data(data) {};
        static Response makeNumberResponse( const Kind kind, const uint number ) {
            Response resp;
            resp._kind = kind;
            resp._number = number;
            return resp;
        };
        static Response makeCapability( const QStringList& caps ) {
            Response resp;
            resp._kind = CAPABILITY;
            resp._codeList = caps;
            return resp;
        };
        Response() : _kind(BAD), _code(NONE) {};
    };

    QTextStream& operator<<( QTextStream& stream, const Code& r );
    QTextStream& operator<<( QTextStream& stream, const Response& r );
    QTextStream& operator<<( QTextStream& stream, const Kind& res );
    bool operator==( const Response& r1, const Response& r2 );

    /** Build Responses::Kind from textual value */
    Kind kindFromString( QByteArray str );

}

}

#endif // IMAP_RESPONSE_H
