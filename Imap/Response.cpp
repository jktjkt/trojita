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

namespace Imap {

QTextStream& operator<<( QTextStream& stream, const ResponseCode& r )
{
    switch (r) {
        case ATOM:
            stream << "<<ATOM>>'"; break;
        case ALERT:
            stream << "ALERT"; break;
        case BADCHARSET:
            stream << "BADCHARSET"; break;
        case CAPABILITY:
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
    stream << "tag: " << r.tag() << ", result: " << r.result() << ", respCode: " << r.respCode();
    if ( r.respCode() != NONE ) {
        stream << " (";
        for ( QList<QByteArray>::const_iterator it = r.respCodeList().begin(); it != r.respCodeList().end(); ++it )
            stream << *it << " ";
        stream << ')';
    }
    stream << ( r.tag().isEmpty() ? ", data: " : ", text: " ) << r.data() << endl;
    return stream;
}

}
