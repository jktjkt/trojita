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

#ifndef IMAP_MSGLISTMODEL_H
#define IMAP_MSGLISTMODEL_H

#include <QAbstractProxyModel>
#include "Model.h"

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

/** @short A model implementing view of the whole IMAP server */
class MsgListModel: public QAbstractProxyModel {
    Q_OBJECT

public:
    MsgListModel( QObject* parent, Model* model );

    virtual QModelIndex index( int row, int column, const QModelIndex& parent=QModelIndex() ) const;
    virtual QModelIndex parent( const QModelIndex& index ) const;
    virtual int rowCount( const QModelIndex& parent=QModelIndex() ) const;
    virtual int columnCount( const QModelIndex& parent=QModelIndex() ) const;
    virtual QModelIndex mapToSource( const QModelIndex& proxyIndex ) const;
    virtual QModelIndex mapFromSource( const QModelIndex& sourceIndex ) const;
    virtual bool hasChildren( const QModelIndex& parent=QModelIndex() ) const;
    virtual QVariant data(const QModelIndex &proxyIndex, int role=Qt::DisplayRole) const;
    virtual QVariant headerData( int section, Qt::Orientation orientation, int role=Qt::DisplayRole ) const;
    virtual Qt::ItemFlags flags( const QModelIndex& index ) const;
    virtual QStringList mimeTypes() const;
    virtual QMimeData* mimeData( const QModelIndexList& indexes ) const;
    virtual Qt::DropActions supportedDropActions() const;

    TreeItemMailbox* currentMailbox() const;

    enum { RoleIsMarkedAsRead = Qt::UserRole + 1, RoleIsMarkedAsDeleted };
    enum { SUBJECT, SEEN, FROM, TO, DATE, SIZE, COLUMN_COUNT };

public slots:
    void resetMe();
    void setMailbox( const QModelIndex& index );
    void handleDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight );
    void handleRowsAboutToBeRemoved( const QModelIndex& parent, int start, int end );
    void handleRowsRemoved( const QModelIndex& parent, int start, int end );
    void handleRowsAboutToBeInserted( const QModelIndex& parent, int start, int end );
    void handleRowsInserted( const QModelIndex& parent, int start, int end );

signals:
    void messageRemoved( void* );
    void mailboxChanged();

private:
    MsgListModel& operator=( const Model& ); // don't implement
    MsgListModel( const Model& ); // don't implement

    TreeItemMsgList* msgList;
};

}

}

#endif /* IMAP_MSGLISTMODEL_H */
