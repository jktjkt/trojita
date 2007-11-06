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
#ifndef IMAP_PARSER_H
#define IMAP_PARSER_H
#include <QObject>
#include <Imap/Response.h>
#include <Imap/Command.h>

/**
 * @file
 * A header file defining Parser class and various helpers.
 *
 * @author Jan Kundrát <jkt@gentoo.org>
 */

class QAbstractSocket;
class QDateTime;
template<class T> class QList;

/** Namespace for IMAP interaction */
namespace Imap {

    /** A handle for accessing command results, FIXME */
    class CommandHandle {};

    /** Flag as defined in RFC3501, section 2.3.2, FIXME */
    class Flag;

    /** Threading algorithm for THREAD command */
    enum ThreadAlgorithm {
        THREAD_ORDEREDSUBJECT /**< ORDEREDSUBJECT algorithm */,
        THREAD_REFERENCES /**< REFERENCES algorithm */
    };

    class Authenticator {};
    class Message;

    /** Class specifying a set of messagess to access */
    class Sequence {
    public:
        /** Converts sequence to string suitable for sending over the wire */
        QString toString() const {return "";};
    };

    /** Invalid argument was passed to some function */
    class InvalidArgumentException: public std::exception {
        /** The error message */
        std::string _msg;
    public:
        InvalidArgumentException( const std::string& msg ) : _msg( msg ) {};
        virtual const char* what() const throw() { return _msg.c_str(); };
        virtual ~InvalidArgumentException() throw() {};
    };


/** Class that does all IMAP parsing */
class Parser : public QObject {
    Q_OBJECT

public:
    /** Constructor. Takes an QAbstractSocket instance as a parameter. */
    Parser( QObject* parent, QAbstractSocket * const socket );

public slots:

    /** CAPABILITY, RFC 3501 section 6.1.1 */
    CommandHandle capability();

    /** NOOP, RFC 3501 section 6.1.2 */
    CommandHandle noop();

    /** LOGOUT, RFC3501 section 6.1.3 */
    CommandHandle logout();


    /** STARTTLS, RFC3051 section 6.2.1 */
    CommandHandle startTls();

    /** AUTHENTICATE, RFC3501 section 6.2.2 */
    CommandHandle authenticate( /* FIXME: parameter */ );

    /** LOGIN, RFC3501 section 6.2.3 */
    CommandHandle login( const QString& user, const QString& pass );


    /** SELECT, RFC3501 section 6.3.1 */
    CommandHandle select( const QString& mailbox );

    /** EXAMINE, RFC3501 section 6.3.2 */
    CommandHandle examine( const QString& mailbox );

    /** CREATE, RFC3501 section 6.3.3 */
    CommandHandle create( const QString& mailbox );

    /** DELETE, RFC3501 section 6.3.4 */
    CommandHandle deleteMailbox( const QString& mailbox );

    /** RENAME, RFC3501 section 6.3.5 */
    CommandHandle rename( const QString& oldName, const QString& newName );

    /** SUBSCRIBE, RFC3501 section 6.3.6 */
    CommandHandle subscribe( const QString& mailbox );
    
    /** UNSUBSCRIBE, RFC3501 section 6.3.7 */
    CommandHandle unSubscribe( const QString& mailbox );

    /** LIST, RFC3501 section 6.3.8 */
    CommandHandle list( const QString& reference, const QString& mailbox );
    
    /** LSUB, RFC3501 section 6.3.9 */
    CommandHandle lSub( const QString& reference, const QString& mailbox );

    /** STATUS, RFC3501 section 6.3.10 */
    CommandHandle status( const QString& mailbox, const QStringList& fields );
    
    /** APPEND, RFC3501 section 6.3.11 */
    CommandHandle append( const QString& mailbox, const QString& message, const QStringList& flags = QStringList(), const QDateTime& timestamp = QDateTime() );


    /** CHECK, RFC3501 sect 6.4.1 */
    CommandHandle check();

    /** CLOSE, RFC3501 sect 6.4.2 */
    CommandHandle close();
    
    /** EXPUNGE, RFC3501 sect 6.4.3 */
    CommandHandle expunge();

    /** SEARCH, RFC3501 sect 6.4.4 */
    CommandHandle search( const QStringList& criteria, const QString& charset = QString::null ) {
        return _searchHelper( "SEARCH", criteria, charset );
    };

    /** FETCH, RFC3501 sect 6.4.5 */
    CommandHandle fetch( const Sequence& seq, const QStringList& items );

    /** STORE, RFC3501 sect 6.4.6 */
    CommandHandle store( const Sequence& seq, const QString& item, const QString& value );

    /** COPY, RFC3501 sect 6.4.7 */
    CommandHandle copy( const Sequence& seq, const QString& mailbox );

    /** UID command (FETCH), RFC3501 sect 6.4.8 */
    CommandHandle uidFetch( const Sequence& seq, const QStringList& items );

    /** UID command (SEARCH), RFC3501 sect 6.4.8 */
    CommandHandle uidSearch( const QStringList& criteria, const QString& charset ) {
        return _searchHelper( "UID SEARCH", criteria, charset );
    };


    /** X<atom>, RFC3501 sect 6.5.1 */
    CommandHandle xAtom( const Commands::Command& commands );


    /** UNSELECT, RFC3691 */
    CommandHandle unSelect();
    
    /** IDLE, RFC2177 */
    CommandHandle idle();

#if 0

    /** SORT, draft-ietf-imapext-sort-19, section 3 */
    CommandHandle sort( /*const SortAlgorithm& algo,*/ const QString& charset, const QStringList& criteria );
    /** UID SORT, draft-ietf-imapext-sort-19, section 3 */
    CommandHandle uidSort( /*const SortAlgorithm& algo,*/ const QString charset, const QStringList& criteria );
    /** THREAD, draft-ietf-imapext-sort-19, section 3 */
    CommandHandle thread( const ThreadAlgorithm& algo, const QString charset, const QStringList& criteria );
#endif

/*signals:
    void responseReceived( std::tr1::shared_ptr<Responses::AbstractResponse> resp );*/

private:
    /** Private copy constructor */
    Parser( const Parser& );
    /** Private assignment operator */
    Parser& operator=( const Parser& );

    /** Queue command for execution.*/
    CommandHandle queueCommand( const Commands::Command& command );

    /** Shortcut function; works exactly same as above mentioned queueCommand() */
    CommandHandle queueCommand( const Commands::TokenType kind, const QString& text ) {
        return queueCommand( Commands::Command() << Commands::PartOfCommand( kind, text ) );
    };

    /** Helper for search() and uidSearch() */
    CommandHandle _searchHelper( const QString& command, const QStringList& criteria, const QString& charset = QString::null );


    QAbstractSocket* _socket;
};

}
#endif /* IMAP_PARSER_H */
