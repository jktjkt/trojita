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

class QAbstractSocket;
class QDateTime;
template<class T> class QList;

/** Namespace for IMAP interaction */
namespace Imap {

    /** A handle for accessing command results, FIXME */
    class CommandHandle {};
    /** Flag as defined in RFC3501, section 2.3.2, FIXME */
    class Flag;
    /** Fetchable item as specified in RFC3501, section 6.4.5, FIXME */
    class Item;

    /** Threading algorithm for THREAD command */
    enum ThreadAlgorithm {
        THREAD_ORDEREDSUBJECT /**< ORDEREDSUBJECT algorithm */,
        THREAD_REFERENCES /**< REFERENCES algorithm */
    };

    class Authenticator {};
    class Message;

    /** Class specifying a set of messagess to access */
    class Sequence;
    /** Parsed IMAP response */
    class Response {};


/** Class that does all IMAP parsing */
class Parser : public QObject {
    Q_OBJECT

private:
    // private copy constructors,...
    Parser( const Parser& );
    Parser& operator=( const Parser& );

public:
    /** Constructor. Takes an QAbstractSocket instance as a parameter. */
    Parser( QObject* parent, QAbstractSocket * const socket );

public slots:
#if 0
    /** CAPABILITY, RFC 3501 sect 6.1.1 */
    CommandHandle capability();
#endif
    /** NOOP, RFC 3501 sect 6.1.2 */
    CommandHandle noop();
#if 0
    /** LOGOUT, RFC3501 sect 6.1.3 */
    CommandHandle logout();

    /** STARTTLS, RFC3501 sect 6.2.1 */
    CommandHandle startTls();
    /** AUTHENTICATE, RFC3501 sect 6.2.2 */
    CommandHandle authenticate( Authenticator auth );
    /** LOGIN, RFC3501 sect 6.2.3 */
    CommandHandle login( const QString& username, const QString& password );

    /** SELECT, RFC3501 sect 6.3.1 */
    CommandHandle select( const QString& mailbox );
    /** EXAMINE, RFC3501 sect 6.3.2 */
    CommandHandle examine( const QString& mailbox );
    /** CREATE, RFC3501 sect 6.3.3 */
    CommandHandle create( const QString& mailbox );
    /** DELETE, RFC3501 sect 6.3.4 */
    CommandHandle deleteMbox( const QString& mailbox );
    /** RENAME, RFC3501 sect 6.3.5
     *
     * Takes oldName and newName to remove from -> to. */
    CommandHandle rename( const QString& oldName, const QString& newName );
    /** SUBSCRIBE, RFC3501 sect 6.3.6 */
    CommandHandle subscribe( const QString& mailbox );
    /** UNSUBSCRIBE, RFC3501 sect 6.3.7 */
    CommandHandle unsubscribe( const QString& mailbox );
    /** LIST, RFC3501 sect 6.3.8 */
    CommandHandle list( const QString& reference, const QString& name );
    /** LSUB, RFC3501 sect 6.3.9 */
    CommandHandle lSub( const QString& reference, const QString& name );
    /** STATUS, RFC3501 sect 6.3.10 */
    CommandHandle status( const QString& mailbox, const QList<Item>& items );
    /** APPEND, RFC3501 sect 6.3.11 */
    CommandHandle append( const QString& mailbox, const Message& message, const QSet<Flag>& flags, const QDateTime& stamp );

    /** CHECK, RFC3501 sect 6.4.1 */
    CommandHandle check();
    /** CLOSE, RFC3501 sect 6.4.2 */
    CommandHandle close();
    /** EXPUNGE, RFC3501 sect 6.4.3 */
    CommandHandle expunge();
    /** SEARCH, RFC3501 sect 6.4.4 */
    CommandHandle search( const QStringList& criteria, const QString& charset );
    /** FETCH, RFC3501 sect 6.4.5 */
    CommandHandle fetch( const Sequence& seq, const QList<Item>& items );
    /** STORE, RFC3501 sect 6.4.6 */
    CommandHandle store( const Sequence& seq, const QString& item, const QString& value );
    /** COPY, RFC3501 sect 6.4.7 */
    CommandHandle copy( const Sequence& seq, const QString& mailbox );
    /** UID command (FETCH), RFC3501 sect 6.4.8 */
    CommandHandle uidFetch( const Sequence& seq, const QList<Item>& items );
    /** UID command (SEARCH), RFC3501 sect 6.4.8 */
    CommandHandle uidSearch( const QStringList& criteria, const QString& charset );

    /** X<atom>, RFC3501 sect 6.5.1 */
    CommandHandle xAtom( const QString& command );

    /** UNSELECT, RFC3691 */
    CommandHandle unSelect();

    /** IDLE, RFC2177 */
    CommandHandle idle();

    /** SORT, draft-ietf-imapext-sort-19, section 3 */
    CommandHandle sort( /*const SortAlgorithm& algo,*/ const QString& charset, const QStringList& criteria );
    /** UID SORT, draft-ietf-imapext-sort-19, section 3 */
    CommandHandle uidSort( /*const SortAlgorithm& algo,*/ const QString charset, const QStringList& criteria );
    /** THREAD, draft-ietf-imapext-sort-19, section 3 */
    CommandHandle thread( const ThreadAlgorithm& algo, const QString charset, const QStringList& criteria );
#endif

signals:
    void responseReceived( Response resp );

private:
    QAbstractSocket * const _socket;

};

}
#endif /* IMAP_PARSER_H */
