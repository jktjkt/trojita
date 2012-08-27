/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

#ifndef IMAP_MODEL_SUBTREEMODEL_H
#define IMAP_MODEL_SUBTREEMODEL_H

#include <QAbstractProxyModel>

/** @short Namespace for IMAP interaction */
namespace Imap
{

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox
{

/** @short Helper for SubtreeModel for type-safe casting of the source model due to QAIM's createIndex being protected */
class SubtreeClassAdaptor;

/** @short Proxy model showing a subtree of the source model

This proxy model presents a subtree of the source model.  The index passed to setSourceModel's rootIndex option
will act as the root item of the exported portion of the original tree.

Certain operations like wide dataChanged() on "weird" regions are not supported and will end in an QASSERT(false).
*/
class SubtreeModel: public QAbstractProxyModel
{
    Q_OBJECT
    Q_DISABLE_COPY(SubtreeModel)

    Q_PROPERTY(bool itemsValid READ itemsValid NOTIFY validityChanged)

public:
    SubtreeModel(QObject *parent, SubtreeClassAdaptor *classSpecificAdaptor);
    virtual ~SubtreeModel();

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual QModelIndex parent(const QModelIndex &child) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QModelIndex mapToSource(const QModelIndex &proxyIndex) const;
    virtual QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;
    void setSourceModel(QAbstractItemModel *sourceModel);
    Q_INVOKABLE void setRootItem(QModelIndex rootIndex);
    Q_INVOKABLE void setRootItemByOffset(const int row);
    Q_INVOKABLE void setRootOneLevelUp();
    Q_INVOKABLE void setOriginalRoot();
    Q_INVOKABLE QModelIndex parentOfRoot() const;
    Q_INVOKABLE bool itemsValid() const;
    QModelIndex rootIndex() const;

private slots:
    void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void handleModelAboutToBeReset();
    void handleModelReset();
    void handleRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void handleRowsRemoved(const QModelIndex &parent, int first, int last);
    void handleRowsAboutToBeInserted(const QModelIndex &parent, int first, int last);
    void handleRowsInserted(const QModelIndex &parent, int first, int last);

signals:
    void validityChanged();

private:
    bool isVisibleIndex(QModelIndex sourceIndex) const;
    QPersistentModelIndex m_rootIndex;
    SubtreeClassAdaptor *m_classAdaptor;
    bool m_usingInvalidRoot;
};

/** @short Subtree model implementation for Model */
class SubtreeModelOfModel: public SubtreeModel
{
    Q_OBJECT
    Q_DISABLE_COPY(SubtreeModelOfModel)

public:
    SubtreeModelOfModel(QObject *parent = 0);
};

/** @short Subtree model implementation for MailboxModel */
class SubtreeModelOfMailboxModel: public SubtreeModel
{
    Q_OBJECT
    Q_DISABLE_COPY(SubtreeModelOfMailboxModel)

public:
    SubtreeModelOfMailboxModel(QObject *parent = 0);
};

}

}

#endif /* IMAP_MODEL_SUBTREEMODEL_H */
