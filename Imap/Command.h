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
#include <QDateTime>

/** Namespace for IMAP interaction */
namespace Imap {

/** Namespace holding all supported IMAP commands and variosu helpers */
namespace Commands {

    /** Enumeration that specifies required method of transmission of this string */
    enum TokenType {
        ATOM /**< Don't use any extra encoding, just send it directly, perhaps because it's already encoded */,
        QUOTED_STRING /**< Transmit using double-quotes */,
        LITERAL /**< Don't bother with checking this data, always use literal form */
    };

    /** Checks if we can use a quoted-string form for transmitting this string.
     *
     * We have to use literals for transmitting strings that are either too
     * long (as that'd cause problems with servers using too small line buffers),
     * contains CR, LF, zero byte or any characters outside of 7-bit ASCII range.
     */
    TokenType howToTransmit( const QString& str );

    /** A part of the actual command.
     *
     * This is used by Parser to decide whether
     * to send the string as-is, to quote them or use a literal form for them.
     */
    class PartOfCommand {
        TokenType _kind; /**< What encoding to use for this item */
        QString _text; /**< Actual text to send */

        friend QTextStream& operator<<( QTextStream& stream, const PartOfCommand& c );

    public:
        /** Default constructor */
        PartOfCommand( const TokenType kind, const QString& text): _kind(kind), _text(text) {};
        /** Constructor that guesses correct type for passed string */
        PartOfCommand( const QString& text): _kind( howToTransmit(text) ), _text(text) {};

    };

    /** Abstract class for specifying what command to execute */
    class AbstractCommand {
        friend  QTextStream& operator<<( QTextStream& stream, const AbstractCommand& c );
    public:
        virtual ~AbstractCommand() {};
    protected:
        QList<PartOfCommand> _cmds;
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
        Append( const QString& mailbox, const QString& message, const QStringList& flags = QStringList(), const QDateTime& timestamp = QDateTime() );
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


    /** X<atom>, RFC3501 sect 6.5.1 */
    class XAtom : public AbstractCommand {
        /* If I try to use ': _cmds(commands) {};', I get following error
         * message with Gentoo Hardened GCC 3.4.6-r2:
         *
         * In constructor `Imap::Commands::XAtom::XAtom(const QList<Imap::Commands::PartOfCommand>&)':
         *     error: class `Imap::Commands::XAtom' does not have any field named `_cmds'
         *
         * Isn't that a compiler bug?
         * */
        XAtom( const QList<PartOfCommand>& commands ) { _cmds = commands; };
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
