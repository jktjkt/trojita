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

#include <QDebug>

namespace Imap {
namespace Mailbox {

MailboxModel::MailboxModel( QObject* parent, Model* model ): QAbstractProxyModel(parent)
{
    setSourceModel( model );

    // FIXME: will need to be expanded when Model supports more signals...
    connect( model, SIGNAL( modelReset() ), this, SIGNAL( modelReset() ) );
    connect( model, SIGNAL( layoutAboutToBeChanged() ), this, SIGNAL( layoutAboutToBeChanged() ) );
    connect( model, SIGNAL( layoutChanged() ), this, SIGNAL( layoutChanged() ) );
    connect( model, SIGNAL( dataChanged( const QModelIndex&, const QModelIndex& ) ),
            this, SLOT( handleDataChanged( const QModelIndex&, const QModelIndex& ) ) );
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

    if ( parent.column() !=  0 && parent.column() != -1 )
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
    if ( role == Qt::DisplayRole ) {
        switch ( proxyIndex.column() ) {
            case NAME:
                return QAbstractProxyModel::data( proxyIndex, role );
            case TOTAL_MESSAGE_COUNT:
                {
                    TreeItemMailbox* mbox = dynamic_cast<TreeItemMailbox*>(
                            static_cast<TreeItem*>( proxyIndex.internalPointer() )
                            );
                    Q_ASSERT( mbox );
                    int num = mbox->totalMessageCount( static_cast<Imap::Mailbox::Model*>( sourceModel() ) );
                    if ( num == -1 )
                        return "?";
                    else
                        return num;
                }
            case UNREAD_MESSAGE_COUNT:
                {
                    TreeItemMailbox* mbox = dynamic_cast<TreeItemMailbox*>(
                            static_cast<TreeItem*>( proxyIndex.internalPointer() )
                            );
                    Q_ASSERT( mbox );
                    int num = mbox->unreadMessageCount( static_cast<Imap::Mailbox::Model*>( sourceModel() ) );
                    if ( num == -1 )
                        return "?";
                    else
                        return num;
                }
            default:
                return QVariant();
        }
        return QLatin1String("blesmrt");
    } else {
        QModelIndex translated = createIndex( proxyIndex.row(), 0, proxyIndex.internalPointer() );
        return QAbstractProxyModel::data( translated, role );
    }
}

}
}

#include "MailboxModel.moc"
