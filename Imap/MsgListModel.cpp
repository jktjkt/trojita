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

#include "Imap/MsgListModel.h"
#include "Imap/MailboxTree.h"
#include "Imap/MailboxModel.h"

#include <QDebug>
#include <typeinfo>

namespace Imap {
namespace Mailbox {

MsgListModel::MsgListModel( QObject* parent, Model* model ): QAbstractProxyModel(parent), msgList(0)
{
    setSourceModel( model );
}

QModelIndex MsgListModel::index( int row, int column, const QModelIndex& parent ) const
{
    if ( ! msgList )
        return QModelIndex();

    if ( parent.internalPointer() != msgList )
        return QModelIndex();

    if ( column != 0 )
        return QModelIndex();

    Model* model = static_cast<Model*>( sourceModel() );

    if ( row < 0 || row >= static_cast<int>( msgList->rowCount( model ) ) )
        return QModelIndex();

    return createIndex( row, column, msgList->child( row, model ) );
}

QModelIndex MsgListModel::parent( const QModelIndex& index ) const
{
    return QModelIndex();
}

int MsgListModel::rowCount( const QModelIndex& parent ) const
{
    if ( parent.isValid() )
        return 0;

    if ( ! msgList )
        return 0;

    return msgList->rowCount( static_cast<Model*>( sourceModel() ) );
}

int MsgListModel::columnCount( const QModelIndex& parent ) const
{
    return parent.isValid() ? 0 : 1; // FIXME: probably shouldn't be hardcoded...
}

QModelIndex MsgListModel::mapToSource( const QModelIndex& proxyIndex ) const
{
    if ( ! msgList )
        return QModelIndex();

    return sourceModel()->index( proxyIndex.row(), proxyIndex.column(), QModelIndex() );
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
        qDebug() << "setMailbox succeeded";
        msgList = list;
        reset();
    } else {
        qDebug() << "setMailbox failed";
    }
}

}
}

#include "MsgListModel.moc"
