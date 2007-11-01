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
#include <QStringList>
#include "Command.h"

namespace Imap {
namespace Commands {

    QTextStream& operator<<( QTextStream& stream, const AbstractCommand& cmd )
    {
        bool doSpace = false;
        for (QList<_PartOfCommand>::const_iterator it = cmd._cmds.begin(); it != cmd._cmds.end(); ++it, doSpace = true ) {
            if (doSpace)
                stream << " ";
            stream << *it;
        }
        return stream << endl;
    }

    QTextStream& operator<<( QTextStream& stream, const _PartOfCommand& part )
    {
        switch (part._kind) {
            case TOKEN_ATOM:
                stream << part._text;
                break;
            case TOKEN_QUOTED_STRING:
                stream << '"' << part._text << '"';
                break;
            case TOKEN_LITERAL:
                stream << "{" << part._text.length() << "}" << endl << part._text << endl;
                break;
        }
        return stream;
    }

    /*
     * Following functions are just dummy utilities that fills the interesting
     * fields with data
     */

    Capability::Capability()
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "CAPABILITY" ) );
    };

    Noop::Noop()
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "NOOP" ) );
    };

    Logout::Logout()
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "LOGOUT" ) );
    };


    StartTls::StartTls()
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "STARTTLS" ) );
    }

    Authenticate::Authenticate()
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "AUTHENTICATE" ) );
    }

    Login::Login( const QString& user, const QString& pass )
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "LOGIN" ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, user ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, pass ) );
    }


    UnSelect::UnSelect()
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "UNSELECT" ) );
    };

    Check::Check()
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "CHECK" ) );
    };

    Idle::Idle()
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "IDLE" ) );
    };

    Select::Select( const QString& mailbox )
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "SELECT" ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, mailbox ) );
    };

    Examine::Examine( const QString& mailbox )
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "EXAMINE" ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, mailbox ) );
    };

    Create::Create( const QString& mailbox )
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "CREATE" ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, mailbox ) );
    };

    Delete::Delete( const QString& mailbox )
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "DELETE" ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, mailbox ) );
    };

    Rename::Rename( const QString& oldName, const QString& newName )
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "RENAME" ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, oldName ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, newName ) );
    };

    Subscribe::Subscribe( const QString& mailbox )
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "SUBSCRIBE" ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, mailbox ) );
    };

    UnSubscribe::UnSubscribe( const QString& mailbox )
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "UNSUBSCRIBE" ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, mailbox ) );
    };

    List::List( const QString& reference, const QString& mailbox )
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "LIST" ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, reference ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, mailbox ) );
    };

    LSub::LSub( const QString& reference, const QString& mailbox )
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "LSUB" ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, reference ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, mailbox ) );
    };

    Status::Status( const QString& mailbox, const QStringList& fields )
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "STATUS" ) );
        _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, mailbox ) );
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "(" + fields.join(" ") +")") );
    }

    Append::Append( const QString& mailbox, const QString& message, const QStringList& flags, const QDateTime& timeStamp )
    {
        _cmds.append( _PartOfCommand( TOKEN_ATOM, "APPEND" ) );
        if (flags.count())
            _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, "(" + flags.join(" ") + ")" ) );
        if (timeStamp.isValid())
            _cmds.append( _PartOfCommand( TOKEN_QUOTED_STRING, timeStamp.toString() ) );
        _cmds.append( _PartOfCommand( TOKEN_LITERAL, message ) );
    }
}
}
