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

#include "ThreadingMsgListModel.h"
#include <QDebug>
#include "ItemRoles.h"
#include "MailboxTree.h"
#include "MsgListModel.h"

namespace Imap {
namespace Mailbox {

ThreadingMsgListModel::ThreadingMsgListModel( QObject* parent ): QAbstractProxyModel(parent)
{
}

void ThreadingMsgListModel::setSourceModel( QAbstractItemModel *sourceModel )
{
    _threading.clear();
    reset();
    Imap::Mailbox::MsgListModel *msgList = qobject_cast<Imap::Mailbox::MsgListModel*>( sourceModel );
    QAbstractProxyModel::setSourceModel( msgList );
    if ( ! msgList )
        return;

    // FIXME: will need to be expanded when Model supports more signals...
    connect( sourceModel, SIGNAL( modelReset() ), this, SLOT( resetMe() ) );
    connect( sourceModel, SIGNAL( layoutAboutToBeChanged() ), this, SIGNAL( layoutAboutToBeChanged() ) );
    connect( sourceModel, SIGNAL( layoutChanged() ), this, SIGNAL( layoutChanged() ) );
    connect( sourceModel, SIGNAL( dataChanged( const QModelIndex&, const QModelIndex& ) ),
            this, SLOT( handleDataChanged( const QModelIndex&, const QModelIndex& ) ) );
    connect( sourceModel, SIGNAL( rowsAboutToBeRemoved( const QModelIndex&, int, int ) ),
             this, SLOT( handleRowsAboutToBeRemoved(const QModelIndex&, int,int ) ) );
    connect( sourceModel, SIGNAL( rowsRemoved( const QModelIndex&, int, int ) ),
             this, SLOT( handleRowsRemoved(const QModelIndex&, int,int ) ) );
    connect( sourceModel, SIGNAL( rowsAboutToBeInserted( const QModelIndex&, int, int ) ),
             this, SLOT( handleRowsAboutToBeInserted(const QModelIndex&, int,int ) ) );
    connect( sourceModel, SIGNAL( rowsInserted( const QModelIndex&, int, int ) ),
             this, SLOT( handleRowsInserted(const QModelIndex&, int,int ) ) );

}

void ThreadingMsgListModel::handleDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    QModelIndex first = mapFromSource( topLeft );
    QModelIndex second = mapFromSource( bottomRight );

    Q_ASSERT(first.isValid());
    Q_ASSERT(second.isValid());

    if ( first.row() != second.row() ) {
        // FIXME: Batched updates...
        Q_ASSERT(false);
        return;
    }

    emit dataChanged( first, second );
}

QModelIndex ThreadingMsgListModel::index( int row, int column, const QModelIndex& parent ) const
{
    if ( parent.isValid() && parent.model() != this ) {
        // foreign model
        return QModelIndex();
    }

    if ( _threading.isEmpty() ) {
        // mapping not available yet
        return QModelIndex();
    }

    if ( row < 0 || column < 0 || column >= MsgListModel::COLUMN_COUNT )
        return QModelIndex();

    if ( parent.isValid() && parent.column() != 0 ) {
        // only the first column should have children
        return QModelIndex();
    }

    uint parentId = parent.isValid() ? parent.internalId() : 0;

    QHash<uint,ThreadNodeInfo>::const_iterator it = _threading.constFind( parentId );
    Q_ASSERT(it != _threading.constEnd());

    if ( it->children.size() <= row )
        return QModelIndex();

    return createIndex( row, column, it->children[row] );
}

QModelIndex ThreadingMsgListModel::parent( const QModelIndex& index ) const
{
    if ( ! index.isValid() || index.model() != this )
        return QModelIndex();

    if ( _threading.isEmpty() )
        return QModelIndex();

    if ( index.row() < 0 || index.column() < 0 || index.column() >= MsgListModel::COLUMN_COUNT )
        return QModelIndex();

    QHash<uint,ThreadNodeInfo>::const_iterator node = _threading.constFind( index.internalId() );
    if ( node == _threading.constEnd() )
        return QModelIndex();

    QHash<uint,ThreadNodeInfo>::const_iterator parentNode = _threading.constFind( node->parent );
    Q_ASSERT(parentNode != _threading.constEnd());
    Q_ASSERT(parentNode->uid == node->parent);

    if ( parentNode->uid == 0 )
        return QModelIndex();

    QHash<uint,ThreadNodeInfo>::const_iterator grantParentNode = _threading.constFind( parentNode->parent );
    Q_ASSERT(grantParentNode != _threading.constEnd());
    Q_ASSERT(grantParentNode->uid == parentNode->parent);

    return createIndex( grantParentNode->children.indexOf( parentNode->uid ), 0, parentNode->uid );
}

bool ThreadingMsgListModel::hasChildren( const QModelIndex& parent ) const
{
    if ( parent.isValid() && parent.column() != 0 )
        return false;

    return ! _threading.isEmpty() && ! _threading.value( parent.internalId() ).children.isEmpty();
}

int ThreadingMsgListModel::rowCount( const QModelIndex& parent ) const
{
    if ( _threading.isEmpty() )
        return 0;

    if ( parent.isValid() && parent.column() != 0 )
        return 0;

    return _threading.value( parent.internalId() ).children.size();
}

int ThreadingMsgListModel::columnCount( const QModelIndex& parent ) const
{
    if ( parent.isValid() && parent.column() != 0 )
        return 0;

    return MsgListModel::COLUMN_COUNT;
}

QModelIndex ThreadingMsgListModel::mapToSource( const QModelIndex& proxyIndex ) const
{
    if ( !proxyIndex.isValid() || !proxyIndex.internalId() )
        return QModelIndex();

    if ( _threading.isEmpty() )
        return QModelIndex();

    Imap::Mailbox::MsgListModel *msgList = qobject_cast<Imap::Mailbox::MsgListModel*>( sourceModel() );
    Q_ASSERT(msgList);

    QHash<uint,ThreadNodeInfo>::const_iterator node = _threading.constFind( proxyIndex.internalId() );
    if ( node == _threading.constEnd() )
        return QModelIndex();

    return msgList->createIndex( node->ptr->row(), proxyIndex.column(), node->ptr );
}

QModelIndex ThreadingMsgListModel::mapFromSource( const QModelIndex& sourceIndex ) const
{
    if ( sourceIndex.model() != sourceModel() )
        return QModelIndex();

    const uint uid = sourceIndex.data( RoleMessageUid ).toUInt();
    if ( uid == 0 )
        return QModelIndex();

    QHash<uint,ThreadNodeInfo>::const_iterator node = _threading.constFind( uid );
    Q_ASSERT(node != _threading.constEnd());

    QHash<uint,ThreadNodeInfo>::const_iterator parentNode = _threading.constFind( node->parent );
    Q_ASSERT(parentNode != _threading.constEnd());
    int offset = parentNode->children.indexOf( uid );
    Q_ASSERT( offset != -1 );

    return createIndex( offset, sourceIndex.column(), uid );
}

void ThreadingMsgListModel::handleRowsAboutToBeRemoved( const QModelIndex& parent, int start, int end )
{
    // FIXME: remove from our mapping...
}

void ThreadingMsgListModel::handleRowsRemoved( const QModelIndex& parent, int start, int end )
{
    resetMe();
    // FIXME
}

void ThreadingMsgListModel::handleRowsAboutToBeInserted( const QModelIndex& parent, int start, int end )
{
    // FIXME: queue them for later...
}

void ThreadingMsgListModel::handleRowsInserted( const QModelIndex& parent, int start, int end )
{
    resetMe();
    // FIXME...
}

void ThreadingMsgListModel::resetMe()
{
    reset();
    _threading.clear();
    updateNoThreading();
    QTimer::singleShot( 1000, this, SLOT(updateFakeThreading()) );
}

void ThreadingMsgListModel::updateNoThreading()
{
    bool containedSomething = false;
    if ( ! _threading.isEmpty() ) {
        beginRemoveRows( QModelIndex(), 0, rowCount() );
        _threading.clear();
        endRemoveRows();
        containedSomething = true;
    }

    int upstreamMessages = sourceModel()->rowCount();
    if ( upstreamMessages )
        beginInsertRows( QModelIndex(), 0, upstreamMessages - 1 );
    QList<uint> allUids;
    for ( int i = 0; i < upstreamMessages; ++i ) {
        QModelIndex index = sourceModel()->index( i, 0 );
        uint uid = index.data( RoleMessageUid ).toUInt();
        Q_ASSERT(uid);
        ThreadNodeInfo node;
        node.uid = uid;
        node.ptr = static_cast<TreeItem*>( index.internalPointer() );
        _threading[ uid ] = node;
        allUids.append( uid );
    }
    _threading[ 0 ].children = allUids;
    _threading[ 0 ].ptr = static_cast<MsgListModel*>( sourceModel() )->msgList;

    if ( upstreamMessages )
        emit endInsertRows();;
}

void ThreadingMsgListModel::updateFakeThreading()
{
    int count = sourceModel()->rowCount();
    if ( count != rowCount() ) {
        qDebug() << "Already threaded, huh?";
        return;
    }

    emit layoutAboutToBeChanged();
    _threading.clear();
    _threading[ 0 ].ptr = static_cast<MsgListModel*>( sourceModel() )->msgList;
    uint lastUid = 0;
    if ( count ) {
        for ( int i = 0; i < count; ++i ) {
            QModelIndex index = sourceModel()->index( i, 0 );
            uint uid = index.data( RoleMessageUid ).toUInt();
            Q_ASSERT(uid);
            ThreadNodeInfo node;
            node.uid = uid;
            node.parent = lastUid;
            Q_ASSERT(_threading.contains( lastUid ));
            _threading[ lastUid ].children.append( uid );
            lastUid = uid;
            node.ptr = static_cast<TreeItem*>( index.internalPointer() );
            _threading[ uid ] = node;
        }
    }
    QList<QModelIndex> updatedIndexes;
    Q_FOREACH( const QModelIndex &oldIndex, persistentIndexList() ) {
        QHash<uint,ThreadNodeInfo>::const_iterator it = _threading.constFind( oldIndex.internalId() );
        if ( it == _threading.constEnd() ) {
            updatedIndexes.append( QModelIndex() );
        } else {
            QHash<uint,ThreadNodeInfo>::const_iterator parentNode = _threading.constFind( it->parent );
            Q_ASSERT(parentNode != _threading.constEnd());
            int offset = parentNode->children.indexOf(it->uid);
            Q_ASSERT(offset != -1);
            updatedIndexes.append( createIndex( offset, oldIndex.column(), it->uid ) );
        }
    }
    changePersistentIndexList( persistentIndexList(), updatedIndexes );
    emit layoutChanged();
}

QDebug operator<<(QDebug debug, const ThreadNodeInfo &node)
{
    debug << "ThreadNodeInfo(" << node.uid << node.ptr<< node.parent << node.children << ")";
    return debug;
}

}
}
