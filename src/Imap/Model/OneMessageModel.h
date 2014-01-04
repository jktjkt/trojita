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

#ifndef IMAP_MODEL_ONEMESSAGEMODEL_H
#define IMAP_MODEL_ONEMESSAGEMODEL_H

#include <QDateTime>
#include <QPersistentModelIndex>
#include <QUrl>
#include <QVariant>

class KDescendantsProxyModel;

/** @short Namespace for IMAP interaction */
namespace Imap
{

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox
{

class Model;
class SubtreeModelOfModel;

/** @short Publish contents of a selected message */
class OneMessageModel: public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(OneMessageModel)

    Q_PROPERTY(QString subject READ subject NOTIFY envelopeChanged)
    Q_PROPERTY(QDateTime date READ date NOTIFY envelopeChanged)
    Q_PROPERTY(QDateTime receivedDate READ receivedDate NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList from READ from NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList to READ to NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList cc READ cc NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList bcc READ bcc NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList sender READ sender NOTIFY envelopeChanged)
    Q_PROPERTY(QVariantList replyTo READ replyTo NOTIFY envelopeChanged)
    Q_PROPERTY(QByteArray inReplyTo READ inReplyTo NOTIFY envelopeChanged)
    Q_PROPERTY(QByteArray messageId READ messageId NOTIFY envelopeChanged)
    Q_PROPERTY(bool isMarkedDeleted READ isMarkedDeleted WRITE setMarkedDeleted NOTIFY flagsChanged)
    Q_PROPERTY(bool isMarkedRead READ isMarkedRead WRITE setMarkedRead NOTIFY flagsChanged)
    Q_PROPERTY(bool isMarkedForwarded READ isMarkedForwarded NOTIFY flagsChanged)
    Q_PROPERTY(bool isMarkedReplied READ isMarkedReplied NOTIFY flagsChanged)
    Q_PROPERTY(bool isMarkedRecent READ isMarkedRecent NOTIFY flagsChanged)
    Q_PROPERTY(QUrl mainPartUrl READ mainPartUrl NOTIFY mainPartUrlChanged)
    Q_PROPERTY(QObject* attachmentsModel READ attachmentsModel NOTIFY mainPartUrlChanged)
    Q_PROPERTY(bool hasValidIndex READ hasValidIndex NOTIFY envelopeChanged)

public:
    explicit OneMessageModel(Model *model);

    QString subject() const;
    QDateTime date() const;
    QDateTime receivedDate() const;
    QVariantList from() const;
    QVariantList to() const;
    QVariantList cc() const;
    QVariantList bcc() const;
    QVariantList sender() const;
    QVariantList replyTo() const;
    QByteArray inReplyTo() const;
    QByteArray messageId() const;
    bool isMarkedDeleted() const;
    void setMarkedDeleted(const bool marked);
    bool isMarkedRead() const;
    void setMarkedRead(const bool marked);
    bool isMarkedForwarded() const;
    bool isMarkedReplied() const;
    bool isMarkedRecent() const;
    QUrl mainPartUrl() const;
    QObject *attachmentsModel() const;
    bool hasValidIndex() const;

    Q_INVOKABLE void setMessage(const QString &mailbox, const uint uid);
    void setMessage(const QModelIndex &message);

signals:
    void envelopeChanged();
    void flagsChanged();
    void mainPartUrlChanged();

private slots:
    void handleModelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

private:
    QPersistentModelIndex m_message;
    SubtreeModelOfModel *m_subtree;
    KDescendantsProxyModel *m_flatteningModel;
    QUrl m_mainPartUrl;
};

}

}

#endif /* IMAP_MODEL_ONEMESSAGEMODEL_H */
