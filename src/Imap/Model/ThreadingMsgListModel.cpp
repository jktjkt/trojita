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
    uidToInternal.clear();
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
    Q_ASSERT(parentNode->internalId == node->parent);

    if ( parentNode->internalId == 0 )
        return QModelIndex();

    QHash<uint,ThreadNodeInfo>::const_iterator grantParentNode = _threading.constFind( parentNode->parent );
    Q_ASSERT(grantParentNode != _threading.constEnd());
    Q_ASSERT(grantParentNode->internalId == parentNode->parent);

    return createIndex( grantParentNode->children.indexOf( parentNode->internalId ), 0, parentNode->internalId );
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

    if ( node->uid ) {
        return msgList->createIndex( node->ptr->row(), proxyIndex.column(), node->ptr );
    } else {
        // it's a fake message
        return QModelIndex();
    }
}

QModelIndex ThreadingMsgListModel::mapFromSource( const QModelIndex& sourceIndex ) const
{
    if ( sourceIndex.model() != sourceModel() )
        return QModelIndex();

    const uint uid = sourceIndex.data( RoleMessageUid ).toUInt();
    if ( uid == 0 )
        return QModelIndex();

    QHash<uint,uint>::const_iterator it = uidToInternal.constFind( uid );
    Q_ASSERT( it != uidToInternal.constEnd() );
    const uint internalId = *it;

    QHash<uint,ThreadNodeInfo>::const_iterator node = _threading.constFind( internalId );
    Q_ASSERT(node != _threading.constEnd());

    QHash<uint,ThreadNodeInfo>::const_iterator parentNode = _threading.constFind( node->parent );
    Q_ASSERT(parentNode != _threading.constEnd());
    int offset = parentNode->children.indexOf( internalId );
    Q_ASSERT( offset != -1 );

    return createIndex( offset, sourceIndex.column(), internalId );
}

QVariant ThreadingMsgListModel::data( const QModelIndex &proxyIndex, int role ) const
{
    if ( ! proxyIndex.isValid() || proxyIndex.model() != this )
        return QVariant();

    QHash<uint,ThreadNodeInfo>::const_iterator it = _threading.constFind( proxyIndex.internalId() );
    Q_ASSERT(it != _threading.constEnd());
    if ( it->uid )
        return QAbstractProxyModel::data( proxyIndex, role );

    switch( role ) {
    case Qt::DisplayRole:
        if ( proxyIndex.column() == 0 )
            return tr("[Message is missing]");
        break;
    case Qt::ToolTipRole:
        return tr("This thread refers to an extra message, but that message is not present in the "
                  "selected mailbox, or is missing from the current search context.");
    }
    return QVariant();
}

Qt::ItemFlags ThreadingMsgListModel::flags( const QModelIndex &index ) const
{
    if ( ! index.isValid() || index.model() != this )
        return Qt::NoItemFlags;

    QHash<uint,ThreadNodeInfo>::const_iterator it = _threading.constFind( index.internalId() );
    Q_ASSERT(it != _threading.constEnd());
    if ( it->uid )
        return QAbstractProxyModel::flags( index );

    return Qt::NoItemFlags;

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
    uidToInternal.clear();
    updateNoThreading();
    QTimer::singleShot( 1000, this, SLOT(askForThreading()) );
    //QTimer::singleShot( 1000, this, SLOT(updateFakeThreading()) );
}

void ThreadingMsgListModel::updateNoThreading()
{
    bool containedSomething = false;
    if ( ! _threading.isEmpty() ) {
        beginRemoveRows( QModelIndex(), 0, rowCount() );
        _threading.clear();
        uidToInternal.clear();
        endRemoveRows();
        containedSomething = true;
    }

    int upstreamMessages = sourceModel()->rowCount();
    if ( upstreamMessages )
        beginInsertRows( QModelIndex(), 0, upstreamMessages - 1 );
    QList<uint> allIds;
    for ( int i = 0; i < upstreamMessages; ++i ) {
        QModelIndex index = sourceModel()->index( i, 0 );
        uint uid = index.data( RoleMessageUid ).toUInt();
        Q_ASSERT(uid);
        ThreadNodeInfo node;
        node.internalId = i + 1;
        node.uid = uid;
        uidToInternal[ node.uid ] = node.internalId;
        node.ptr = static_cast<TreeItem*>( index.internalPointer() );
        _threading[ node.internalId ] = node;
        allIds.append( node.internalId );
    }
    _threading[ 0 ].children = allIds;
    _threading[ 0 ].ptr = static_cast<MsgListModel*>( sourceModel() )->msgList;

    if ( upstreamMessages )
        emit endInsertRows();;
}

void ThreadingMsgListModel::askForThreading()
{
    int count = sourceModel()->rowCount();
    if ( count != rowCount() ) {
        qDebug() << "Already threaded, huh?";
        return;
    }

    if ( ! count )
        return;

    const Imap::Mailbox::Model *realModel;
    QModelIndex someMessage = sourceModel()->index(0,0);
    QModelIndex realIndex;
    Imap::Mailbox::Model::realTreeItem( someMessage, &realModel, &realIndex );
    QModelIndex mailboxIndex = realIndex.parent().parent();
    realModel->_taskFactory->createThreadTask( const_cast<Imap::Mailbox::Model*>(realModel),
                                               mailboxIndex, QLatin1String("REFS"),
                                               QStringList() << QLatin1String("ALL") );
    connect( realModel, SIGNAL(threadingAvailable(QModelIndex,QString,QStringList,QList<Imap::Responses::Thread::Node>)),
             this, SLOT(slotThreadingAvailable(QModelIndex,QString,QStringList,QList<Imap::Responses::Thread::Node>)) );
}

void ThreadingMsgListModel::slotThreadingAvailable( const QModelIndex &mailbox, const QString &algorithm,
                                                    const QStringList &searchCriteria,
                                                    const QList<Imap::Responses::Thread::Node> &mapping )
{
    int count = sourceModel()->rowCount();
    if ( count != rowCount() ) {
        qDebug() << "Already threaded, huh?";
        return;
    }

    // FIXME: check for correct mailbox, algorithm and search criteria...

    disconnect( sender(), 0, this,
                SLOT(slotThreadingAvailable(QModelIndex,QString,QStringList,QList<Imap::Responses::Thread::Node>)) );


    emit layoutAboutToBeChanged();
    _threading.clear();
    uidToInternal.clear();
    _threading[ 0 ].ptr = static_cast<MsgListModel*>( sourceModel() )->msgList;

    int upstreamMessages = sourceModel()->rowCount();
    for ( int i = 0; i < upstreamMessages; ++i ) {
        QModelIndex index = sourceModel()->index( i, 0 );
        uint uid = index.data( RoleMessageUid ).toUInt();
        uint internalId = i + 1;
        _threadingHelperLastId = internalId;
        uidToInternal[ uid ] = internalId;
        Q_ASSERT(uid);
        Q_ASSERT(!_threading.contains( internalId ));
        //Q_ASSERT(_threading.contains( _threading[ internalId ].parent ));
        //Q_ASSERT(_threading[ _threading[ internalId ].parent ].children.contains( internalId ));
        _threading[ internalId ].ptr = static_cast<TreeItem*>( index.internalPointer() );
        _threading[ internalId ].uid = uid;
    }

    registerThreading( mapping, 0 );
    qDebug() << _threading;

    updatePersistentIndexes();
    emit layoutChanged();
}

void ThreadingMsgListModel::registerThreading( const QList<Imap::Responses::Thread::Node> &mapping, uint parentId )
{
    Q_FOREACH( const Imap::Responses::Thread::Node &node, mapping ) {
        uint nodeId;
        if ( node.num == 0 ) {
            ThreadNodeInfo fake;
            fake.internalId = ++_threadingHelperLastId;
            fake.parent = parentId;
            Q_ASSERT(_threading.contains( parentId ));
            _threading[ parentId ].children.append( fake.internalId );;
            _threading[ fake.internalId ] = fake;
            nodeId = fake.internalId;
        } else {
            QHash<uint,uint>::const_iterator nodeIt = uidToInternal.constFind( node.num );
            Q_ASSERT(nodeIt != uidToInternal.constEnd()); // FIXME: exception?
            nodeId = *nodeIt;
        }
        Q_FOREACH( const Imap::Responses::Thread::Node &child, node.children ) {
            uint childId;
            if ( child.num == 0 ) {
                ThreadNodeInfo fake;
                fake.internalId = ++_threadingHelperLastId;
                fake.parent = nodeId;
                Q_ASSERT(_threading.contains( nodeId ));
                _threading[ nodeId ].children.append( fake.internalId );;
                _threading[ fake.internalId ] = fake;
                childId = fake.internalId;
            } else {
                QHash<uint,uint>::const_iterator childIt = uidToInternal.constFind( child.num );
                Q_ASSERT(childIt != uidToInternal.constEnd()); // FIXME: exception?
                childId = *childIt;
            }
            _threading[ nodeId ].children.append( childId );
            _threading[ childId ].parent = nodeId;
        }
        registerThreading( node.children, nodeId );
    }
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
    uidToInternal.clear();
    _threading[ 0 ].ptr = static_cast<MsgListModel*>( sourceModel() )->msgList;
    uint lastId = 0;
    uint internalIdCounter = 1;
    if ( count ) {
        for ( int i = 0; i < count; ++i, ++internalIdCounter ) {
            QModelIndex index = sourceModel()->index( i, 0 );
            uint uid = index.data( RoleMessageUid ).toUInt();
            Q_ASSERT(uid);
            ThreadNodeInfo node;
            node.internalId = internalIdCounter;
            uidToInternal[ uid ] = node.internalId;
            node.uid = uid;
            node.parent = lastId;
            Q_ASSERT(_threading.contains( lastId ));
            _threading[ lastId ].children.append( node.internalId );
            lastId = node.internalId;
            node.ptr = static_cast<TreeItem*>( index.internalPointer() );
            _threading[ node.internalId ] = node;

            if ( internalIdCounter % 6 == 0 ) {
                ThreadNodeInfo fake;
                fake.internalId = ++internalIdCounter;
                fake.parent = lastId;
                Q_ASSERT(_threading.contains( lastId ));
                _threading[ lastId ].children.append( fake.internalId );;
                lastId = fake.internalId;
                _threading[ fake.internalId ] = fake;
            }
        }
    }
    updatePersistentIndexes();
    emit layoutChanged();
}

void ThreadingMsgListModel::updatePersistentIndexes()
{
    QList<QModelIndex> updatedIndexes;
    Q_FOREACH( const QModelIndex &oldIndex, persistentIndexList() ) {
        QHash<uint,ThreadNodeInfo>::const_iterator it = _threading.constFind( oldIndex.internalId() );
        if ( it == _threading.constEnd() ) {
            updatedIndexes.append( QModelIndex() );
        } else {
            QHash<uint,ThreadNodeInfo>::const_iterator parentNode = _threading.constFind( it->parent );
            Q_ASSERT(parentNode != _threading.constEnd());
            int offset = parentNode->children.indexOf(it->internalId);
            Q_ASSERT(offset != -1);
            updatedIndexes.append( createIndex( offset, oldIndex.column(), it->internalId ) );
        }
    }
    changePersistentIndexList( persistentIndexList(), updatedIndexes );
}

QDebug operator<<(QDebug debug, const ThreadNodeInfo &node)
{
    debug << "ThreadNodeInfo(" << node.internalId << node.uid << node.ptr<< node.parent << node.children << ")";
    return debug;
}

}
}
