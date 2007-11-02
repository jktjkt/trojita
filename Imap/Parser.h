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
#include <memory>
#include <QObject>
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

public:
    /** Constructor. Takes an QAbstractSocket instance as a parameter. */
    Parser( QObject* parent, QAbstractSocket * const socket );

public slots:
#if 0
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


    /** SORT, draft-ietf-imapext-sort-19, section 3 */
    CommandHandle sort( /*const SortAlgorithm& algo,*/ const QString& charset, const QStringList& criteria );
    /** UID SORT, draft-ietf-imapext-sort-19, section 3 */
    CommandHandle uidSort( /*const SortAlgorithm& algo,*/ const QString charset, const QStringList& criteria );
    /** THREAD, draft-ietf-imapext-sort-19, section 3 */
    CommandHandle thread( const ThreadAlgorithm& algo, const QString charset, const QStringList& criteria );
#endif

    /** Queue command for execution.*/
    CommandHandle queueCommand( std::auto_ptr<Imap::Commands::AbstractCommand> command );

signals:
    void responseReceived( Response resp );

private:
    // private copy constructor & assignment operator
    Parser( const Parser& );
    Parser& operator=( const Parser& );

    /** Add command to internal queue and schedule its execution.
     *
     * @param   command a list of strings that are to be sent to the server
     *                  (using apropriate form for each of them)
     */
    CommandHandle queueCommand( const QStringList& command );

    QAbstractSocket* _socket;

};

}
#endif /* IMAP_PARSER_H */
