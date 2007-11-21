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
#include "Imap/Response.h"
#include "Imap/Exceptions.h"

namespace Imap {
namespace Responses {

QTextStream& operator<<( QTextStream& stream, const Code& r )
{
    switch (r) {
        case ATOM:
            stream << "<<ATOM>>'"; break;
        case ALERT:
            stream << "ALERT"; break;
        case BADCHARSET:
            stream << "BADCHARSET"; break;
        case CAPABILITIES:
            stream << "CAPABILITY"; break;
        case PARSE:
            stream << "PARSE"; break;
        case PERMANENTFLAGS:
            stream << "PERMANENTFLAGS"; break;
        case READ_ONLY:
            stream << "READ-ONLY"; break;
        case READ_WRITE:
            stream << "READ-WRITE"; break;
        case TRYCREATE:
            stream << "TRYCREATE"; break;
        case UIDNEXT:
            stream << "UIDNEXT"; break;
        case UIDVALIDITY:
            stream << "UIDVALIDITY"; break;
        case UNSEEN:
            stream << "UNSEEN"; break;
        case NONE:
            stream << "<<NONE>>"; break;
    }
    return stream;
}

QTextStream& operator<<( QTextStream& stream, const Response& r )
{
    stream << "tag: " << r.tag() << ", kind: " << r.kind() << ", code: " << r.code();
    if ( r.code() != NONE ) {
        stream << " (";
        for ( QStringList::const_iterator it = r.codeList().begin(); it != r.codeList().end(); ++it )
            stream << *it << " ";
        stream << ')';
    }
    stream << ( r.tag().isEmpty() ? ", data: " : ", text: " ) << r.data() << endl;
    return stream;
}

QTextStream& operator<<( QTextStream& stream, const Kind& res )
{
    switch ( res ) {
        case OK:
            stream << "OK";
            break;
        case NO:
            stream << "NO";
            break;
        case BAD:
            stream << "BAD";
            break;
        case BYE:
            stream << "BYE";
            break;
        case PREAUTH:
            stream << "PREAUTH";
            break;
        case EXPUNGE:
            stream << "EXPUNGE";
            break;
        case FETCH:
            stream << "FETCH";
            break;
        case EXISTS:
            stream << "EXISTS";
            break;
        case RECENT:
            stream << "RECENT";
            break;
        case CAPABILITY:
            stream << "CAPABILITY";
            break;
        case LIST:
            stream << "LIST";
    }
    return stream;
}

bool operator==( const Response& r1, const Response& r2 )
{
    return r1.tag() == r2.tag() && r1.kind() == r2.kind() && r1.code() == r2.code() && r1.codeList() == r2.codeList() && r1.data() == r2.data();
}

Kind kindFromString( QByteArray str )
{
    str = str.toUpper();

    if ( str == "OK" )
        return OK;
    if ( str == "NO" )
        return NO;
    if ( str == "BAD" )
        return BAD;
    if ( str == "BYE" )
        return BYE;
    if ( str == "PREAUTH" )
        return PREAUTH;
    if ( str == "FETCH" )
        return FETCH;
    if ( str == "EXPUNGE" )
        return EXPUNGE;
    if ( str == "FETCH" )
        return FETCH;
    if ( str == "EXISTS" )
        return EXISTS;
    if ( str == "RECENT" )
        return RECENT;
    if ( str == "CAPABILITY" )
        return CAPABILITY;
    if ( str == "LIST" )
        return LIST;
    throw UnrecognizedResponseKind( str.constData() );
}

}
}
