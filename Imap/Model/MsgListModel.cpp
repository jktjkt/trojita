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
#include "Utils.h"

#include "iconloader/qticonloader.h"

#include <QApplication>
#include <QDebug>
#include <QFontMetrics>
#include <QMimeData>

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
        return;
    }

    QModelIndex first = mapFromSource( topLeft );
    QModelIndex second = mapFromSource( bottomRight );
    if ( ! first.isValid() || ! second.isValid() ) {
        return;
    }
    second = createIndex( second.row(), COLUMN_COUNT - 1, Model::realTreeItem( second ) );

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
    Q_UNUSED( index );
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
    if ( dynamic_cast<TreeItemMessage*>( Model::realTreeItem( sourceIndex ) ) ) {
        return index( sourceIndex.row(), 0, QModelIndex() );
    } else {
        return QModelIndex();
    }
}

QVariant MsgListModel::data( const QModelIndex& proxyIndex, int role ) const
{
    if ( ! msgList )
        return QVariant();

    if ( ! proxyIndex.isValid() )
        return QVariant();

    TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( Model::realTreeItem( proxyIndex ) );
                        Q_ASSERT( message );

    switch ( role ) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            switch ( proxyIndex.column() ) {
                case SUBJECT:
                    return QAbstractProxyModel::data( proxyIndex, Qt::DisplayRole );
                case FROM:
                    return Imap::Message::MailAddress::prettyList(
                            message->envelope( static_cast<Model*>( sourceModel() ) ).from,
                            role == Qt::DisplayRole );
                case TO:
                    return Imap::Message::MailAddress::prettyList(
                            message->envelope( static_cast<Model*>( sourceModel() ) ).to,
                            role == Qt::DisplayRole );
                case DATE:
                {
                    QDateTime res = message->envelope( static_cast<Model*>( sourceModel() ) ).date;
                    if ( res.date() == QDate::currentDate() )
                        return res.time().toString( Qt::SystemLocaleShortDate );
                    else
                        return res.toString( Qt::SystemLocaleShortDate );
                }
                case SIZE:
                    return PrettySize::prettySize( message->size( static_cast<Model*>( sourceModel() ) ) );
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
        case Qt::DecorationRole:
                switch ( proxyIndex.column() ) {
                case SUBJECT:
                    if ( ! message->fetched() )
                        return QVariant();
                    if ( message->isMarkedAsDeleted() )
                        return QtIconLoader::icon( QLatin1String("mail-deleted"),
                                                   QIcon( QLatin1String(":/icons/mail-deleted.png") ) );
                    else if ( message->isMarkedAsForwarded() && message->isMarkedAsReplied() )
                        return QtIconLoader::icon( QLatin1String("mail-replied-forw"),
                                                   QIcon( QLatin1String(":/icons/mail-replied-forw.png") ) );
                    else if ( message->isMarkedAsReplied() )
                        return QtIconLoader::icon( QLatin1String("mail-replied"),
                                                   QIcon( QLatin1String(":/icons/mail-replied.png") ) );
                    else if ( message->isMarkedAsForwarded() )
                        return QtIconLoader::icon( QLatin1String("mail-forwarded"),
                                                   QIcon( QLatin1String(":/icons/mail-forwarded.png") ) );
                    else if ( message->isMarkedAsRecent() )
                        return QtIconLoader::icon( QLatin1String("mail-recent"),
                                                   QIcon( QLatin1String(":/icons/mail-recent.png") ) );
                    else
                        return QIcon( QLatin1String(":/icons/transparent.png") );
                case SEEN:
                    if ( ! message->fetched() )
                        return QVariant();
                    if ( ! message->isMarkedAsRead() )
                        return QIcon( QLatin1String(":/icons/mail-unread.png") );
                    else
                        return QIcon( QLatin1String(":/icons/mail-read.png") );
                default:
                    return QVariant();
                }
        case Qt::FontRole:
            {
                TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( Model::realTreeItem( proxyIndex ) );
                Q_ASSERT( message );

                if ( ! message->fetched() )
                    return QVariant();

                QFont font;
                if ( message->isMarkedAsDeleted() )
                    font.setStrikeOut( true );
                if ( ! message->isMarkedAsRead() )
                    font.setBold( true );
                return font;
            }
        case RoleIsMarkedAsDeleted:
            return dynamic_cast<TreeItemMessage*>( Model::realTreeItem(
                                proxyIndex ) )->isMarkedAsDeleted();
        case RoleIsMarkedAsRead:
            return dynamic_cast<TreeItemMessage*>( Model::realTreeItem(
                                proxyIndex ) )->isMarkedAsRead();
        default:
            {
            QModelIndex translated = createIndex( proxyIndex.row(), 0, Model::realTreeItem( proxyIndex ) );
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

Qt::ItemFlags MsgListModel::flags( const QModelIndex& index ) const
{
    if ( ! index.isValid() || index.model() != this )
        return QAbstractProxyModel::flags( index );

    TreeItemMessage* message = dynamic_cast<TreeItemMessage*>(
            Model::realTreeItem( index ) );
    Q_ASSERT( message );

    if ( ! message->fetched() )
        return QAbstractProxyModel::flags( index );

    return Qt::ItemIsDragEnabled | QAbstractProxyModel::flags( index );
}

Qt::DropActions MsgListModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList MsgListModel::mimeTypes() const
{
    return QStringList() << QLatin1String("application/x-trojita-message-list");
}

QMimeData* MsgListModel::mimeData( const QModelIndexList& indexes ) const
{
    if ( indexes.isEmpty() )
        return 0;

    QMimeData* res = new QMimeData();
    QByteArray encodedData;
    QDataStream stream( &encodedData, QIODevice::WriteOnly );

    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( Model::realTreeItem(
            indexes.front() )->parent()->parent() );
    Q_ASSERT( mailbox );
    stream << mailbox->mailbox();

    for ( QModelIndexList::const_iterator it = indexes.begin(); it != indexes.end(); ++it ) {
        TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( Model::realTreeItem( *it ) );
        Q_ASSERT( message );
        Q_ASSERT( message->fetched() ); // should've been handled by flags()
        Q_ASSERT( message->parent()->parent() == mailbox );
        Q_ASSERT( message->uid() > 0 );
        stream << message->uid();
    }
    res->setData( QLatin1String("application/x-trojita-message-list"), encodedData );
    return res;
}

void MsgListModel::resetMe()
{
    setMailbox( QModelIndex() );
}

void MsgListModel::handleRowsAboutToBeRemoved( const QModelIndex& parent, int start, int end )
{
    if ( ! parent.isValid() ) {
        // Top-level items are tricky :(. As a quick hack, let's just die.
        resetMe();
        return;
    }

    if ( ! msgList )
        return;

    TreeItem* item = Model::realTreeItem( parent );
    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( item );
    TreeItemMsgList* newList = dynamic_cast<TreeItemMsgList*>( item );

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
            const Model* model = 0;
            QModelIndex translatedParent;
            Model::realTreeItem( parent, &model, &translatedParent );
            // FIXME: this assumes that no rows were removed by the proxy model
            TreeItemMailbox* m = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( model->index( i, 0, translatedParent ).internalPointer() ) );
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
    Q_UNUSED( start );
    Q_UNUSED( end );

    if ( ! msgList )
        return;

    if ( ! parent.isValid() ) {
        // already handled by resetMe() in handleRowsAboutToBeRemoved()
        return;
    }

    if( dynamic_cast<TreeItemMsgList*>( Model::realTreeItem( parent ) ) == msgList )
        endRemoveRows();
}

void MsgListModel::handleRowsAboutToBeInserted( const QModelIndex& parent, int start, int end )
{
    if ( ! parent.isValid() )
        return;

    TreeItemMsgList* newList = dynamic_cast<TreeItemMsgList*>( Model::realTreeItem( parent ) );
    if ( msgList && msgList == newList ) {
        beginInsertRows( mapFromSource( parent), start, end );
    }
}

void MsgListModel::handleRowsInserted( const QModelIndex& parent, int start, int end )
{
    if ( ! parent.isValid() )
        return;

    Q_UNUSED( start );
    Q_UNUSED( end );
    TreeItemMsgList* newList = dynamic_cast<TreeItemMsgList*>( Model::realTreeItem( parent ) );
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

    const Model* model = 0;
    TreeItemMailbox* mbox = dynamic_cast<TreeItemMailbox*>( Model::realTreeItem( index, &model ));
    Q_ASSERT( mbox );
    TreeItemMsgList* newList = dynamic_cast<TreeItemMsgList*>(
            mbox->child( 0, const_cast<Model*>( model ) ) );
    Q_ASSERT( newList );
    if ( newList != msgList && mbox->isSelectable() ) {
        msgList = newList;
        reset();
        emit mailboxChanged();
    }
}

TreeItemMailbox* MsgListModel::currentMailbox() const
{
    return msgList ? dynamic_cast<TreeItemMailbox*>( msgList->parent() ) : 0;
}

}
}

#include "MsgListModel.moc"
