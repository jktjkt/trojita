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

MailboxModel::MailboxModel( QObject* parent, Model* model ): QAbstractProxyModel(parent), foo("%1")
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
    qDebug() << foo.arg("hasChildren") << parent; foo = " " + foo;
    QModelIndex index = mapToSource( parent );

    TreeItemMailbox* mbox = dynamic_cast<TreeItemMailbox*>(
            static_cast<TreeItem*>(
                index.internalPointer()
                ) );
    bool res = mbox ?
               mbox->hasChildMailboxes( static_cast<Model*>( sourceModel() ) ) :
               sourceModel()->hasChildren( index );
    foo = foo.mid( 1 ); qDebug() << foo.arg("-> hasChildren") << res;
        return res;
}


void MailboxModel::handleDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    qDebug() << foo.arg("handleDataChanged") << topLeft << bottomRight;
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
    qDebug() << foo.arg("index") << row << column << parent; foo = " " + foo;

    if ( row < 0 || column < 0 ) {
        foo = foo.mid( 1 ); qDebug() << foo.arg("-> index") << QModelIndex();
        return QModelIndex();
    }

    QModelIndex res;
    QModelIndex translatedParent = mapToSource( parent );

    if ( hasChildren( translatedParent ) &&
         column < sourceModel()->columnCount( translatedParent ) &&
         row < sourceModel()->rowCount( translatedParent ) - 1 ) {
        res = createIndex( row, column, translatedParent.child( row, column ).internalPointer() );
    } else {
        res = QModelIndex();
    }
    foo = foo.mid( 1 ); qDebug() << foo.arg("-> index") << res;
    return res;
}

QModelIndex MailboxModel::parent( const QModelIndex& index ) const
{
    qDebug() << foo.arg("parent") << index; foo = " " + foo;
    QModelIndex res;
    if ( !index.isValid() )
        res = QModelIndex();
    else
        res = mapFromSource( mapToSource( index ).parent() );
    foo = foo.mid( 1 ); qDebug() << foo.arg("-> parent") << res;
    return res;
}

int MailboxModel::rowCount( const QModelIndex& parent ) const
{
    qDebug() << foo.arg("rowCount") << parent; foo = " " + foo;
    int res = sourceModel()->rowCount( mapToSource( parent ) ) - 1;
    foo = foo.mid( 1 ); qDebug() << foo.arg("-> rowCount") << res;
    return res;
}

int MailboxModel::columnCount( const QModelIndex& parent ) const
{
    qDebug() << foo.arg("columnCount") << parent; foo = " " + foo;
    int res = sourceModel()->columnCount( mapToSource( parent ) );
    foo = foo.mid( 1 ); qDebug() << foo.arg("-> columnCount") << res;
    return res;
}

QVariant MailboxModel::data( const QModelIndex& proxyIndex, int role ) const
{
    // FIXME: delete after debugging
    qDebug() << foo.arg("data") << proxyIndex; foo = " " + foo;
    QVariant res = QAbstractProxyModel::data( proxyIndex, role );
    foo = foo.mid( 1 ); qDebug() << foo.arg("-> data") << res;
    return res;
}

QModelIndex MailboxModel::mapToSource( const QModelIndex& proxyIndex ) const
{
    Q_ASSERT( foo.length() < 20 );
    qDebug() << foo.arg("mapToSource") << proxyIndex; foo = " " + foo;
    int row = proxyIndex.row();
    if ( row >= 0 ) {
        // because all real mailboxes in the source model start at 1, 0 is a list of messages
        ++row;
    }
    QModelIndex res;
    if ( proxyIndex.isValid() ) {
        //res = sourceModel()->index( row, proxyIndex.column(), mapToSource( parent( proxyIndex ) ) );
        res = sourceModel()->index( row, proxyIndex.column(), proxyIndex );
    }
    foo = foo.mid( 1 ); qDebug() << foo.arg("-> mapToSource") << res;
    return res;
}

QModelIndex MailboxModel::mapFromSource( const QModelIndex& sourceIndex ) const
{
    qDebug() << foo.arg("mapFromSource") << sourceIndex; foo = " " + foo;
    QModelIndex res;
    int row = sourceIndex.row();
    if ( row == 0 ) {
        foo = foo.mid( 1 ); qDebug() << foo.arg("-> mapFromSource") << QModelIndex();
        return QModelIndex();
    }

    if ( row > 0 )
        --row;
    if ( sourceIndex.isValid() )
        res = createIndex( row, sourceIndex.column(), sourceIndex.internalPointer() );
    foo = foo.mid( 1 ); qDebug() << foo.arg("-> mapFromSource") << res;
    return res;
}

}
}

#include "MailboxModel.moc"
