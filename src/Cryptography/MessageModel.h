/* Copyright (C) 2014 - 2015 Stephan Platz <trojita@paalsteek.de>
   Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

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

#ifndef CRYPTOGRAPHY_MESSAGEMODEL_H
#define CRYPTOGRAPHY_MESSAGEMODEL_H

#include <QHash>
#include <QPersistentModelIndex>

#include "Cryptography/MessagePart.h"

namespace Cryptography {

class PartReplacer;
class ProxyMessagePart;

/** @short A model for a single message including raw and possibly decrypted message parts
 *
 * This model mimics the structure of a source model and has some provisions for adding custom
 * subtree items as replacements for arbitrary nodes (not just leaves). Internally, each node is
 * represented either by a LocalMessagePart (for those items which were added manually as
 * replacements) or ProxyMessagePart (for those items which are just proxies pointing back to the
 * original model, with unmodified position).
 *
 * This model makes a couple of assumptions. It doesn't support dynamic models at all; any removed
 * or added row/column/whatever in the source model will likely end up in a segfault.
 */
class MessageModel: public QAbstractItemModel
{
    Q_OBJECT

public:
    MessageModel(QObject *parent, const QModelIndex &message);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    QModelIndex message() const;

    /** @short Push a new complete subtree to become the only child of the indicated parent

    The @arg parent is required to not have any children at the time of call. The @arg child should
    form a standalone tree of MIME parts prepared in advance, and it will be pushed to the complete tree
    so that the root item of the added subtree becomes the first and only child of @arg parent. This
    means that @arg parent will have exactly one child when this function returns.
    */
    void insertSubtree(const QModelIndex &parent, Cryptography::MessagePart::Ptr tree);

    /** @short Overload, for taking several items and making them new children at the same level */
    void insertSubtree(const QModelIndex &parent, std::vector<MessagePart::Ptr> &&parts);

    void replaceMeWithSubtree(const QModelIndex &parent, MessagePart *partToReplace, MessagePart::Ptr tree);

    /** @short Activate a custom MIME part handler/replacer */
    void registerPartHandler(std::shared_ptr<PartReplacer> module);

#ifdef MIME_TREE_DEBUG
    void debugDump();
#endif

private slots:
    void mapDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

signals:
    void error(const QModelIndex &parent, const QString &error, const QString &details);

private:
    MessagePart *translatePtr(const QModelIndex &part) const;

    const QPersistentModelIndex m_message;
    QHash<QPersistentModelIndex, MessagePart*> m_map;
    MessagePart::Ptr m_rootPart;
    std::vector<std::shared_ptr<PartReplacer>> m_partHandlers;
    QMetaObject::Connection m_insertRows;

    friend class TopLevelMessage;
    friend class ProxyMessagePart;
};
}
#endif /* CRYPTOGRAPHY_MESSAGEMODEL_H */
