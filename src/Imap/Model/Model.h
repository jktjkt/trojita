/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef IMAP_MODEL_H
#define IMAP_MODEL_H

#include <QAbstractItemModel>
#include <QPointer>
#include <QTimer>
#include "Cache.h"
#include "../ConnectionState.h"
#include "../Parser/Parser.h"
#include "Streams/SocketFactory.h"
#include "CopyMoveOperation.h"
#include "TaskFactory.h"

class QAuthenticator;

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

class Model;

/** @short Response handler for implementing the State Pattern */
class ModelStateHandler: public QObject {
    Q_OBJECT
public:
    ModelStateHandler( Model* _m );

    virtual void handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp ) = 0;
    virtual void handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp ) = 0;
    virtual void handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp ) = 0;
    virtual void handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp ) = 0;
    virtual void handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp ) = 0;
    virtual void handleSort( Imap::Parser* ptr, const Imap::Responses::Sort* const resp ) = 0;
    virtual void handleThread( Imap::Parser* ptr, const Imap::Responses::Thread* const resp ) = 0;
    virtual void handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp ) = 0;

protected:
    Model* m;
private:
    ModelStateHandler(const ModelStateHandler&); // don't implement
    ModelStateHandler& operator=(const ModelStateHandler&); // don't implement
};

class UnauthenticatedHandler;
class AuthenticatedHandler;
class SyncingHandler;
class SelectedHandler;

class TreeItem;
class TreeItemMailbox;
class TreeItemMsgList;
class TreeItemMessage;
class TreeItemPart;
class MsgListModel;
class MailboxModel;

class IdleLauncher;

class _MailboxListUpdater;
class _NumberOfMessagesUpdater;

class ImapTask;
class KeepMailboxOpenTask;

/** @short A model implementing view of the whole IMAP server */
class Model: public QAbstractItemModel {
    Q_OBJECT
    //Q_PROPERTY( ThreadAlgorithm threadSorting READ threadSorting WRITE setThreadSorting )

    struct Task {
        enum Kind { NONE, STARTTLS, LOGIN, LIST, STATUS, SELECT, FETCH_MESSAGE_METADATA, NOOP,
                    CAPABILITY, STORE, NAMESPACE, EXPUNGE, FETCH_WITH_FLAGS,
                    COPY, CREATE, DELETE, LOGOUT, LIST_AFTER_CREATE,
                    FETCH_PART, IDLE, SEARCH_UIDS, FETCH_FLAGS };
        Kind kind;
        TreeItem* what;
        QString str;
        Task( const Kind _kind, TreeItem* _what ): kind(_kind), what(_what) {}
        Task( const Kind _kind, const QString& _str ): kind(_kind), str(_str) {}
        Task(): kind(NONE) {}
    };

    /** @short How to open a mailbox */
    enum RWMode {
        ReadOnly /**< @short Use EXAMINE or leave it in SELECTed mode*/,
        ReadWrite /**< @short Invoke SELECT if necessarry */
    };

    enum {
        /** @short Don't request message structures etc when the number of messages we'd ask for is greater than this */
        StructureFetchLimit = 100 };

    enum {
        /** @short How many messages before and after to preload when structure of one is being requested */
        StructurePreload = 50 };

    /** @short Helper structure for keeping track of each parser's state */
    struct ParserState {
        /** @short Which parser are we talking about here */
        QPointer<Parser> parser;
        /** @short The mailbox which we'd like to have selected */
        TreeItemMailbox* mailbox;
        RWMode mode;
        ConnectionState connState;
        /** @short Mapping of IMAP tag to the helper structure */
        QMap<CommandHandle, Task> commandMap;
        /** @short List of tasks which are active already, and should therefore receive events */
        QList<ImapTask*> activeTasks;
        /** @short A list of cepabilities, as advertised by the server */
        QStringList capabilities;
        /** @short Is the @arg capabilities usable? */
        bool capabilitiesFresh;
        /** @short LIST responses which were not processed yet */
        QList<Responses::List> listResponses;
        ModelStateHandler* responseHandler;
        QList<uint> uidMap;
        IdleLauncher* idleLauncher;

        ParserState( Parser* _parser, TreeItemMailbox* _mailbox, const RWMode _mode,
                const ConnectionState _connState, ModelStateHandler* _respHandler ):
            parser(_parser), mailbox(_mailbox), mode(_mode), connState(_connState),
            capabilitiesFresh(false), responseHandler(_respHandler), idleLauncher(0) {}
        ParserState(): mailbox(0), mode(ReadOnly), connState(CONN_STATE_NONE),
            capabilitiesFresh(false), responseHandler(0), idleLauncher(0) {}
    };

    /** @short Policy for accessing network */
    enum NetworkPolicy {
        /**< @short No access to the network at all

        All network activity is suspended. If an action requires network access,
        it will either fail or be queued for later. */
        NETWORK_OFFLINE,
        /** @short Connections are possible, but expensive

        Information that is cached is preferred, as long as it is usable.
        Trojita will never miss a mail in this mode, but for example it won't
        check for new mailboxes. */
        NETWORK_EXPENSIVE,
        /** @short Connections have zero cost

        Normal mode of operation. All network activity is assumed to have zero
        cost and Trojita is free to ask network as often as possible. It will
        still use local cache when it makes sense, though. */
        NETWORK_ONLINE
    };

    enum { PollingPeriod = 60000 };

    mutable AbstractCache* _cache;
    mutable SocketFactoryPtr _socketFactory;
    TaskFactoryPtr _taskFactory;
    mutable QMap<Parser*,ParserState> _parsers;
    int _maxParsers;
    mutable TreeItemMailbox* _mailboxes;
    mutable NetworkPolicy _netPolicy;
    bool _startTls;
    QTimer* noopTimer;

    mutable QList<Imap::Responses::NamespaceData> _personalNamespace, _otherUsersNamespace, _sharedNamespace;


public:
    Model( QObject* parent, AbstractCache* cache, SocketFactoryPtr socketFactory, TaskFactoryPtr taskFactory, bool offline );
    ~Model();

    virtual QModelIndex index(int row, int column, const QModelIndex& parent ) const;
    virtual QModelIndex parent(const QModelIndex& index ) const;
    virtual int rowCount(const QModelIndex& index ) const;
    virtual int columnCount(const QModelIndex& index ) const;
    virtual QVariant data(const QModelIndex& index, int role ) const;
    virtual bool hasChildren( const QModelIndex& parent = QModelIndex() ) const;

    void handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp );
    void handleCapability( Imap::Parser* ptr, const Imap::Responses::Capability* const resp );
    void handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp );
    void handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp );
    void handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp );
    void handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp );
    void handleStatus( Imap::Parser* ptr, const Imap::Responses::Status* const resp );
    void handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp );
    void handleNamespace( Imap::Parser* ptr, const Imap::Responses::Namespace* const resp );
    void handleSort( Imap::Parser* ptr, const Imap::Responses::Sort* const resp );
    void handleThread( Imap::Parser* ptr, const Imap::Responses::Thread* const resp );

    AbstractCache* cache() const { return _cache; }
    /** Throw away current cache implementation, replace it with the new one

The old cache is automatically deleted.
*/
    void setCache( AbstractCache* cache );

    /** @short Force a SELECT / EXAMINE of a mailbox

This command sends a SELECT or EXAMINE command to the remote server, even if the
requested mailbox is currently selected. This has a side effect that we synchronize
the list of messages, which is why this function exists in the first place.
*/
    void resyncMailbox( const QModelIndex& mbox );

    /** @short Ask the server to set/unset the \\Deleted flag for a particular message */
    void markMessageDeleted( TreeItemMessage* msg, bool marked );
    /** @short Ask the server to set/unset the \\Seen flag for a particular message */
    void markMessageRead( TreeItemMessage* msg, bool marked );

    /** @short Run the EXPUNGE command in the specified mailbox */
    void expungeMailbox( TreeItemMailbox* mbox );

    /** @short Copy or move a sequence of messages between two mailboxes */
    void copyMoveMessages( TreeItemMailbox* sourceMbox, const QString& destMboxName, QList<uint> uids, const CopyMoveOperation op );

    /** @short Create a new mailbox */
    void createMailbox( const QString& name );
    /** @short Delete an existing mailbox */
    void deleteMailbox( const QString& name );

    /** @short Returns true if we are allowed to access the network */
    bool isNetworkAvailable() const { return _netPolicy != NETWORK_OFFLINE; }
    /** @short Returns true if the network access is cheap */
    bool isNetworkOnline() const { return _netPolicy == NETWORK_ONLINE; }

    /** @short Return a TreeItem* for a specified index

Certain proxy models implement their own indexes. These indexes typically won't
share the internalPointer() with the original model.  Because we use this pointer
quite often, a method is needed to automatically go through the list of all proxy
models and return the appropriate raw pointer.

Upon success, a valid pointer is returned, *whichModel is set to point to the
corresponding Model instance and *translatedIndex contains the index in the real
underlying model. If any of these pointers is NULL, it won't get changed, of course.

Upon failure, this function returns 0 and doesn't touch any of @arg whichModel
and @arg translatedIndex.
*/
    static TreeItem* realTreeItem( QModelIndex index, const Model** whichModel = 0, QModelIndex* translatedIndex = 0 );

public slots:
    /** @short Ask for an updated list of mailboxes on the server */
    void reloadMailboxList();

    /** @short Set the netowrk access policy to "no access allowed" */
    void setNetworkOffline() { setNetworkPolicy( NETWORK_OFFLINE ); }
    /** @short Set the network access policy to "possible, but expensive" */
    void setNetworkExpensive() { setNetworkPolicy( NETWORK_EXPENSIVE ); }
    /** @short Set the network access policy to "it's cheap to use it" */
    void setNetworkOnline() { setNetworkPolicy( NETWORK_ONLINE ); }

    /** @short Try to maintain a connection to the given mailbox

      This function informs the Model that the user is interested in receiving
      updates about the mailbox state, such as about the arrival of new messages.
      The usual response to such a hint is launching the IDLE command.
    */
    void switchToMailbox( const QModelIndex& mbox );

private slots:
    /** @short Handler for the "parser got disconnected" event */
    void slotParserDisconnected( const QString );

    /** @short Parser throwed out an exception */
    void slotParseError( const QString& exceptionClass, const QString& errorMessage, const QByteArray& line, int position );

    /** @short Send a NOOP over all connections

      The main reason for such an action is to ask for updated data for all mailboxes
*/
    void performNoop();
    /** @short An event handler which tries to restart the IDLE command, if possible */
    void idleTerminated();

    /** @short Helper for low-level state change propagation */
    void handleSocketStateChanged(Imap::ConnectionState state);

    /** @short Handler for the Parser::sendingCommand() signal */
    void parserIsSendingCommand( const QString& tag );

    /** @short The parser has received a full line */
    void slotParserLineReceived( const QByteArray& line );

    /** @short The parser has sent a block of data */
    void slotParserLineSent( const QByteArray& line );

signals:
    /** @short This signal is emitted then the server sent us an ALERT response code */
    void alertReceived( const QString& message );
    /** @short The network went offline

      This signal is emitted if the network connection went offline for any reason.
    Common reasons are an explicit user action or a network error.
 */
    void networkPolicyOffline();
    /** @short The network access policy got changed to "expensive" */
    void networkPolicyExpensive();
    /** @short The network is available and cheap again */
    void networkPolicyOnline();
    /** @short A connection error has been encountered */
    void connectionError( const QString& message );
    /** @short The server requests the user to authenticate

      The user is expected to file username and password to the QAuthenticator* object.
*/
    void authRequested( QAuthenticator* auth );

    /** @short The amount of messages in the indicated mailbox might have changed */
    void messageCountPossiblyChanged( const QModelIndex& mailbox );

    /** @short We've succeeded to create the given mailbox */
    void mailboxCreationSucceded( const QString& mailbox );
    /** @short The mailbox creation failed for some reason */
    void mailboxCreationFailed( const QString& mailbox, const QString& message );
    /** @short We've succeeded to delete a mailbox */
    void mailboxDeletionSucceded( const QString& mailbox );
    /** @short Mailbox deletion failed */
    void mailboxDeletionFailed( const QString& mailbox, const QString& message );

    /** @short Inform the GUI about the progress of a connection */
    void connectionStateChanged( QObject* parser, Imap::ConnectionState state ); // got to use fully qualified namespace here

    /** @short An interaction with the remote server is taking place */
    void activityHappening( bool isHappening );

    /** @short The parser has received a full line */
    void logParserLineReceived( uint parser, const QByteArray& line );

    /** @short The parser has sent a block of data */
    void logParserLineSent( uint parser, const QByteArray& line );

    /** @short The parser has encountered a fatal error */
    void logParserFatalError( uint parser, const QString& exceptionClass, const QString& message, const QByteArray& line, int position );

private:
    Model& operator=( const Model& ); // don't implement
    Model( const Model& ); // don't implement


    friend class TreeItem;
    friend class TreeItemMailbox;
    friend class TreeItemMsgList;
    friend class TreeItemMessage;
    friend class TreeItemPart;
    friend class MsgListModel; // needs access to createIndex()
    friend class MailboxModel; // needs access to createIndex()

    friend class UnauthenticatedHandler;
    friend class AuthenticatedHandler;
    friend class SelectedHandler;
    friend class SelectingHandler;

    friend class IdleLauncher;
    friend class _MailboxListUpdater;
    friend class _NumberOfMessagesUpdater;

    friend class ImapTask;
    friend class CreateConnectionTask;
    friend class FetchMsgPartTask;
    friend class UpdateFlagsTask;
    friend class ListChildMailboxesTask;
    friend class NumberOfMessagesTask;
    friend class FetchMsgMetadataTask;
    friend class ExpungeMailboxTask;
    friend class CreateMailboxTask;
    friend class DeleteMailboxTask;
    friend class CopyMoveMessagesTask;
    friend class ObtainSynchronizedMailboxTask;
    friend class KeepMailboxOpenTask;
    friend class OpenConnectionTask;
    friend class GetAnyConnectionTask;
    friend class Fake_ListChildMailboxesTask;
    friend class Fake_OpenConnectionTask;

    friend class TestingTaskFactory; // needs access to _socketFactory

    void _askForChildrenOfMailbox( TreeItemMailbox* item );
    void _askForMessagesInMailbox( TreeItemMsgList* item );
    void _askForNumberOfMessages( TreeItemMsgList* item );
    void _askForMsgMetadata( TreeItemMessage* item );
    void _askForMsgPart( TreeItemPart* item, bool onlyFromCache=false );

    void _finalizeList( Parser* parser, TreeItemMailbox* const mailboxPtr );
    void _finalizeIncrementalList( Parser* parser, const QString& parentMailboxName );
    void _finalizeFetch( Parser* parser, const QMap<CommandHandle, Task>::const_iterator command );
    void _finalizeFetchPart( Parser* parser, TreeItemPart* const part );

    void replaceChildMailboxes( TreeItemMailbox* mailboxPtr, const QList<TreeItem*> mailboxes );
    void enterIdle( Parser* parser );
    void updateCapabilities( Parser* parser, const QStringList capabilities );

    TreeItem* translatePtr( const QModelIndex& index ) const;

    void emitMessageCountChanged( TreeItemMailbox* const mailbox );

    TreeItemMailbox* findMailboxByName( const QString& name ) const;
    TreeItemMailbox* findMailboxByName( const QString& name, const TreeItemMailbox* const root ) const;
    TreeItemMailbox* findParentMailboxByName( const QString& name ) const;

    void saveUidMap( TreeItemMsgList* list );

    /** @short Returns parser suitable for dealing with some mailbox.
     *
     * This parser might be already working hard in another mailbox; if that is
     * the case, it is asked to switch to the correct one.
     *
     * If allowed by policy, new parser might be created in the background.
     * */
    Parser* _getParser( TreeItemMailbox* mailbox, const RWMode rw, const bool reSync=false );

    /** @short Return a corresponding KeepMailboxOpenTask for a given mailbox */
    KeepMailboxOpenTask* findTaskResponsibleFor( const QModelIndex& mailbox );

    NetworkPolicy networkPolicy() const { return _netPolicy; }
    void setNetworkPolicy( const NetworkPolicy policy );

    /** @short Helper function for changing connection state */
    void changeConnectionState( Parser* parser, ConnectionState state );

    /** @short Try to authenticate the user to the IMAP server */
    CommandHandle performAuthentication( Imap::Parser* ptr );

    /** @short Check if all the parsers are indeed idling, and update the GUI if so */
    void parsersMightBeIdling();

    /** @short Dispose of the parser in a C++-safe way */
    void killParser( Parser* parser );

    /** @short Helper for the slotParseError() */
    void broadcastParseError( const uint parser, const QString& exceptionClass, const QString& errorMessage, const QByteArray& line, int position );

    /** @short Remove deleted Tasks from the activeTasks list */
    void removeDeletedTasks( const QList<ImapTask*>& deletedTasks, QList<ImapTask*>& activeTasks );

    ModelStateHandler* unauthHandler;
    ModelStateHandler* authenticatedHandler;
    ModelStateHandler* selectedHandler;
    ModelStateHandler* selectingHandler;

    QStringList _onlineMessageFetch;

    /** @short Helper for keeping track of user/pass over time

    The reason for using QAuthenticator* instead of non-pointer member is that isNull()
    and operator=() does not really work together and that I got a mysterious segfault when
    trying the operator=(). Oh well...
    */
    QAuthenticator* _authenticator;

    uint lastParserId;

protected slots:
    void responseReceived();

    void runReadyTasks();

};

}

}

#endif /* IMAP_MODEL_H */
