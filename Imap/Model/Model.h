/* Copyright (C) 2007 - 2008 Jan Kundrát <jkt@gentoo.org>

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

#ifndef IMAP_MODEL_H
#define IMAP_MODEL_H

#include <QAbstractItemModel>
#include <QPointer>
#include <QTimer>
#include "Cache.h"
#include "../Parser/Parser.h"
#include "Streams/SocketFactory.h"

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
    virtual void handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp ) = 0;

protected:
    Model* m;
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

class IdleLauncher: public QObject {
    Q_OBJECT
public:
    IdleLauncher( Model* model, Parser* ptr );
    void restart();
    bool idling();
public slots:
    void perform();
    void idlingTerminated();
private:
    Model* m;
    QPointer<Parser> parser;
    QTimer* timer;
    bool _idling;
};

/** @short A model implementing view of the whole IMAP server */
class Model: public QAbstractItemModel {
    Q_OBJECT
    //Q_PROPERTY( ThreadAlgorithm threadSorting READ threadSorting WRITE setThreadSorting )

    struct Task {
        enum Kind { NONE, STARTTLS, LOGIN, LIST, STATUS, SELECT, FETCH, NOOP,
                    CAPABILITY, STORE, NAMESPACE, EXPUNGE, FETCH_WITH_FLAGS,
                    COPY, CREATE, DELETE, LOGOUT };
        Kind kind;
        TreeItem* what;
        Task( const Kind _kind, TreeItem* _what ): kind(_kind), what(_what) {}
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

    /** @short IMAP state of a connection */
    enum ConnectionState {
        CONN_STATE_ESTABLISHED /**< Connection established */,
        CONN_STATE_AUTH /**< We are authenticated, now we must select a mailbox */,
        CONN_STATE_SYNC /**< We are synchronizing a mailbox */,
        CONN_STATE_SELECTED /**< Some mailbox is selected */,
        CONN_STATE_LOGOUT /**< We have been logged out */
    };

    struct ParserState {
        QPointer<Parser> parser;
        TreeItemMailbox* mailbox;
        RWMode mode;
        ConnectionState connState;
        TreeItemMailbox* currentMbox;
        uint selectingAnother;
        QMap<CommandHandle, Task> commandMap;
        QStringList capabilities;
        bool capabilitiesFresh;
        QList<Responses::List> listResponses;
        SyncState syncState;
        ModelStateHandler* responseHandler;
        QList<uint> uidMap;
        QMap<uint, QStringList> syncingFlags;
        IdleLauncher* idleLauncher;

        ParserState( Parser* _parser, TreeItemMailbox* _mailbox, const RWMode _mode,
                const ConnectionState _connState, ModelStateHandler* _respHandler ):
            parser(_parser), mailbox(_mailbox), mode(_mode),
            connState(_connState), currentMbox(0), selectingAnother(0),
            capabilitiesFresh(false), responseHandler(_respHandler), idleLauncher(0) {}
        ParserState(): mailbox(0), mode(ReadOnly), connState(CONN_STATE_LOGOUT),
            currentMbox(0), selectingAnother(0), capabilitiesFresh(false),
            responseHandler(0), idleLauncher(0) {}
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

    mutable CachePtr _cache;
    mutable SocketFactoryPtr _socketFactory;
    mutable QMap<Parser*,ParserState> _parsers;
    int _maxParsers;
    mutable TreeItemMailbox* _mailboxes;
    mutable NetworkPolicy _netPolicy;
    bool _startTls;
    QTimer* noopTimer;

    mutable QList<Imap::Responses::NamespaceData> _personalNamespace, _otherUsersNamespace, _sharedNamespace;


public:
    Model( QObject* parent, CachePtr cache, SocketFactoryPtr socketFactory, bool offline );
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

    CachePtr cache() const { return _cache; }

    /** @short Force a SELECT / EXAMINE of a mailbox

This command sends a SELECT or EXAMINE command to the remote server, even if the
requested mailbox is currently selected. This has a side effect that we synchronize
the list of messages, which is why this function exists in the first place.
*/
    void resyncMailbox( TreeItemMailbox* mbox );

    /** @short Ask the server to set/unset the \\Deleted flag for a particular message */
    void markMessageDeleted( TreeItemMessage* msg, bool marked );
    /** @short Ask the server to set/unset the \\Seen flag for a particular message */
    void markMessageRead( TreeItemMessage* msg, bool marked );

    /** @short Run the EXPUNGE command in the specified mailbox */
    void expungeMailbox( TreeItemMailbox* mbox );

    /** @short Mark multiple messages \\Deleted at once

      This is useful mainly for drag & drop operations
*/
    void markUidsDeleted( TreeItemMailbox* mbox, const Sequence& messages );
    void copyMessages( TreeItemMailbox* sourceMbox, const QString& destMboxName, const Sequence& seq );
    void createMailbox( const QString& name );
    void deleteMailbox( const QString& name );

    bool isNetworkAvailable() const { return _netPolicy != NETWORK_OFFLINE; }
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
    void reloadMailboxList();

    void setNetworkOffline() { setNetworkPolicy( NETWORK_OFFLINE ); }
    void setNetworkExpensive() { setNetworkPolicy( NETWORK_EXPENSIVE ); }
    void setNetworkOnline() { setNetworkPolicy( NETWORK_ONLINE ); }

    void switchToMailbox( const QModelIndex& mbox );

private slots:
    void slotParserDisconnected( const QString );
    void performNoop();
    void idleTerminated();

signals:
    void alertReceived( const QString& message );
    void networkPolicyOffline();
    void networkPolicyExpensive();
    void networkPolicyOnline();
    void connectionError( const QString& message );
    void authRequested( QAuthenticator* auth );

    void messageCountPossiblyChanged( const QModelIndex& mailbox );

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

    void _askForChildrenOfMailbox( TreeItemMailbox* item );
    void _askForMessagesInMailbox( TreeItemMsgList* item );
    void _askForNumberOfMessages( TreeItemMsgList* item );
    void _askForMsgMetadata( TreeItemMessage* item );
    void _askForMsgPart( TreeItemPart* item, bool onlyFromCache=false );

    void _finalizeList( Parser* parser, const QMap<CommandHandle, Task>::const_iterator command );
    void _finalizeSelect( Parser* parser, const QMap<CommandHandle, Task>::const_iterator command );
    void _finalizeFetch( Parser* parser, const QMap<CommandHandle, Task>::const_iterator command );

    void replaceChildMailboxes( TreeItemMailbox* mailboxPtr, const QList<TreeItem*> mailboxes );
    void enterIdle( Parser* parser );
    void updateCapabilities( Parser* parser, const QStringList capabilities );
    void updateFlags( TreeItemMessage* message, const QString& flagOperation, const QString& flags );

    TreeItem* translatePtr( const QModelIndex& index ) const;

    void emitMessageCountChanged( TreeItemMailbox* const mailbox );

    TreeItemMailbox* findMailboxByName( const QString& name ) const;
    TreeItemMailbox* findMailboxByName( const QString& name, const TreeItemMailbox* const root ) const;

    void saveUidMap( TreeItemMsgList* list );
    void _fullMboxSync( TreeItemMailbox* mailbox, TreeItemMsgList* list, Parser* parser, const SyncState& syncState );

    /** @short Returns parser suitable for dealing with some mailbox.
     *
     * This parser might be already working hard in another mailbox; if that is
     * the case, it is asked to switch to the correct one.
     *
     * If allowed by policy, new parser might be created in the background.
     * */
    Parser* _getParser( TreeItemMailbox* mailbox, const RWMode rw, const bool reSync=false ) const;

    NetworkPolicy networkPolicy() const { return _netPolicy; }
    void setNetworkPolicy( const NetworkPolicy policy );

    void completelyReset();

    ModelStateHandler* unauthHandler;
    ModelStateHandler* authenticatedHandler;
    ModelStateHandler* selectedHandler;
    ModelStateHandler* selectingHandler;

    QStringList _onlineMessageFetch;

protected slots:
    void responseReceived();

};

}

}

#endif /* IMAP_MODEL_H */
