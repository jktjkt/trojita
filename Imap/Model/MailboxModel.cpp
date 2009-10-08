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

#include "MailboxModel.h"
#include "MailboxTree.h"

#include "iconloader/qticonloader.h"

#include <QDebug>
#include <QFont>
#include <QMimeData>

namespace Imap {
namespace Mailbox {

MailboxModel::MailboxModel( QObject* parent, Model* model ): QAbstractProxyModel(parent)
{
    setSourceModel( model );

    // FIXME: will need to be expanded when Model supports more signals...
    connect( model, SIGNAL( modelAboutToBeReset() ), this, SLOT( resetMe() ) );
    connect( model, SIGNAL( layoutAboutToBeChanged() ), this, SIGNAL( layoutAboutToBeChanged() ) );
    connect( model, SIGNAL( layoutChanged() ), this, SIGNAL( layoutChanged() ) );
    connect( model, SIGNAL( dataChanged( const QModelIndex&, const QModelIndex& ) ),
            this, SLOT( handleDataChanged( const QModelIndex&, const QModelIndex& ) ) );
    connect( model, SIGNAL( rowsAboutToBeRemoved( const QModelIndex&, int, int ) ),
             this, SLOT( handleRowsAboutToBeRemoved( const QModelIndex&, int, int ) ) );
    connect( model, SIGNAL( rowsRemoved( const QModelIndex&, int, int ) ),
             this, SLOT( handleRowsRemoved( const QModelIndex&, int, int ) ) );
    connect( model, SIGNAL( messageCountPossiblyChanged( const QModelIndex& ) ),
             this, SLOT( handleMessageCountPossiblyChanged( const QModelIndex& ) ) );
    // FIXME: rowsAboutToBeInserted, just for the sake of completeness :)
}

void MailboxModel::resetMe()
{
    reset();
}

bool MailboxModel::hasChildren( const QModelIndex& parent ) const
{
    if ( parent.isValid() && parent.column() != 0 )
        return false;

    QModelIndex index = mapToSource( parent );

    TreeItemMailbox* mbox = dynamic_cast<TreeItemMailbox*>(
            static_cast<TreeItem*>(
                index.internalPointer()
                ) );
    return mbox ?
            mbox->hasChildMailboxes( static_cast<Model*>( sourceModel() ) ) :
            sourceModel()->hasChildren( index );
}

void MailboxModel::handleDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    if ( topLeft.parent() != bottomRight.parent() || topLeft.column() != bottomRight.column() ) {
        emit layoutAboutToBeChanged();
        emit layoutChanged();
    }

    QModelIndex first = mapFromSource( topLeft );
    QModelIndex second = mapFromSource( bottomRight );

    if ( first.isValid() && second.isValid() && first.parent() == second.parent() && first.column() == second.column() ) {
        emit dataChanged( first, second );
    } else {
        // can't do much besides throwing out everything
        emit layoutAboutToBeChanged();
        emit layoutChanged();
    }
}

QModelIndex MailboxModel::index( int row, int column, const QModelIndex& parent ) const
{
    if ( row < 0 || column < 0 )
        return QModelIndex();

    if ( parent.column() != 0 && parent.column() != -1 )
        return QModelIndex();

    QModelIndex res;
    QModelIndex translatedParent = mapToSource( parent );

    if ( column < COLUMN_COUNT &&
         row < sourceModel()->rowCount( translatedParent ) - 1 ) {
        void* ptr = sourceModel()->index( row + 1, 0, translatedParent ).internalPointer();
        Q_ASSERT( ptr );
        return createIndex( row, column, ptr );
    } else {
        return QModelIndex();
    }
}

QModelIndex MailboxModel::parent( const QModelIndex& index ) const
{
    return mapFromSource( mapToSource( index ).parent() );
}

int MailboxModel::rowCount( const QModelIndex& parent ) const
{
    if ( parent.column() != 0 && parent.column() != -1 )
        return 0;
    int res = sourceModel()->rowCount( mapToSource( parent ) );
    if ( res > 0 )
        --res;
    return res;
}

int MailboxModel::columnCount( const QModelIndex& parent ) const
{
    return parent.column() == 0 || parent.column() == -1 ? COLUMN_COUNT : 0;
}

QModelIndex MailboxModel::mapToSource( const QModelIndex& proxyIndex ) const
{
    int row = proxyIndex.row();
    if ( row < 0 || proxyIndex.column() < 0 )
        return QModelIndex();
    ++row;
    return static_cast<Imap::Mailbox::Model*>( sourceModel() )->createIndex( row, 0, proxyIndex.internalPointer() );
}

QModelIndex MailboxModel::mapFromSource( const QModelIndex& sourceIndex ) const
{
    if ( !sourceIndex.isValid() )
        return QModelIndex();

    if ( ! dynamic_cast<Imap::Mailbox::TreeItemMailbox*>(
            static_cast<Imap::Mailbox::TreeItem*>( sourceIndex.internalPointer() ) ) )
        return QModelIndex();

    int row = sourceIndex.row();
    if ( row == 0 )
        return QModelIndex();
    if ( row > 0 )
        --row;

    return createIndex( row, 0, sourceIndex.internalPointer() );
}

QVariant MailboxModel::data( const QModelIndex& proxyIndex, int role ) const
{
    if ( ! proxyIndex.isValid() )
        return QVariant();

    TreeItemMailbox* mbox = dynamic_cast<TreeItemMailbox*>(
            static_cast<TreeItem*>( proxyIndex.internalPointer() )
            );
    Q_ASSERT( mbox );
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( mbox->_children[0] );
    Q_ASSERT( list );
    switch ( role ) {
        case Qt::DisplayRole:
            switch ( proxyIndex.column() ) {
                case NAME:
                    return QAbstractProxyModel::data( proxyIndex, role );
                case TOTAL_MESSAGE_COUNT:
                {
                    if ( ! mbox->isSelectable() )
                        return QVariant();

                    int num = list->totalMessageCount(
                                    static_cast<Imap::Mailbox::Model*>( sourceModel() ) );
                    return list->numbersFetched() ? QString::number( num ) : tr("?");
                }
                case UNREAD_MESSAGE_COUNT:
                {
                    if ( ! mbox->isSelectable() )
                        return QVariant();

                    int num = list->unreadMessageCount(
                                    static_cast<Imap::Mailbox::Model*>( sourceModel() ) );
                    return list->numbersFetched() ? QString::number( num ) : tr("?");
                }
                default:
                    return QVariant();
            }
        case Qt::FontRole:
            if ( list->numbersFetched() && list->unreadMessageCount(
                                    static_cast<Imap::Mailbox::Model*>( sourceModel() ) ) > 0 ) {
                QFont font;
                font.setBold( true );
                return font;
            } else
                return QVariant();
        case Qt::TextAlignmentRole:
            switch ( proxyIndex.column() ) {
                case TOTAL_MESSAGE_COUNT:
                case UNREAD_MESSAGE_COUNT:
                    return Qt::AlignRight;
                default:
                    return QVariant();
            }
        case Qt::DecorationRole:
            switch ( proxyIndex.column() ) {
                case NAME:
                    if ( list->loading() || ! list->numbersFetched() )
                        return QtIconLoader::icon( QLatin1String("folder-grey"),
                                                   QIcon( QLatin1String(":/icons/folder-grey.png") ) );
                    else if ( mbox->mailbox().toUpper() == QLatin1String("INBOX") )
                        return QtIconLoader::icon( QLatin1String("mail-folder-inbox"),
                                                   QIcon( QLatin1String(":/icons/mail-folder-inbox") ) );
                    else
                        return QtIconLoader::icon( QLatin1String("folder"),
                                                   QIcon( QLatin1String(":/icons/folder.png") ) );
                default:
                    return QVariant();
            }
        default:
            return QAbstractProxyModel::data( createIndex( proxyIndex.row(), 0, proxyIndex.internalPointer() ), role );
    }
}

void MailboxModel::handleMessageCountPossiblyChanged( const QModelIndex& mailbox )
{
    QModelIndex translated = mapFromSource( mailbox );
    if ( translated.isValid() ) {
        QModelIndex topLeft = createIndex( translated.row(), TOTAL_MESSAGE_COUNT, translated.internalPointer() );
        QModelIndex bottomRight = createIndex( translated.row(), UNREAD_MESSAGE_COUNT, translated.internalPointer() );
        emit dataChanged( topLeft, bottomRight );
    }
}

QVariant MailboxModel::headerData ( int section, Qt::Orientation orientation, int role ) const
{
    if ( orientation != Qt::Horizontal || role != Qt::DisplayRole )
        return QAbstractItemModel::headerData( section, orientation, role );

    switch ( section ) {
        case NAME:
            return tr( "Mailbox" );
        case TOTAL_MESSAGE_COUNT:
            return tr( "Total" );
        case UNREAD_MESSAGE_COUNT:
            return tr( "Unread" );
        default:
            return QVariant();
    }
}

Qt::ItemFlags MailboxModel::flags( const QModelIndex& index ) const
{
    if ( ! index.isValid() )
        return QAbstractProxyModel::flags( index );

    TreeItemMailbox* mbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( index.internalPointer() ) );
    Q_ASSERT( mbox );
    if ( mbox->isSelectable() && static_cast<Model*>( sourceModel() )->isNetworkAvailable() )
        return Qt::ItemIsDropEnabled | QAbstractProxyModel::flags( index );
    else
        return QAbstractProxyModel::flags( index );
}

Qt::DropActions MailboxModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList MailboxModel::mimeTypes() const
{
    return QStringList() << QLatin1String("application/x-trojita-message-list");
}

bool MailboxModel::dropMimeData( const QMimeData* data, Qt::DropAction action,
                                 int row, int column, const QModelIndex& parent )
{
    if ( action != Qt::CopyAction && action != Qt::MoveAction )
        return false;

    if ( ! parent.isValid() )
        return false;

    if ( ! static_cast<Model*>( sourceModel() )->isNetworkAvailable() )
        return false;

    TreeItemMailbox* target = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( parent.internalPointer() ) );
    Q_ASSERT( target );

    if ( ! target->isSelectable() )
        return false;

    QByteArray encodedData = data->data( "application/x-trojita-message-list" );
    QDataStream stream( &encodedData, QIODevice::ReadOnly );

    Q_ASSERT( ! stream.atEnd() );
    QString origMboxName;
    stream >> origMboxName;
    TreeItemMailbox* origMbox = static_cast<Model*>( sourceModel() )->findMailboxByName( origMboxName );
    if ( ! origMbox ) {
        qDebug() << "Can't find original mailbox when performing a drag&drop on messages";
        return false;
    }

    Imap::Sequence seq;
    while ( ! stream.atEnd() ) {
        uint uid;
        stream >> uid;
        seq.add( uid );
    }

    static_cast<Model*>( sourceModel() )->copyMessages( origMbox, target->mailbox(), seq );
    if ( action == Qt::MoveAction )
        static_cast<Model*>( sourceModel() )->markUidsDeleted( origMbox, seq );
    return true;
}

void MailboxModel::handleRowsAboutToBeRemoved( const QModelIndex& parent, int first, int last )
{
    TreeItemMailbox* parentMbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( parent.internalPointer() ) );
    if ( parent.internalPointer() && ! parentMbox )
        return;
    if ( ! parentMbox )
        parentMbox = static_cast<Imap::Mailbox::Model*>( sourceModel() )->_mailboxes;
    Q_ASSERT( first == 1 );
    Q_ASSERT( last == parentMbox->_children.size() - 1 );
    beginRemoveRows( mapFromSource( parent ), 0, last - 1 );
}

void MailboxModel::handleRowsRemoved( const QModelIndex& parent, int first, int last )
{
    Q_UNUSED( first );
    Q_UNUSED( last );
    TreeItemMailbox* parentMbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( parent.internalPointer() ) );
    if ( parent.internalPointer() && ! parentMbox )
        return;
    endRemoveRows();
}


}
}

#include "MailboxModel.moc"
