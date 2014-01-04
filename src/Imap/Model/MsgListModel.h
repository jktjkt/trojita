/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef IMAP_MSGLISTMODEL_H
#define IMAP_MSGLISTMODEL_H

#include <QAbstractProxyModel>
#include "Model.h"
#include "ItemRoles.h"

/** @short Namespace for IMAP interaction */
namespace Imap
{

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox
{

/** @short A model implementing view of the whole IMAP server */
class MsgListModel: public QAbstractProxyModel
{
    Q_OBJECT

    Q_PROPERTY(bool itemsValid READ itemsValid NOTIFY indexStateChanged)

public:
    MsgListModel(QObject *parent, Model *model);

    virtual QModelIndex index(int row, int column, const QModelIndex &parent=QModelIndex()) const;
    virtual QModelIndex parent(const QModelIndex &index) const;
    virtual int rowCount(const QModelIndex &parent=QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent=QModelIndex()) const;
    virtual QModelIndex mapToSource(const QModelIndex &proxyIndex) const;
    virtual QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;
    virtual bool hasChildren(const QModelIndex &parent=QModelIndex()) const;
    virtual QVariant data(const QModelIndex &proxyIndex, int role=Qt::DisplayRole) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const;
    virtual QStringList mimeTypes() const;
    virtual QMimeData *mimeData(const QModelIndexList &indexes) const;
    virtual Qt::DropActions supportedDropActions() const;

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    QHash<int, QByteArray> trojitaProxyRoleNames() const;
#else
    virtual QHash<int, QByteArray> roleNames() const;
#endif

    QModelIndex currentMailbox() const;

    bool itemsValid() const;

    enum { SUBJECT, SEEN, FROM, TO, CC, BCC, DATE, RECEIVED_DATE, SIZE, COLUMN_COUNT };

public slots:
    void resetMe();
    void setMailbox(const QModelIndex &index);
    Q_INVOKABLE void setMailbox(const QString &mailboxName);
    void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void handleRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end);
    void handleRowsRemoved(const QModelIndex &parent, int start, int end);
    void handleRowsAboutToBeInserted(const QModelIndex &parent, int start, int end);
    void handleRowsInserted(const QModelIndex &parent, int start, int end);

signals:
    void messageRemoved(void *);
    void mailboxChanged(const QModelIndex &mailbox);

    /** @short Messages are available for the first time after selecting new mailbox */
    void messagesAvailable();

    void indexStateChanged();

private:
    MsgListModel &operator=(const MsgListModel &);  // don't implement
    MsgListModel(const MsgListModel &);  // don't implement

    void checkPersistentIndex() const;

    QPersistentModelIndex msgList;
    mutable TreeItemMsgList *msgListPtr;
    bool waitingForMessages;

    friend class ThreadingMsgListModel;
};

}

}

#endif /* IMAP_MSGLISTMODEL_H */
