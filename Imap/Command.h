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
#ifndef IMAP_COMMAND_H
#define IMAP_COMMAND_H
#include <QTextStream>
#include <QList>

/** Namespace for IMAP interaction */
namespace Imap {

namespace Commands {

    /** Enumeration that specifies required encoding for this string */
    enum TokenType {
        TOKEN_ATOM /**< No encoding, just send it directly */,
        TOKEN_QUOTED_STRING /**< Add quotes, escape them */,
        TOKEN_LITERAL /** Use a string literal syntax */
    };

    /** A part of the actual command. */
    class _PartOfCommand {
        TokenType _kind; /**< WHat encoding to use for this item */
        QString _text; /**< Actual text to send */

        friend QTextStream& operator<<( QTextStream& stream, const _PartOfCommand& c );

    public:
        /**< Default constructor */
        _PartOfCommand( const TokenType kind, const QString& text): _kind(kind), _text(text) {};

    };

    /** Abstract class for specifying what command to execute */
    class AbstractCommand {
        friend  QTextStream& operator<<( QTextStream& stream, const AbstractCommand& c );
    public:
        virtual ~AbstractCommand() {};
    protected:
        QList<_PartOfCommand> _cmds;
    };



    /** CAPABILITY, RFC 3501 section 6.1.1 */
    class Capability : public AbstractCommand {
    public:
        Capability();
    };

    /** NOOP, RFC 3501 section 6.1.2 */
    class Noop : public AbstractCommand {
    public:
        Noop();
    };

    /** LOGOUT, RFC3501 section 6.1.3 */
    class Logout : public AbstractCommand {
    public:
        Logout();
    };


    /** STARTTLS, RFC3051 section 6.2.1 */
    class StartTls : public AbstractCommand {
    public:
        StartTls();
    };

    /** AUTHENTICATE, RFC3501 section 6.2.2 */
    class Authenticate : public AbstractCommand {
    public:
        Authenticate( /* FIXME: parameter */ );
    private:
        void* _authenticator;
    };

    /** LOGIN, RFC3501 section 6.2.3 */
    class Login : public AbstractCommand {
    public:
        Login( const QString& user, const QString& pass );
    };


    /** SELECT, RFC3501 section 6.3.1 */
    class Select : public AbstractCommand {
    public:
        Select( const QString& mailbox );
    };

    /** EXAMINE, RFC3501 section 6.3.2 */
    class Examine : public AbstractCommand {
    public:
        Examine( const QString& mailbox );
    };

    /** CREATE, RFC3501 section 6.3.3 */
    class Create : public AbstractCommand {
    public:
        Create( const QString& mailbox );
    };

    /** DELETE, RFC3501 section 6.3.4 */
    class Delete : public AbstractCommand {
    public:
        Delete( const QString& mailbox );
    };

    /** RENAME, RFC3501 section 6.3.5 */
    class Rename : public AbstractCommand {
    public:
        Rename( const QString& oldName, const QString& newName );
    };

    /** SUBSCRIBE, RFC3501 section 6.3.6 */
    class Subscribe : public AbstractCommand {
    public:
        Subscribe( const QString& mailbox );
    };

    /** UNSUBSCRIBE, RFC3501 section 6.3.7 */
    class UnSubscribe : public AbstractCommand {
    public:
        UnSubscribe( const QString& mailbox );
    };

    /** LIST, RFC3501 section 6.3.8 */
    class List : public AbstractCommand {
    public:
        List( const QString& reference, const QString& mailbox );
    };

    /** LSUB, RFC3501 section 6.3.9 */
    class LSub : public AbstractCommand {
    public:
        LSub( const QString& reference, const QString& mailbox );
    };

    /** STATUS, RFC3501 section 6.3.10 */
    class Status : public AbstractCommand {
    public:
        Status( const QString& mailbox, const QStringList& fields );
    };

    /** APPEND, RFC3501 section 6.3.11 */
    class Append : public AbstractCommand {
    public:
        Append( const QString& mailbox );
    };



    /** CHECK, RFC3501 sect 6.4.1 */
    class Check : public AbstractCommand {
    public:
        Check();
    };

    /** CLOSE, RFC3501 sect 6.4.2 */
    class Close : public AbstractCommand {
    public:
        Close();
    };

    /** EXPUNGE, RFC3501 sect 6.4.3 */
    class Expunge : public AbstractCommand {
    public:
        Expunge();
    };


    /** UNSELECT, RFC3691 */
    class UnSelect : public AbstractCommand {
    public:
        UnSelect();
    };

    /** IDLE, RFC2177 */
    class Idle : public AbstractCommand {
    public:
        Idle();
    };



    /** Used for dumping a command to debug stream */
    QTextStream& operator<<( QTextStream& stream, const AbstractCommand& cmd );

    /** Helper for operator<<( QTextStream& stream, const Command& cmd ) */
    QTextStream& operator<<( QTextStream& stream, const AbstractCommand& part );

    }
}
#endif /* IMAP_COMMAND_H */
