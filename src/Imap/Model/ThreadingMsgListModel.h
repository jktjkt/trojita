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

#ifndef IMAP_THREADINGMSGLISTMODEL_H
#define IMAP_THREADINGMSGLISTMODEL_H

#include <QAbstractProxyModel>
#include "Imap/Parser/Response.h"

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

class TreeItem;

struct ThreadNodeInfo {
    uint internalId;
    uint uid;
    uint parent;
    QList<uint> children;
    TreeItem *ptr;
    ThreadNodeInfo(): internalId(0), uid(0), parent(0), ptr(0) {}
};

QDebug operator<<(QDebug debug, const ThreadNodeInfo &node);

/** @short A model implementing view of the whole IMAP server */
class ThreadingMsgListModel: public QAbstractProxyModel {
    Q_OBJECT

public:
    ThreadingMsgListModel( QObject *parent );
    virtual void setSourceModel( QAbstractItemModel *sourceModel );

    virtual QModelIndex index( int row, int column, const QModelIndex& parent=QModelIndex() ) const;
    virtual QModelIndex parent( const QModelIndex& index ) const;
    virtual int rowCount( const QModelIndex& parent=QModelIndex() ) const;
    virtual int columnCount( const QModelIndex& parent=QModelIndex() ) const;
    virtual QModelIndex mapToSource( const QModelIndex& proxyIndex ) const;
    virtual QModelIndex mapFromSource( const QModelIndex& sourceIndex ) const;
    virtual bool hasChildren( const QModelIndex& parent=QModelIndex() ) const;
    virtual QVariant data( const QModelIndex &proxyIndex, int role ) const;
    virtual Qt::ItemFlags flags( const QModelIndex &index ) const;

    /** @short List of capabilities which could be used for threading

    If any of them are present in server's capabilities, at least some level of threading will be possible.
*/
    static QStringList supportedCapabilities();

public slots:
    void resetMe();
    void handleDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight );
    void handleRowsAboutToBeRemoved( const QModelIndex& parent, int start, int end );
    void handleRowsRemoved( const QModelIndex& parent, int start, int end );
    void handleRowsAboutToBeInserted( const QModelIndex& parent, int start, int end );
    void handleRowsInserted( const QModelIndex& parent, int start, int end );
    void slotThreadingAvailable( const QModelIndex &mailbox, const QString &algorithm,
                                 const QStringList &searchCriteria,
                                 const QVector<Imap::Responses::Thread::Node> &mapping );

private slots:
    void updateNoThreading();
    void askForThreading();

private:
    void updatePersistentIndexes();
    void registerThreading( const QVector<Imap::Responses::Thread::Node> &mapping, uint parentId );

    ThreadingMsgListModel& operator=( const ThreadingMsgListModel& ); // don't implement
    ThreadingMsgListModel( const ThreadingMsgListModel& ); // don't implement

    QHash<uint,uint> uidToInternal;
    QHash<uint,ThreadNodeInfo> _threading;
    uint _threadingHelperLastId;
};

}

}

#endif /* IMAP_THREADINGMSGLISTMODEL_H */
