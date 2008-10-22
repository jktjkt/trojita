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
#include "Imap/Cache.h"
#include "Imap/Authenticator.h"
#include "Imap/Parser.h"
#include "Imap/SocketFactory.h"

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

class TreeItem;
class TreeItemMailbox;
class TreeItemMsgList;
class TreeItemMessage;
class TreeItemPart;

/** @short A model implementing view of the whole IMAP server */
class Model: public QAbstractItemModel {
    Q_OBJECT
    //Q_PROPERTY( ThreadAlgorithm threadSorting READ threadSorting WRITE setThreadSorting )

    struct Task {
        enum Kind { NONE, LIST, STATUS, SELECT, FETCH };
        Kind kind;
        TreeItem* what;
        Task( const Kind _kind, TreeItem* _what ): kind(_kind), what(_what) {};
        Task(): kind(NONE) {};
    };

    enum RWMode { ReadOnly, ReadWrite };

    /** @short IMAP state of a connection */
    enum ConnectionState {
        CONN_STATE_ESTABLISHED /**< Connection established, no details known yet */,
        CONN_STATE_NOT_AUTH /**< Before we can do anything, we have to authenticate ourselves */,
        CONN_STATE_AUTH /**< We are authenticated, now we must select a mailbox */,
        CONN_STATE_SELECTED /**< Some mailbox is selected */,
        CONN_STATE_LOGOUT /**< We have been logged out */
    };

    struct ParserState {
        ParserPtr parser;
        TreeItemMailbox* mailbox;
        RWMode mode;
        ConnectionState connState;
        TreeItemMailbox* handler;
        QMap<CommandHandle, Task> commandMap;

        ParserState( ParserPtr _parser, TreeItemMailbox* _mailbox, const RWMode _mode, 
                const ConnectionState _connState ): 
            parser(_parser), mailbox(_mailbox), mode(_mode), connState(_connState), handler(0) {};
        ParserState(): mailbox(0), mode(ReadOnly), connState(CONN_STATE_LOGOUT), handler(0) {};
    };

    CachePtr _cache;
    AuthenticatorPtr _authenticator;
    SocketFactoryPtr _socketFactory;
    mutable QMap<Parser*,ParserState> _parsers;
    int _maxParsers;

    QStringList _capabilities;
    bool _capabilitiesFresh;

    mutable TreeItemMailbox* _mailboxes;

    QList<Responses::List> _listResponses;
    QList<Responses::Status> _statusResponses;

    QList<Imap::Responses::NamespaceData> _personalNamespace, _otherUsersNamespace, _sharedNamespace;


public:
    Model( QObject* parent, CachePtr cache, AuthenticatorPtr authenticator,
            SocketFactoryPtr socketFactory );
    ~Model();

    virtual QModelIndex index(int row, int column, const QModelIndex& parent ) const;
    virtual QModelIndex parent(const QModelIndex& index ) const;
    virtual int rowCount(const QModelIndex& index ) const;
    virtual int columnCount(const QModelIndex& index ) const;
    virtual QVariant data(const QModelIndex& index, int role ) const;
    virtual bool hasChildren( const QModelIndex& parent = QModelIndex() ) const;

    void handleState( Imap::ParserPtr ptr, const Imap::Responses::State* const resp );
    void handleCapability( Imap::ParserPtr ptr, const Imap::Responses::Capability* const resp );
    void handleNumberResponse( Imap::ParserPtr ptr, const Imap::Responses::NumberResponse* const resp );
    void handleList( Imap::ParserPtr ptr, const Imap::Responses::List* const resp );
    void handleFlags( Imap::ParserPtr ptr, const Imap::Responses::Flags* const resp );
    void handleSearch( Imap::ParserPtr ptr, const Imap::Responses::Search* const resp );
    void handleStatus( Imap::ParserPtr ptr, const Imap::Responses::Status* const resp );
    void handleFetch( Imap::ParserPtr ptr, const Imap::Responses::Fetch* const resp );
    void handleNamespace( Imap::ParserPtr ptr, const Imap::Responses::Namespace* const resp );

private:
    Model& operator=( const Model& ); // don't implement
    Model( const Model& ); // don't implement



    friend class TreeItemMailbox;
    friend class TreeItemMsgList;
    friend class TreeItemMessage;
    friend class TreeItemPart;

    void _askForChildrenOfMailbox( TreeItem* item ) const;
    void _askForMessagesInMailbox( TreeItem* item ) const;
    void _askForMsgMetadata( TreeItem* item ) const;

    void _finalizeList( const QMap<CommandHandle, Task>::const_iterator command );
    void _finalizeStatus( const QMap<CommandHandle, Task>::const_iterator command );
    void _finalizeSelect( ParserPtr parser, const QMap<CommandHandle, Task>::const_iterator command );
    void _finalizeFetch( ParserPtr parser, const QMap<CommandHandle, Task>::const_iterator command );

    TreeItem* translatePtr( const QModelIndex& index ) const;

    /** @short Returns parser suitable for dealing with some mailbox.
     *
     * This parser might be already working hard in another mailbox; if that is
     * the case, it is asked to switch to the correct one.
     *
     * If allowed by policy, new parser might be created in the background.
     * */
    ParserPtr _getParser( TreeItemMailbox* mailbox, const RWMode rw ) const;

protected slots:
    void responseReceived();

};

bool SortMailboxes( const TreeItem* const a, const TreeItem* const b );

}

}

#endif /* IMAP_MODEL_H */
