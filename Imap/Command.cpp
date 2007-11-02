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

    QTextStream& operator<<( QTextStream& stream, const AbstractCommand& cmd )
    {
        bool doSpace = false;
        for (QList<PartOfCommand>::const_iterator it = cmd._cmds.begin(); it != cmd._cmds.end(); ++it, doSpace = true ) {
            if (doSpace)
                stream << " ";
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
                return stream << part._text;
            case QUOTED_STRING:
                return stream << '"' << part._text << '"';
            case LITERAL:
                return stream << "{" << part._text.length() << "}" << endl << part._text;
        }
    }

    /*
     * Following functions are just dummy utilities that fills the interesting
     * fields with data
     */

    Capability::Capability()
    {
        _cmds.append( PartOfCommand( ATOM, "CAPABILITY" ) );
    };

    Noop::Noop()
    {
        _cmds.append( PartOfCommand( ATOM, "NOOP" ) );
    };

    Logout::Logout()
    {
        _cmds.append( PartOfCommand( ATOM, "LOGOUT" ) );
    };


    StartTls::StartTls()
    {
        _cmds.append( PartOfCommand( ATOM, "STARTTLS" ) );
    }

    Authenticate::Authenticate()
    {
        _cmds.append( PartOfCommand( ATOM, "AUTHENTICATE" ) );
    }

    Login::Login( const QString& user, const QString& pass )
    {
        _cmds.append( PartOfCommand( ATOM, "LOGIN" ) );
        _cmds.append( PartOfCommand( user ) );
        _cmds.append( PartOfCommand( pass ) );
    }


    UnSelect::UnSelect()
    {
        _cmds.append( PartOfCommand( ATOM, "UNSELECT" ) );
    };

    Check::Check()
    {
        _cmds.append( PartOfCommand( ATOM, "CHECK" ) );
    };

    Idle::Idle()
    {
        _cmds.append( PartOfCommand( ATOM, "IDLE" ) );
    };

    Select::Select( const QString& mailbox )
    {
        _cmds.append( PartOfCommand( ATOM, "SELECT" ) );
        _cmds.append( PartOfCommand( mailbox ) );
    };

    Examine::Examine( const QString& mailbox )
    {
        _cmds.append( PartOfCommand( ATOM, "EXAMINE" ) );
        _cmds.append( PartOfCommand( mailbox ) );
    };

    Create::Create( const QString& mailbox )
    {
        _cmds.append( PartOfCommand( ATOM, "CREATE" ) );
        _cmds.append( PartOfCommand( mailbox ) );
    };

    Delete::Delete( const QString& mailbox )
    {
        _cmds.append( PartOfCommand( ATOM, "DELETE" ) );
        _cmds.append( PartOfCommand( mailbox ) );
    };

    Rename::Rename( const QString& oldName, const QString& newName )
    {
        _cmds.append( PartOfCommand( ATOM, "RENAME" ) );
        _cmds.append( PartOfCommand( oldName ) );
        _cmds.append( PartOfCommand( newName ) );
    };

    Subscribe::Subscribe( const QString& mailbox )
    {
        _cmds.append( PartOfCommand( ATOM, "SUBSCRIBE" ) );
        _cmds.append( PartOfCommand( mailbox ) );
    };

    UnSubscribe::UnSubscribe( const QString& mailbox )
    {
        _cmds.append( PartOfCommand( ATOM, "UNSUBSCRIBE" ) );
        _cmds.append( PartOfCommand( mailbox ) );
    };

    List::List( const QString& reference, const QString& mailbox )
    {
        _cmds.append( PartOfCommand( ATOM, "LIST" ) );
        _cmds.append( PartOfCommand( reference ) );
        _cmds.append( PartOfCommand( mailbox ) );
    };

    LSub::LSub( const QString& reference, const QString& mailbox )
    {
        _cmds.append( PartOfCommand( ATOM, "LSUB" ) );
        _cmds.append( PartOfCommand( reference ) );
        _cmds.append( PartOfCommand( mailbox ) );
    };

    Status::Status( const QString& mailbox, const QStringList& fields )
    {
        _cmds.append( PartOfCommand( ATOM, "STATUS" ) );
        _cmds.append( PartOfCommand( mailbox ) );
        _cmds.append( PartOfCommand( ATOM, "(" + fields.join(" ") +")") );
    }

    Append::Append( const QString& mailbox, const QString& message, const QStringList& flags, const QDateTime& timeStamp )
    {
        _cmds.append( PartOfCommand( ATOM, "APPEND" ) );
        if (flags.count())
            _cmds.append( PartOfCommand( ATOM, "(" + flags.join(" ") + ")" ) );
        if (timeStamp.isValid())
            _cmds.append( PartOfCommand( timeStamp.toString() ) );
        _cmds.append( PartOfCommand( LITERAL, message ) );
    }
}
}
