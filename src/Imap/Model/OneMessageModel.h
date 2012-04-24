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

#ifndef IMAP_MODEL_ONEMESSAGEMODEL_H
#define IMAP_MODEL_ONEMESSAGEMODEL_H

#include <QDateTime>
#include <QPersistentModelIndex>
#include <QVariant>

/** @short Namespace for IMAP interaction */
namespace Imap
{

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox
{

class Model;
class SubtreeModel;

/** @short Publish contents of a selected message */
class OneMessageModel: public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(OneMessageModel)

    Q_PROPERTY(QString subject READ subject NOTIFY envelopeChanged)
    Q_PROPERTY(QDateTime date READ date NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList from READ from NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList to READ to NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList cc READ cc NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList bcc READ bcc NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList sender READ sender NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList replyTo READ replyTo NOTIFY envelopeChanged)
    Q_PROPERTY(QByteArray inReplyTo READ inReplyTo NOTIFY envelopeChanged)
    Q_PROPERTY(QByteArray messageId READ messageId NOTIFY envelopeChanged)
    Q_PROPERTY(bool isMarkedDeleted READ isMarkedDeleted NOTIFY envelopeChanged)
    Q_PROPERTY(bool isMarkedRead READ isMarkedRead NOTIFY envelopeChanged)
    Q_PROPERTY(bool isMarkedForwarded READ isMarkedForwarded NOTIFY envelopeChanged)
    Q_PROPERTY(bool isMarkedReplied READ isMarkedReplied NOTIFY envelopeChanged)
    Q_PROPERTY(bool isMarkedRecent READ isMarkedRecent NOTIFY envelopeChanged)

public:
    OneMessageModel(Model *model);

    QString subject() const;
    QDateTime date() const;
    QVariantList from() const;
    QVariantList to() const;
    QVariantList cc() const;
    QVariantList bcc() const;
    QVariantList sender() const;
    QVariantList replyTo() const;
    QByteArray inReplyTo() const;
    QByteArray messageId() const;
    bool isMarkedDeleted() const;
    bool isMarkedRead() const;
    bool isMarkedForwarded() const;
    bool isMarkedReplied() const;
    bool isMarkedRecent() const;

    Q_INVOKABLE void setMessage(const QString &mailbox, const uint uid);
    void setMessage(const QModelIndex &message);

signals:
    void envelopeChanged();

private:
    QPersistentModelIndex m_message;
    SubtreeModel *m_subtree;
};

}

}

#endif /* IMAP_MODEL_ONEMESSAGEMODEL_H */
