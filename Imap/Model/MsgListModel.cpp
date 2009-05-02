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

#include "MsgListModel.h"
#include "MailboxTree.h"
#include "MailboxModel.h"

#include <QApplication>
#include <QDebug>
#include <QFontMetrics>
#include <typeinfo>

namespace Imap {
namespace Mailbox {

MsgListModel::MsgListModel( QObject* parent, Model* model ): QAbstractProxyModel(parent), msgList(0)
{
    setSourceModel( model );

    // FIXME: will need to be expanded when Model supports more signals...
    connect( model, SIGNAL( modelAboutToBeReset() ), this, SLOT( resetMe() ) );
    connect( model, SIGNAL( layoutAboutToBeChanged() ), this, SIGNAL( layoutAboutToBeChanged() ) );
    connect( model, SIGNAL( layoutChanged() ), this, SIGNAL( layoutChanged() ) );
    connect( model, SIGNAL( dataChanged( const QModelIndex&, const QModelIndex& ) ),
            this, SLOT( handleDataChanged( const QModelIndex&, const QModelIndex& ) ) );
    connect( model, SIGNAL( rowsAboutToBeRemoved( const QModelIndex&, int, int ) ),
             this, SLOT( handleRowsAboutToBeRemoved(const QModelIndex&, int,int ) ) );
    connect( model, SIGNAL( rowsRemoved( const QModelIndex&, int, int ) ),
             this, SLOT( handleRowsRemoved(const QModelIndex&, int,int ) ) );
    connect( model, SIGNAL( rowsAboutToBeInserted( const QModelIndex&, int, int ) ),
             this, SLOT( handleRowsAboutToBeInserted(const QModelIndex&, int,int ) ) );
    connect( model, SIGNAL( rowsInserted( const QModelIndex&, int, int ) ),
             this, SLOT( handleRowsInserted(const QModelIndex&, int,int ) ) );
}

void MsgListModel::handleDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    if ( topLeft.parent() != bottomRight.parent() || topLeft.column() != bottomRight.column() ) {
        emit layoutAboutToBeChanged();
        emit layoutChanged();
    }

    QModelIndex first = mapFromSource( topLeft );
    QModelIndex second = mapFromSource( bottomRight );

    if ( first.isValid() && second.isValid() && first.parent() == second.parent() ) {
        emit dataChanged( first, second );
    } else {
        // can't do much besides throwing out everything
        emit layoutAboutToBeChanged();
        emit layoutChanged();
    }
}

QModelIndex MsgListModel::index( int row, int column, const QModelIndex& parent ) const
{
    if ( ! msgList )
        return QModelIndex();

    if ( parent.isValid() )
        return QModelIndex();

    if ( column < 0 || column >= COLUMN_COUNT )
        return QModelIndex();

    Model* model = dynamic_cast<Model*>( sourceModel() );
    Q_ASSERT( model );

    if ( row >= static_cast<int>( msgList->rowCount( model ) ) || row < 0 )
        return QModelIndex();

    return createIndex( row, column, msgList->child( row, model ) );
}

QModelIndex MsgListModel::parent( const QModelIndex& index ) const
{
    return QModelIndex();
}

bool MsgListModel::hasChildren( const QModelIndex& parent ) const
{
    return ! parent.isValid();
}

int MsgListModel::rowCount( const QModelIndex& parent ) const
{
    if ( parent.isValid() )
        return 0;

    if ( ! msgList )
        return 0;

    return msgList->rowCount( dynamic_cast<Model*>( sourceModel() ) );
}

int MsgListModel::columnCount( const QModelIndex& parent ) const
{
    if ( parent.isValid() )
        return 0;
    if ( parent.column() != 0 && parent.column() != -1 )
        return 0;
    return COLUMN_COUNT;
}

QModelIndex MsgListModel::mapToSource( const QModelIndex& proxyIndex ) const
{
    if ( ! msgList )
        return QModelIndex();

    if ( proxyIndex.parent().isValid() )
        return QModelIndex();

    Model* model = dynamic_cast<Model*>( sourceModel() );
    Q_ASSERT( model );

    return model->createIndex( proxyIndex.row(), 0, msgList->child( proxyIndex.row(), model ) );
}

QModelIndex MsgListModel::mapFromSource( const QModelIndex& sourceIndex ) const
{
    if ( ! msgList )
        return QModelIndex();

    if ( sourceIndex.model() != sourceModel() )
        return QModelIndex();
    if ( dynamic_cast<TreeItemMessage*>( static_cast<TreeItem*>( sourceIndex.internalPointer() ) ) ) {
        return index( sourceIndex.row(), 0, QModelIndex() );
    } else {
        return QModelIndex();
    }
}

QVariant MsgListModel::data( const QModelIndex& proxyIndex, int role ) const
{
    if ( ! msgList )
        return QVariant();

    if ( ! proxyIndex.internalPointer() )
        return QVariant();

    switch ( role ) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            switch ( proxyIndex.column() ) {
                case SUBJECT:
                    return QAbstractProxyModel::data( proxyIndex, Qt::DisplayRole );
                case FROM:
                    return Imap::Message::MailAddress::prettyList(
                            dynamic_cast<TreeItemMessage*>( static_cast<TreeItem*>(
                                proxyIndex.internalPointer() )
                                )->envelope( static_cast<Model*>( sourceModel() ) ).from );
                case TO:
                    return Imap::Message::MailAddress::prettyList(
                            dynamic_cast<TreeItemMessage*>( static_cast<TreeItem*>(
                                proxyIndex.internalPointer() )
                                )->envelope( static_cast<Model*>( sourceModel() ) ).to );
                case DATE:
                    return dynamic_cast<TreeItemMessage*>( static_cast<TreeItem*>(
                                proxyIndex.internalPointer() )
                                )->envelope( static_cast<Model*>( sourceModel() ) ).date;
                case SIZE:
                    {
                    uint size = dynamic_cast<TreeItemMessage*>( static_cast<TreeItem*>(
                                proxyIndex.internalPointer() )
                                )->size( static_cast<Model*>( sourceModel() ) );
                    return size; // FIXME: nice format
                    }
                default:
                    return QVariant();
            }
        case Qt::TextAlignmentRole:
            switch ( proxyIndex.column() ) {
                case SIZE:
                    return Qt::AlignRight;
                default:
                    return QVariant();
            }
        default:
            {
            QModelIndex translated = createIndex( proxyIndex.row(), 0, proxyIndex.internalPointer() );
            return QAbstractProxyModel::data( translated, role );
            }
    }
}

QVariant MsgListModel::headerData ( int section, Qt::Orientation orientation, int role ) const
{
    if ( orientation != Qt::Horizontal || role != Qt::DisplayRole )
        return QAbstractItemModel::headerData( section, orientation, role );

    switch ( section ) {
        case SUBJECT:
            return tr( "Subject" );
        case FROM:
            return tr( "From" );
        case TO:
            return tr( "To" );
        case DATE:
            return tr( "Date" );
        case SIZE:
            return tr( "Size" );
        default:
            return QVariant();
    }
}

void MsgListModel::resetMe()
{
    setMailbox( QModelIndex() );
}

void MsgListModel::handleRowsAboutToBeRemoved( const QModelIndex& parent, int start, int end )
{
    if ( ! msgList )
        return;

    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( parent.internalPointer() ) );
    TreeItemMsgList* newList = dynamic_cast<TreeItemMsgList*>( static_cast<TreeItem*>( parent.internalPointer() ) );

    if ( parent.isValid() ) {
        Q_ASSERT( parent.model() == sourceModel() );
    } else {
        // a top-level mailbox might have been deleted, so we gotta setup proper pointer
        mailbox = static_cast<Model*>( sourceModel() )->_mailboxes;
        Q_ASSERT( mailbox );
    }

    if ( newList ) {
        if ( newList == msgList ) {
            beginRemoveRows( mapFromSource( parent ), start, end );
            for ( int i = start; i <= end; ++i )
                emit messageRemoved( msgList->child( i, static_cast<Model*>( sourceModel() ) ) );
        }
    } else if ( mailbox ) {
        Q_ASSERT( start > 0 );
        // if we're below it, we're gonna die
        for ( int i = start; i <= end; ++i ) {
            TreeItemMailbox* m = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( sourceModel()->index( i, 0, parent ).internalPointer() ) );
            Q_ASSERT( m );
            TreeItem* up = msgList->parent();
            while ( up ) {
                if ( m == up ) {
                    resetMe();
                    return;
                }
                up = up->parent();
            }
        }
    }
}

void MsgListModel::handleRowsRemoved( const QModelIndex& parent, int start, int end )
{
    if ( ! msgList )
        return;

    if ( ! parent.isValid() )
        return;

    if( dynamic_cast<TreeItemMsgList*>( static_cast<TreeItem*>( parent.internalPointer() ) ) == msgList )
        endRemoveRows();
}

void MsgListModel::handleRowsAboutToBeInserted( const QModelIndex& parent, int start, int end )
{
    TreeItemMsgList* newList = dynamic_cast<TreeItemMsgList*>( static_cast<TreeItem*>( parent.internalPointer() ) );
    if ( msgList && msgList == newList ) {
        beginInsertRows( mapFromSource( parent), start, end );
    }
}

void MsgListModel::handleRowsInserted( const QModelIndex& parent, int start, int end )
{
    TreeItemMsgList* newList = dynamic_cast<TreeItemMsgList*>( static_cast<TreeItem*>( parent.internalPointer() ) );
    if ( msgList && msgList == newList ) {
        endInsertRows();
    }
}

void MsgListModel::setMailbox( const QModelIndex& index )
{
    if ( ! index.isValid() ) {
        msgList = 0;
        reset();
        emit mailboxChanged();
        return;
    }

    TreeItemMailbox* mbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( index.internalPointer() ));
    Q_ASSERT( mbox );
    TreeItemMsgList* newList = dynamic_cast<TreeItemMsgList*>( mbox->child( 0, static_cast<Model*>( const_cast<QAbstractItemModel*>( index.model() ) ) ) );
    Q_ASSERT( newList );
    if ( newList != msgList ) {
        msgList = newList;
        reset();
        emit mailboxChanged();
    }
}

}
}

#include "MsgListModel.moc"
