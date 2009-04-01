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

    QModelIndex res;
    QModelIndex translatedParent = mapToSource( parent );

    if ( column < sourceModel()->columnCount( translatedParent ) &&
         row < sourceModel()->rowCount( translatedParent ) - 1 ) {
        void* ptr = sourceModel()->index( row + 1, column, translatedParent ).internalPointer();
        Q_ASSERT( ptr );
        return createIndex( row, column, ptr );
    } else {
        return QModelIndex();
    }
}

QModelIndex MailboxModel::parent( const QModelIndex& index ) const
{
    return index.isValid() ? mapFromSource( mapToSource( index ).parent() ) : QModelIndex();
}

int MailboxModel::rowCount( const QModelIndex& parent ) const
{
    int res = sourceModel()->rowCount( mapToSource( parent ) );
    if ( res > 0 )
        --res;
    return res;
}

int MailboxModel::columnCount( const QModelIndex& parent ) const
{
    return sourceModel()->columnCount( mapToSource( parent ) );
}

QModelIndex MailboxModel::mapToSource( const QModelIndex& proxyIndex ) const
{
    int row = proxyIndex.row();
    if ( row < 0 || proxyIndex.column() < 0 )
        return QModelIndex();
    if ( row == -1 )
        return QModelIndex();
    ++row;
    return static_cast<Imap::Mailbox::Model*>( sourceModel() )->createIndex( row, proxyIndex.column(), proxyIndex.internalPointer() );
}

QModelIndex MailboxModel::mapFromSource( const QModelIndex& sourceIndex ) const
{
    if ( !sourceIndex.isValid() )
        return QModelIndex();

    int row = sourceIndex.row();
    if ( row == 0 )
        return QModelIndex();
    if ( row > 0 )
        --row;

    return createIndex( row, sourceIndex.column(), sourceIndex.internalPointer() );
}

}
}

#include "MailboxModel.moc"
