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

#include <QDebug>
#include <typeinfo>

namespace Imap {
namespace Mailbox {

MsgListModel::MsgListModel( QObject* parent, Model* model ): QAbstractProxyModel(parent), msgList(0)
{
    setSourceModel( model );

    // FIXME: will need to be expanded when Model supports more signals...
    connect( model, SIGNAL( modelReset() ), this, SIGNAL( modelReset() ) );
    connect( model, SIGNAL( layoutAboutToBeChanged() ), this, SIGNAL( layoutAboutToBeChanged() ) );
    connect( model, SIGNAL( layoutChanged() ), this, SIGNAL( layoutChanged() ) );
    connect( model, SIGNAL( dataChanged( const QModelIndex&, const QModelIndex& ) ),
            this, SLOT( handleDataChanged( const QModelIndex&, const QModelIndex& ) ) );
}

void MsgListModel::handleDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    if ( topLeft.parent() != bottomRight.parent() || topLeft.column() != bottomRight.column() )
        emit layoutChanged();

    QModelIndex first = mapFromSource( topLeft );
    QModelIndex second = mapFromSource( bottomRight );

    if ( first.isValid() && second.isValid() && first.parent() == second.parent() && first.column() == second.column() ) {
        emit dataChanged( first, second );
    } else {
        // can't do much besides throwing out everything
        emit layoutChanged();
    }
}

QModelIndex MsgListModel::index( int row, int column, const QModelIndex& parent ) const
{
    if ( ! msgList )
        return QModelIndex();

    if ( parent.isValid() )
        return QModelIndex();

    if ( column != 0 )
        return QModelIndex();

    Model* model = dynamic_cast<Model*>( sourceModel() );
    Q_ASSERT( model );

    if ( row >= static_cast<int>( msgList->rowCount( model ) ) )
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
    return parent.isValid() ? 0 : 1; // FIXME: probably shouldn't be hardcoded...
}

QModelIndex MsgListModel::mapToSource( const QModelIndex& proxyIndex ) const
{
    if ( ! msgList )
        return QModelIndex();

    if ( proxyIndex.parent().isValid() )
        return QModelIndex();

    Model* model = dynamic_cast<Model*>( sourceModel() );
    Q_ASSERT( model );

    if ( proxyIndex.column() != 0 )
        return QModelIndex();

    return model->createIndex( proxyIndex.row(), proxyIndex.column(), msgList->child( proxyIndex.row(), model ) );
}

QModelIndex MsgListModel::mapFromSource( const QModelIndex& sourceIndex ) const
{
    if ( sourceIndex.internalPointer() != msgList )
        return QModelIndex();

    return index( sourceIndex.row(), 0, QModelIndex() );
}

void MsgListModel::setMailbox( const QModelIndex& index )
{
    /* We can't just grab QModelIndex::internalPointer(), as we want to support
     * going through the MsgListModel, which is a QSortFilterProxyModel, which
     * uses indices for its own twisted internal perverse purposes. Oh well,
     * this is even documented, kind of... */

    const Model* originalModel = dynamic_cast<const Model*>( index.model() );
    const MailboxModel* mailboxModel = dynamic_cast<const MailboxModel*>( index.model() );

    TreeItemMsgList* list = 0;
    TreeItemMailbox* mbox = 0;
    if ( mailboxModel ) {
        mbox = dynamic_cast<TreeItemMailbox*>(
                static_cast<TreeItem*>(
                    mailboxModel->mapToSource( index ).internalPointer()
                    ) );
        Q_ASSERT( mbox );
        Model* realModel = dynamic_cast<Model*>( mailboxModel->sourceModel() );
        if ( realModel ) {
            list = dynamic_cast<TreeItemMsgList*>( mbox->child( 0, realModel ) );
        }
    } else if ( originalModel ) {
        mbox = dynamic_cast<TreeItemMailbox*>(
                static_cast<TreeItem*>(
                    index.internalPointer()
                    ) );
        list = dynamic_cast<TreeItemMsgList*>(
                static_cast<TreeItem*>( index.internalPointer() ) );
        if ( mbox && ! list ) {
            list = dynamic_cast<TreeItemMsgList*>( mbox->child( 0, originalModel ) );
        }
    } else {
        qDebug() << "unrecognized kind of model, sorry:" << typeid(index.model()).name();
    }

    if ( list ) {
        msgList = list;
        reset();
    }
}

}
}

#include "MsgListModel.moc"
