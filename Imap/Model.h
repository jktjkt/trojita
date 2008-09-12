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

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

/** @short IMAP state of a connection */
enum ConnectionState {
    CONN_STATE_ESTABLISHED /**< Connection established, no details known yet */,
    CONN_STATE_NOT_AUTH /**< Before we can do anything, we have to authenticate ourselves */,
    CONN_STATE_AUTH /**< We are authenticated, now we must select a mailbox */,
    CONN_STATE_SELECTING /**< The SELECT/EXAMINE command has been issued, but not yet completed */,
    CONN_STATE_SELECTED /**< Some mailbox is selected */,
    CONN_STATE_LOGOUT /**< We have been logged out */
};

class TreeItem;
class TreeItemMailbox;
class TreeItemMessage;
class TreeItemPart;

/** @short A model implementing view of the whole IMAP server */
class Model: public QAbstractItemModel {
    Q_OBJECT
    //Q_PROPERTY( ThreadAlgorithm threadSorting READ threadSorting WRITE setThreadSorting )

    struct Task {
        enum Kind { NONE, LIST };
        Kind kind;
        TreeItem* what;
        Task( const Kind _kind, TreeItem* _what ): kind(_kind), what(_what) {};
        Task(): kind(NONE) {};
    };

    CachePtr _cache;
    AuthenticatorPtr _authenticator;
    ParserPtr _parser;
    ConnectionState _state;

    QStringList _capabilities;
    bool _capabilitiesFresh;

    mutable TreeItemMailbox* _mailboxes;
    mutable QMap<CommandHandle, Task> _commandMap;

    QList<Responses::List> _listResponses;

    QList<Imap::Responses::NamespaceData> _personalNamespace, _otherUsersNamespace, _sharedNamespace;


public:
    Model( QObject* parent, CachePtr cache, AuthenticatorPtr authenticator,
            ParserPtr parser );

    virtual QModelIndex index(int row, int column, const QModelIndex& parent ) const;
    virtual QModelIndex parent(const QModelIndex& index ) const;
    virtual int rowCount(const QModelIndex& index ) const;
    virtual int columnCount(const QModelIndex& index ) const;
    virtual QVariant data(const QModelIndex& index, int role ) const;

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
    void handleStateInitial( const Imap::Responses::State* const state );
    void handleStateAuthenticated( const Imap::Responses::State* const state );
    void handleStateSelecting( const Imap::Responses::State* const state );
    void handleStateSelected( const Imap::Responses::State* const state );

    void _updateState( const ConnectionState state );


    friend class TreeItemMailbox;
    friend class TreeItemMessage;
    friend class TreeItemPart;

    void _askForChildrenOfMailbox( TreeItem* item ) const;

    void _finalizeList( const QMap<CommandHandle, Task>::const_iterator command );

    TreeItem* translatePtr( const QModelIndex& index ) const;

protected slots:
    void responseReceived();

};

bool SortMailboxes( const TreeItem* const a, const TreeItem* const b );

}

}

#endif /* IMAP_MODEL_H */
