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

#ifndef IMAP_MAILBOXMODEL_H
#define IMAP_MAILBOXMODEL_H

#include <QAbstractProxyModel>
#include "Model.h"

/** @short Namespace for IMAP interaction */
namespace Imap
{

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox
{

template <typename SourceModel> class SubtreeClassSpecificItem;

/** @short A model implementing view of the whole IMAP server */
class MailboxModel: public QAbstractProxyModel
{
    Q_OBJECT

public:
    MailboxModel(QObject *parent, Model *model);

    virtual QModelIndex index(int row, int column, const QModelIndex &parent=QModelIndex()) const;
    virtual QModelIndex parent(const QModelIndex &index) const;
    virtual int rowCount(const QModelIndex &parent=QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent=QModelIndex()) const;
    virtual QModelIndex mapToSource(const QModelIndex &proxyIndex) const;
    virtual QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;
    virtual bool hasChildren(const QModelIndex &parent = QModelIndex()) const;

    virtual QVariant data(const QModelIndex &proxyIndex, int role = Qt::DisplayRole) const;

    virtual Qt::ItemFlags flags(const QModelIndex &index) const;
    virtual Qt::DropActions supportedDropActions() const;
    virtual QStringList mimeTypes() const;
    virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                              int row, int column, const QModelIndex &parent);

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    QHash<int, QByteArray> trojitaProxyRoleNames() const;
#else
    virtual QHash<int, QByteArray> roleNames() const;
#endif

protected slots:
    void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void handleMessageCountPossiblyChanged(const QModelIndex &mailbox);

private slots:
    void handleModelAboutToBeReset();
    void handleModelReset();
    void handleRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void handleRowsRemoved(const QModelIndex &parent, int first, int last);
    void handleRowsAboutToBeInserted(const QModelIndex &parent, int first, int last);
    void handleRowsInserted(const QModelIndex &parent, int first, int last);

private:
    MailboxModel &operator=(const MailboxModel &);  // don't implement
    MailboxModel(const MailboxModel &);  // don't implement
    friend class SubtreeClassSpecificItem<MailboxModel>;
};

}

}

#endif /* IMAP_MAILBOXMODEL_H */
