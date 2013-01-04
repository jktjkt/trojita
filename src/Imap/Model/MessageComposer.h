/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

#ifndef IMAP_MESSAGECOMPOSER_H
#define IMAP_MESSAGECOMPOSER_H

#include <QAbstractListModel>
#include <QPointer>

#include "Composer/Recipients.h"
#include "Imap/Model/CatenateData.h"
#include "Imap/Parser/Message.h"

namespace Imap {
namespace Mailbox {

class Model;

class AttachmentItem;

/** @short Model storing individual parts of a composed message */
class MessageComposer : public QAbstractListModel
{
    Q_OBJECT
public:

    explicit MessageComposer(Model *model, QObject *parent = 0);
    ~MessageComposer();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    virtual Qt::DropActions supportedDropActions() const;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const;
    virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
    virtual QStringList mimeTypes() const;
    virtual QMimeData *mimeData(const QModelIndexList &indexes) const;

    void setFrom(const Message::MailAddress &from);
    void setRecipients(const QList<QPair<Composer::RecipientKind, Message::MailAddress> > &recipients);
    void setInReplyTo(const QList<QByteArray> &inReplyTo);
    void setReferences(const QList<QByteArray> &references);
    void setTimestamp(const QDateTime &timestamp);
    void setSubject(const QString &subject);
    void setOrganization(const QString &organization);
    void setText(const QString &text);

    bool isReadyForSerialization() const;
    bool asRawMessage(QIODevice *target, QString *errorMessage) const;
    bool asCatenateData(QList<CatenatePair> &target, QString *errorMessage) const;

    QDateTime timestamp() const;
    QByteArray rawFromAddress() const;
    QList<QByteArray> rawRecipientAddresses() const;

    bool addFileAttachment(const QString &path);
    void removeAttachment(const QModelIndex &index);

    void setPreloadEnabled(const bool preload);

private:
    static QByteArray generateMessageId(const Imap::Message::MailAddress &sender);
    static QByteArray encodeHeaderField(const QString &text);
    static QByteArray generateMimeBoundary();

    void writeCommonMessageBeginning(QIODevice *target, const QByteArray boundary) const;
    bool writeAttachmentHeader(QIODevice *target, QString *errorMessage, const AttachmentItem *attachment, const QByteArray &boundary) const;
    bool writeAttachmentBody(QIODevice *target, QString *errorMessage, const AttachmentItem *attachment) const;

    void writeHeaderWithMsgIds(QIODevice *target, const QByteArray &headerName, const QList<QByteArray> &messageIds) const;

    bool validateDropImapMessage(QDataStream &stream, QString &mailbox, uint &uidValidity, QList<uint> &uids) const;
    bool validateDropImapPart(QDataStream &stream, QString &mailbox, uint &uidValidity, uint &uid, QString &partId, QString &trojitaPath) const;
    bool dropImapMessage(QDataStream &stream);
    bool dropImapPart(QDataStream &stream);
    bool dropAttachmentList(QDataStream &stream);

    Message::MailAddress m_from;
    QList<QPair<Composer::RecipientKind, Message::MailAddress> > m_recipients;
    QList<QByteArray> m_inReplyTo;
    QList<QByteArray> m_references;
    QDateTime m_timestamp;
    QString m_subject;
    QString m_organization;
    QString m_text;

    QList<AttachmentItem *> m_attachments;
    QPointer<Model> m_model;
    bool m_shouldPreload;
};

}
}

#endif // IMAP_MESSAGECOMPOSER_H
