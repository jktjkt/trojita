/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

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

ThreadingMsgListModel::ThreadingMsgListModel( QObject* parent ): QAbstractProxyModel(parent), modelResetInProgress(false)
{
}

void ThreadingMsgListModel::setSourceModel( QAbstractItemModel *sourceModel )
{
    _threading.clear();
    ptrToInternal.clear();
    unknownUids.clear();
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
    resetMe();
}

void ThreadingMsgListModel::handleDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    if ( topLeft.row() != bottomRight.row() ) {
        // FIXME: Batched updates...
        Q_ASSERT(false);
        return;
    }

    if ( unknownUids.contains(topLeft) ) {
        // The message wasn't fully synced before, and now it is
        unknownUids.removeOne(topLeft);
        qDebug() << "Got UID for" << topLeft.row();
        if ( unknownUids.isEmpty() ) {
            // Let's re-thread, then!
            askForThreading();
        }
        return;
    }

    QModelIndex first = mapFromSource( topLeft );
    QModelIndex second = mapFromSource( bottomRight );

    // There's a possibility for a short race window when te underlying model signals
    // new data for a message but said message doesn't have threading info yet. Therefore,
    // the translated indexes are invalid, so we have to handle that gracefully.
    // "Doing nothing" could be a reasonable thing.
    if ( ! first.isValid() || ! second.isValid() )
        return;

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

    QHash<void*,uint>::const_iterator it = ptrToInternal.constFind( sourceIndex.internalPointer() );
    if ( it == ptrToInternal.constEnd() )
        return QModelIndex();

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

    if ( it->uid ) {
        // It's a real item which exists in the underlying model
        if ( role == RoleThreadRootWithUnreadMessages && ! proxyIndex.parent().isValid() ) {
            return threadContainsUnreadMessages(it->internalId);
        } else {
            return QAbstractProxyModel::data( proxyIndex, role );
        }
    }

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
    // Deal with that only when it's done, so nothing to do here
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
}

void ThreadingMsgListModel::handleRowsRemoved( const QModelIndex& parent, int start, int end )
{
    resetMe();
    // FIXME
}

void ThreadingMsgListModel::handleRowsAboutToBeInserted( const QModelIndex& parent, int start, int end )
{
    // Nothing to do at this point
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
}

void ThreadingMsgListModel::handleRowsInserted( const QModelIndex& parent, int start, int end )
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
    askForThreading();
}

void ThreadingMsgListModel::resetMe()
{
    // Prevent possible recursion here
    if ( modelResetInProgress )
        return;

    modelResetInProgress = true;
    _threading.clear();
    ptrToInternal.clear();
    unknownUids.clear();
    reset();
    updateNoThreading();
    modelResetInProgress = false;

    // If there are any messages, try to thread them
    if ( sourceModel() && rowCount() )
        askForThreading();
}

void ThreadingMsgListModel::updateNoThreading()
{
    if ( ! _threading.isEmpty() ) {
        beginRemoveRows( QModelIndex(), 0, rowCount() - 1 );
        _threading.clear();
        ptrToInternal.clear();
        endRemoveRows();
    }
    unknownUids.clear();

    if ( ! sourceModel() ) {
        // Maybe we got reset because the parent model is no longer here...
        return;
    }

    int upstreamMessages = sourceModel()->rowCount();
    QList<uint> allIds;
    QHash<uint,ThreadNodeInfo> newThreading;
    QHash<void*,uint> newPtrToInternal;

    for ( int i = 0; i < upstreamMessages; ++i ) {
        QModelIndex index = sourceModel()->index( i, 0 );
        uint uid = index.data( RoleMessageUid ).toUInt();
        ThreadNodeInfo node;
        node.internalId = i + 1;
        node.uid = uid;
        node.ptr = static_cast<TreeItem*>( index.internalPointer() );
        if ( node.uid ) {
            newThreading[ node.internalId ] = node;
            allIds.append( node.internalId );
            newPtrToInternal[ node.ptr ] = node.internalId;
        } else {
            qDebug() << "Message" << index.row() << "has unkown UID";
            unknownUids << index;
        }
    }

    if ( newThreading.size() ) {
        beginInsertRows( QModelIndex(), 0, newThreading.size() - 1 );
        _threading = newThreading;
        ptrToInternal = newPtrToInternal;
        _threading[ 0 ].children = allIds;
        _threading[ 0 ].ptr = static_cast<MsgListModel*>( sourceModel() )->msgList;
        endInsertRows();
    }
}

void ThreadingMsgListModel::askForThreading()
{
    if ( ! sourceModel() ) {
        updateNoThreading();
        return;
    }

    if ( ! sourceModel()->rowCount() )
        return;

    const Imap::Mailbox::Model *realModel;
    QModelIndex someMessage = sourceModel()->index(0,0);
    QModelIndex realIndex;
    Imap::Mailbox::Model::realTreeItem( someMessage, &realModel, &realIndex );
    QModelIndex mailboxIndex = realIndex.parent().parent();

    if ( realModel->capabilities().contains( QLatin1String("THREAD=REFS")) ) {
        requestedAlgorithm = QLatin1String("REFS");
    } else if ( realModel->capabilities().contains( QLatin1String("THREAD=REFERENCES") ) ) {
        requestedAlgorithm = QLatin1String("REFERENCES");
    } else if ( realModel->capabilities().contains( QLatin1String("THREAD=ORDEREDSUBJECT") ) ) {
        requestedAlgorithm = QLatin1String("ORDEREDSUBJECT");
    }

    if ( ! requestedAlgorithm.isEmpty() ) {
        realModel->_taskFactory->createThreadTask( const_cast<Imap::Mailbox::Model*>(realModel),
                                                   mailboxIndex, requestedAlgorithm,
                                                   QStringList() << QLatin1String("ALL") );
        connect( realModel, SIGNAL(threadingAvailable(QModelIndex,QString,QStringList,QVector<Imap::Responses::ThreadingNode>)),
                 this, SLOT(slotThreadingAvailable(QModelIndex,QString,QStringList,QVector<Imap::Responses::ThreadingNode>)) );
    }
}

void ThreadingMsgListModel::slotThreadingAvailable( const QModelIndex &mailbox, const QString &algorithm,
                                                    const QStringList &searchCriteria,
                                                    const QVector<Imap::Responses::ThreadingNode> &mapping )
{
    const Imap::Mailbox::Model *realModel;
    QModelIndex someMessage = sourceModel()->index(0,0);
    QModelIndex realIndex;
    Imap::Mailbox::Model::realTreeItem( someMessage, &realModel, &realIndex );
    QModelIndex mailboxIndex = realIndex.parent().parent();
    if ( mailboxIndex != mailbox ) {
        // this is for another mailbox
        return;
    }

    if ( algorithm != requestedAlgorithm ) {
        qDebug() << "Weird, asked for threading via" << requestedAlgorithm << " but got" << algorithm <<
                "instead -- ignoring.";
        return;
    }

    if ( searchCriteria.size() != 1 || searchCriteria.front() != QLatin1String("ALL") ) {
        qDebug() << "Weird, requesting messages matching ALL, but got this instead: " << searchCriteria;
        return;
    }

    disconnect( sender(), 0, this,
                SLOT(slotThreadingAvailable(QModelIndex,QString,QStringList,QVector<Imap::Responses::ThreadingNode>)) );

    if ( ! unknownUids.isEmpty() ) {
        // Some messages have UID zero, which means that they weren't loaded yet. Too bad.
        // FIXME: maybe we could re-use the response...
        qDebug() << unknownUids.size() << "messages have 0 UID";
        return;
    }

    emit layoutAboutToBeChanged();

    updatePersistentIndexesPhase1();

    _threading.clear();
    ptrToInternal.clear();
    _threading[ 0 ].ptr = static_cast<MsgListModel*>( sourceModel() )->msgList;

    int upstreamMessages = sourceModel()->rowCount();
    QHash<uint,void*> uidToPtrCache;
    for ( int i = 0; i < upstreamMessages; ++i ) {
        QModelIndex index = sourceModel()->index( i, 0 );
        ThreadNodeInfo node;
        node.uid = index.data( RoleMessageUid ).toUInt();
        if ( ! node.uid ) {
            throw UnknownMessageIndex("Encountered a message with zero UID when threading. This is a bug in Trojita, sorry.");
        }

        node.internalId = i + 1;
        node.ptr = static_cast<TreeItem*>( index.internalPointer() );
        uidToPtrCache[node.uid] = node.ptr;
        _threadingHelperLastId = node.internalId;
        Q_ASSERT(!_threading.contains( node.internalId ));
        _threading[ node.internalId ] = node;
        ptrToInternal[ node.ptr ] = node.internalId;
    }

    registerThreading( mapping, 0, uidToPtrCache );

    updatePersistentIndexesPhase2();
    emit layoutChanged();
}

void ThreadingMsgListModel::registerThreading( const QVector<Imap::Responses::ThreadingNode> &mapping, uint parentId, const QHash<uint,void*> &uidToPtr )
{
    Q_FOREACH( const Imap::Responses::ThreadingNode &node, mapping ) {
        uint nodeId;
        if ( node.num == 0 ) {
            ThreadNodeInfo fake;
            fake.internalId = ++_threadingHelperLastId;
            fake.parent = parentId;
            Q_ASSERT(_threading.contains( parentId ));
            // The child will be registered to the list of parent's children after the if/else branch
            _threading[ fake.internalId ] = fake;
            nodeId = fake.internalId;
        } else {
            QHash<uint,void*>::const_iterator ptrIt = uidToPtr.find( node.num );
            if ( ptrIt == uidToPtr.constEnd() ) {
                throw UnknownMessageIndex("THREAD response referenced a UID which is not known at this point");
            }

            QHash<void*,uint>::const_iterator nodeIt = ptrToInternal.constFind( *ptrIt );
            Q_ASSERT(nodeIt != ptrToInternal.constEnd()); // FIXME: exception?
            nodeId = *nodeIt;
        }
        _threading[ parentId ].children.append( nodeId );
        _threading[ nodeId ].parent = parentId;
        registerThreading( node.children, nodeId, uidToPtr );
    }
}

void ThreadingMsgListModel::updatePersistentIndexesPhase1()
{
    oldPersistentIndexes = persistentIndexList();
    oldPtrs.clear();
    Q_FOREACH( const QModelIndex &idx, oldPersistentIndexes ) {
        if ( ! idx.isValid() ) {
            oldPtrs << 0;
            continue;
        }
        QModelIndex translated = mapToSource(idx);
        Q_ASSERT(translated.isValid());
        oldPtrs << translated.internalPointer();
    }
}

void ThreadingMsgListModel::updatePersistentIndexesPhase2()
{
    Q_ASSERT(oldPersistentIndexes.size() == oldPtrs.size());
    QList<QModelIndex> updatedIndexes;
    for ( int i = 0; i < oldPersistentIndexes.size(); ++i ) {
        QHash<void*,uint>::const_iterator ptrIt = ptrToInternal.constFind(oldPtrs[i]);
        if ( ptrIt == ptrToInternal.constEnd() ) {
            updatedIndexes.append( QModelIndex() );
            continue;
        }
        QHash<uint,ThreadNodeInfo>::const_iterator it = _threading.constFind(*ptrIt);
        Q_ASSERT(it != _threading.constEnd());
        QHash<uint,ThreadNodeInfo>::const_iterator parentNode = _threading.constFind( it->parent );
        Q_ASSERT(parentNode != _threading.constEnd());
        int offset = parentNode->children.indexOf(it->internalId);
        Q_ASSERT(offset != -1);
        updatedIndexes.append( createIndex( offset, oldPersistentIndexes[i].column(), it->internalId ) );
    }
    changePersistentIndexList( oldPersistentIndexes, updatedIndexes );
    oldPersistentIndexes.clear();
    oldPtrs.clear();
}

QDebug operator<<(QDebug debug, const ThreadNodeInfo &node)
{
    debug << "ThreadNodeInfo(" << node.internalId << node.uid << node.ptr<< node.parent << node.children << ")";
    return debug;
}

QStringList ThreadingMsgListModel::supportedCapabilities()
{
    return QStringList() << QLatin1String("THREAD=REFS") << QLatin1String("THREAD=REFERENCES") << QLatin1String("THREAD=ORDEREDSUBJECT");
}

QStringList ThreadingMsgListModel::mimeTypes() const
{
    return sourceModel() ? sourceModel()->mimeTypes() : QStringList();
}

QMimeData* ThreadingMsgListModel::mimeData( const QModelIndexList& indexes ) const
{
    if ( ! sourceModel() )
        return 0;

    QModelIndexList translated;
    Q_FOREACH(const QModelIndex &idx, indexes) {
        translated << mapToSource(idx);
    }
    return sourceModel()->mimeData(translated);
}

Qt::DropActions ThreadingMsgListModel::supportedDropActions() const
{
    return sourceModel() ? sourceModel()->supportedDropActions() : Qt::DropActions(0);
}

bool ThreadingMsgListModel::threadContainsUnreadMessages(const uint root) const
{
    // FIXME: cache the value somewhere...
    QList<uint> queue;
    queue.append(root);
    while ( ! queue.isEmpty() ) {
        uint current = queue.takeFirst();
        QHash<uint,ThreadNodeInfo>::const_iterator it = _threading.constFind(current);
        Q_ASSERT(it != _threading.constEnd());
        Q_ASSERT(it->ptr);
        TreeItemMessage *message = dynamic_cast<TreeItemMessage*>(it->ptr);
        Q_ASSERT(message);
        if ( ! message->isMarkedAsRead() )
            return true;
        queue.append(it->children);
    }
    return false;
}

}
}
