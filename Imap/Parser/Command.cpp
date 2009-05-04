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
#include <ctype.h>
#include <QStringList>
#include "Command.h"

namespace Imap {
namespace Commands {

    QTextStream& operator<<( QTextStream& stream, const Command& cmd )
    {
        for (QList<PartOfCommand>::const_iterator it = cmd._cmds.begin(); it != cmd._cmds.end(); ++it ) {
            if ( it != cmd._cmds.begin () ) {
                stream << " ";
            }
            stream << *it;
        }
        return stream << endl;
    }

    TokenType howToTransmit( const QString& str )
    {
        if (str.length() > 100)
            return LITERAL;

        if (str.isEmpty())
            return QUOTED_STRING;

        TokenType res = ATOM;

        for ( int i = 0; i < str.size(); ++i ) {
            char c = str.at(i).toAscii();

            if ( !isalnum(c) )
                res = QUOTED_STRING;

            if ( !isascii(c) || c == '\r' || c == '\n' || c == '\0' || c == '"' ) {
                return LITERAL;
            }
        }
        return res;
    }

    QTextStream& operator<<( QTextStream& stream, const PartOfCommand& part )
    {
        switch (part._kind) {
            case ATOM:
                stream << part._text;
                break;
            case QUOTED_STRING:
                stream << '"' << part._text << '"';
                break;
            case LITERAL:
                stream << "{" << part._text.length() << "}" << endl << part._text;
                break;
            case IDLE:
                stream << "IDLE" << endl << "[Entering IDLE mode...]";
                break;
            case STARTTLS:
                stream << "STARTTLS" << endl << "[Starting TLS...]";
                break;
        }
        return stream;
    }

}
}
