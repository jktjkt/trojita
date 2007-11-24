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
#include <deque>
#include <memory>
#include <tr1/memory>
#include <QObject>
#include <QMutex>
#include <QThread>
#include <QSemaphore>
#include <QIODevice>
#include <QPair>
#include <Imap/Response.h>
#include <Imap/Command.h>
#include <Imap/Exceptions.h>

/**
 * @file
 * A header file defining Parser class and various helpers.
 *
 * @author Jan Kundrát <jkt@gentoo.org>
 */

class ImapParserParseTest;
class QDateTime;
template<class T> class QList;

/** @short Namespace for IMAP interaction */
namespace Imap {

    /** @short Flag as defined in RFC3501, section 2.3.2, FIXME */
    class Flag;

    /** @short Threading algorithm for THREAD command */
    enum ThreadAlgorithm {
        THREAD_ORDEREDSUBJECT /**< ORDEREDSUBJECT algorithm */,
        THREAD_REFERENCES /**< REFERENCES algorithm */
    };

    class Authenticator {};
    class Message;

    /** @short Class specifying a set of messagess to access */
    class Sequence {
    public:
        /** Converts sequence to string suitable for sending over the wire */
        QString toString() const {return "";};
    };

    /** @short A handle identifying a command sent to the server */
    typedef QString CommandHandle;

    class Parser; // will be defined later

    /** @short Helper thread for Parser that deals with actual I/O */
    class WorkerThread : public QThread {
        Q_OBJECT

        virtual void run();

        /** Prevent copying */
        WorkerThread( const WorkerThread& );
        /** Prevent copying */
        WorkerThread& operator=( const WorkerThread& );

        /** Reference to our parser */
        Parser* _parser;

    public:
        WorkerThread( Parser * const parser ) : _parser( parser ) {};
    };

    /** @short Class that does all IMAP parsing */
    class Parser : public QObject {
        Q_OBJECT

        friend class WorkerThread;
        friend class ImapParserParseTest;

    public:
        /** @short Constructor.
         *
         * Takes an QIODevice instance as a parameter. */
        Parser( QObject* parent, std::auto_ptr<QIODevice> socket );

        /** @short Destructor */
        ~Parser();

    public slots:

        /** @short CAPABILITY, RFC 3501 section 6.1.1 */
        CommandHandle capability();

        /** @short NOOP, RFC 3501 section 6.1.2 */
        CommandHandle noop();

        /** @short LOGOUT, RFC3501 section 6.1.3 */
        CommandHandle logout();


        /** @short STARTTLS, RFC3051 section 6.2.1 */
        CommandHandle startTls();

        /** @short AUTHENTICATE, RFC3501 section 6.2.2 */
        CommandHandle authenticate( /* FIXME: parameter */ );

        /** @short LOGIN, RFC3501 section 6.2.3 */
        CommandHandle login( const QString& user, const QString& pass );


        /** @short SELECT, RFC3501 section 6.3.1 */
        CommandHandle select( const QString& mailbox );

        /** @short EXAMINE, RFC3501 section 6.3.2 */
        CommandHandle examine( const QString& mailbox );

        /** @short CREATE, RFC3501 section 6.3.3 */
        CommandHandle create( const QString& mailbox );

        /** @short DELETE, RFC3501 section 6.3.4 */
        CommandHandle deleteMailbox( const QString& mailbox );

        /** @short RENAME, RFC3501 section 6.3.5 */
        CommandHandle rename( const QString& oldName, const QString& newName );

        /** @short SUBSCRIBE, RFC3501 section 6.3.6 */
        CommandHandle subscribe( const QString& mailbox );
        
        /** @short UNSUBSCRIBE, RFC3501 section 6.3.7 */
        CommandHandle unSubscribe( const QString& mailbox );

        /** @short LIST, RFC3501 section 6.3.8 */
        CommandHandle list( const QString& reference, const QString& mailbox );
        
        /** @short LSUB, RFC3501 section 6.3.9 */
        CommandHandle lSub( const QString& reference, const QString& mailbox );

        /** @short STATUS, RFC3501 section 6.3.10 */
        CommandHandle status( const QString& mailbox, const QStringList& fields );
        
        /** @short APPEND, RFC3501 section 6.3.11 */
        CommandHandle append( const QString& mailbox, const QString& message,
                const QStringList& flags = QStringList(), const QDateTime& timestamp = QDateTime() );


        /** @short CHECK, RFC3501 sect 6.4.1 */
        CommandHandle check();

        /** @short CLOSE, RFC3501 sect 6.4.2 */
        CommandHandle close();
        
        /** @short EXPUNGE, RFC3501 sect 6.4.3 */
        CommandHandle expunge();

        /** @short SEARCH, RFC3501 sect 6.4.4 */
        CommandHandle search( const QStringList& criteria, const QString& charset = QString::null ) {
            return _searchHelper( "SEARCH", criteria, charset );
        };

        /** @short FETCH, RFC3501 sect 6.4.5 */
        CommandHandle fetch( const Sequence& seq, const QStringList& items );

        /** @short STORE, RFC3501 sect 6.4.6 */
        CommandHandle store( const Sequence& seq, const QString& item, const QString& value );

        /** @short COPY, RFC3501 sect 6.4.7 */
        CommandHandle copy( const Sequence& seq, const QString& mailbox );

        /** @short UID command (FETCH), RFC3501 sect 6.4.8 */
        CommandHandle uidFetch( const Sequence& seq, const QStringList& items );

        /** @short UID command (SEARCH), RFC3501 sect 6.4.8 */
        CommandHandle uidSearch( const QStringList& criteria, const QString& charset ) {
            return _searchHelper( "UID SEARCH", criteria, charset );
        };


        /** @short X<atom>, RFC3501 sect 6.5.1 */
        CommandHandle xAtom( const Commands::Command& commands );


        /** @short UNSELECT, RFC3691 */
        CommandHandle unSelect();
        
        /** @short IDLE, RFC2177 */
        CommandHandle idle();


#if 0
        /** SORT, draft-ietf-imapext-sort-19, section 3 */
        CommandHandle sort( /*const SortAlgorithm& algo,*/ const QString& charset, const QStringList& criteria );
        /** UID SORT, draft-ietf-imapext-sort-19, section 3 */
        CommandHandle uidSort( /*const SortAlgorithm& algo,*/ const QString charset, const QStringList& criteria );
        /** THREAD, draft-ietf-imapext-sort-19, section 3 */
        CommandHandle thread( const ThreadAlgorithm& algo, const QString charset, const QStringList& criteria );
#endif

    private slots:

        /** @short Socket told us that we can read data */
        void socketReadyRead();

        /** @short Socket got disconnected */
        void socketDisconected();

    signals:
        /** @short Socket got disconnected */
        void disconnected();

        /** @short New response received */
        void responseReceived();

    private:
        /** @short Private copy constructor */
        Parser( const Parser& );
        /** @short Private assignment operator */
        Parser& operator=( const Parser& );

        /** @short Queue command for execution.*/
        CommandHandle queueCommand( Commands::Command command );

        /** @short Shortcut function; works exactly same as above mentioned queueCommand() */
        CommandHandle queueCommand( const Commands::TokenType kind, const QString& text ) {
            return queueCommand( Commands::Command() << Commands::PartOfCommand( kind, text ) );
        };

        /** @short Helper for search() and uidSearch() */
        CommandHandle _searchHelper( const QString& command, const QStringList& criteria,
                const QString& charset = QString::null );

        /** @short Generate tag for next command */
        QString generateTag();

        /** @short Wait for a command continuation request being sent by the server */
        void waitForContinuationRequest();

        /** @short Execute first queued command, ie. send it to the server */
        bool executeIfPossible();
        /** @short Execute passed command right now */
        bool executeACommand( const Commands::Command& cmd );

        /** @short Process a line from IMAP server */
        void processLine( const QByteArray& line );

        /** @short Parse line for untagged reply */
        Responses::Response parseUntagged( const QByteArray& line );

        /** @short Parse line for tagged reply */
        Responses::Response parseTagged( const QByteArray& line );

        /** @short Concatenate words to one QByteArray */
        QByteArray _concatWords( QList<QByteArray>::const_iterator it, const QList<QByteArray>::const_iterator end );

        /** @short Constructs ResponseCode instance from a pair of iterators.
         *
         * Helper function for parseTagged() and parseUntagged().
         *
         * This function modifies its first argument so it points to the
         * beginning of non-response-code data (which might be 'end').
         *
         * Last argument, "line", is used only when raising exceptions.
         */
        QPair<Responses::Code, QStringList> _parseResponseCode( QList<QByteArray>::const_iterator& begin,
                const QList<QByteArray>::const_iterator& end, const char * const line ) const;

        /** @short Add parsed response to the internal queue, emit notification signal */
        void queueResponse( const Responses::Response& resp );

        /** @short Connection to the IMAP server */
        std::auto_ptr<QIODevice> _socket;

        /** @short Keeps track of the last-used command tag */
        unsigned int _lastTagUsed;

        /** @short Mutex for synchronizing access to our queues */
        QMutex _cmdMutex, _respMutex;

        /** @short Queue storing commands that are about to be executed */
        std::deque<Commands::Command> _cmdQueue;

        /** @short Queue storing parsed replies from the IMAP server */
        std::deque<Responses::Response> _respQueue;

        /** @short Worker thread instance */
        WorkerThread _workerThread;

        /** @short Used for throttling worker thread's activity */
        QSemaphore _workerSemaphore;

        /** @short Ask worker thread to stop ASAP */
        bool _workerStop;

        /** @short Mutex guarding _workerStop */
        QMutex _workerStopMutex;

    };

}
#endif /* IMAP_PARSER_H */
