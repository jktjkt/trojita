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

#include "MessageComposer.h"
#include <QBuffer>
#include <QMimeData>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#  include <QMimeDatabase>
#else
#  include "mimetypes-qt4/include/QMimeDatabase"
#endif
#include <QUrl>
#include <QUuid>
#include "Common/Application.h"
#include "Composer/ComposerAttachments.h"
#include "Gui/IconLoader.h"
#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/Utils.h"

namespace {
    static QString xTrojitaAttachmentList = QLatin1String("application/x-trojita-attachments-list");
    static QString xTrojitaMessageList = QLatin1String("application/x-trojita-message-list");
    static QString xTrojitaImapPart = QLatin1String("application/x-trojita-imap-part");
}

namespace Composer {

MessageComposer::MessageComposer(Imap::Mailbox::Model *model, QObject *parent) :
    QAbstractListModel(parent), m_model(model), m_shouldPreload(false)
{
}

MessageComposer::~MessageComposer()
{
    qDeleteAll(m_attachments);
}

int MessageComposer::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_attachments.size();
}

QVariant MessageComposer::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= m_attachments.size())
        return QVariant();

    switch (role) {
    case Qt::DisplayRole:
        return m_attachments[index.row()]->caption();
    case Qt::ToolTipRole:
        return m_attachments[index.row()]->tooltip();
    case Qt::DecorationRole:
    {
        // This is more or less copy-pasted from Gui/AttachmentView.cpp. Unfortunately, sharing the implementation
        // is not trivial due to the way how the static libraries are currently built.
        QMimeType mimeType = QMimeDatabase().mimeTypeForName(m_attachments[index.row()]->mimeType());
        if (mimeType.isValid() && !mimeType.isDefault()) {
            return QIcon::fromTheme(mimeType.iconName(), Gui::loadIcon(QLatin1String("mail-attachment")));
        } else {
            return Gui::loadIcon(QLatin1String("mail-attachment"));
        }
    }
    case Imap::Mailbox::RoleAttachmentContentDispositionMode:
        return static_cast<int>(m_attachments[index.row()]->contentDispositionMode());
    }
    return QVariant();
}

Qt::DropActions MessageComposer::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

Qt::ItemFlags MessageComposer::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractListModel::flags(index);

    if (index.isValid()) {
        f |= Qt::ItemIsDragEnabled;
    }
    f |= Qt::ItemIsDropEnabled;
    return f;
}

QMimeData *MessageComposer::mimeData(const QModelIndexList &indexes) const
{
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_6);

    QList<AttachmentItem*> items;
    Q_FOREACH(const QModelIndex &index, indexes) {
        if (index.model() != this || !index.isValid() || index.column() != 0 || index.parent().isValid())
            continue;
        if (index.row() < 0 || index.row() >= m_attachments.size())
            continue;
        items << m_attachments[index.row()];
    }

    if (items.isEmpty())
        return 0;

    stream << items.size();
    Q_FOREACH(const AttachmentItem *attachment, items) {
        attachment->asDroppableMimeData(stream);
    }
    QMimeData *res = new QMimeData();
    res->setData(xTrojitaAttachmentList, encodedData);
    return res;
}

bool MessageComposer::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction)
        return true;

    if (column > 0)
        return false;

    if (!m_model)
        return false;

    Q_UNUSED(row);
    Q_UNUSED(parent);
    // FIXME: would be cool to support attachment reshuffling and to respect the desired drop position


    if (data->hasFormat(xTrojitaAttachmentList)) {
        QByteArray encodedData = data->data(xTrojitaAttachmentList);
        QDataStream stream(&encodedData, QIODevice::ReadOnly);
        return dropAttachmentList(stream);
    } else if (data->hasFormat(xTrojitaMessageList)) {
        QByteArray encodedData = data->data(xTrojitaMessageList);
        QDataStream stream(&encodedData, QIODevice::ReadOnly);
        return dropImapMessage(stream);
    } else if (data->hasFormat(xTrojitaImapPart)) {
        QByteArray encodedData = data->data(xTrojitaImapPart);
        QDataStream stream(&encodedData, QIODevice::ReadOnly);
        return dropImapPart(stream);
    } else if (data->hasUrls()) {
        bool attached = false;
        QList<QUrl> urls = data->urls();
        foreach (const QUrl &url, urls) {
#if QT_VERSION >= QT_VERSION_CHECK(4, 8, 0)
            if (url.isLocalFile()) {
#else
            if (url.scheme() == QLatin1String("file")) {
#endif
                // Careful here -- we definitely don't want the boolean evaluation shortcuts taking effect!
                // At the same time, any file being recognized and attached is enough to "satisfy" the drop
                attached = addFileAttachment(url.path()) || attached;
            }
        }
        return attached;
    } else {
        return false;
    }
}

/** @short Container wrapper which calls qDeleteAll on all items which remain in the list at the time of destruction */
template <typename T>
class WillDeleteAll {
public:
    T d;
    ~WillDeleteAll() {
        qDeleteAll(d);
    }
};

/** @short Handle a drag-and-drop of a list of attachments */
bool MessageComposer::dropAttachmentList(QDataStream &stream)
{
    stream.setVersion(QDataStream::Qt_4_6);
    if (stream.atEnd()) {
        qDebug() << "drag-and-drop: cannot decode data: end of stream";
        return false;
    }
    int num;
    stream >> num;
    if (stream.status() != QDataStream::Ok) {
        qDebug() << "drag-and-drop: stream failed:" << stream.status();
        return false;
    }
    if (num < 0) {
        qDebug() << "drag-and-drop: invalid number of items";
        return false;
    }

    // A crude RAII here; there are many places where the validation might fail even though we have already allocated memory
    WillDeleteAll<QList<AttachmentItem*>> items;

    for (int i = 0; i < num; ++i) {
        int kind = -1;
        stream >> kind;

        switch (kind) {
        case AttachmentItem::ATTACHMENT_IMAP_MESSAGE:
        {
            QString mailbox;
            uint uidValidity;
            QList<uint> uids;
            stream >> mailbox >> uidValidity >> uids;
            if (!validateDropImapMessage(stream, mailbox, uidValidity, uids))
                return false;
            if (uids.size() != 1) {
                qDebug() << "drag-and-drop: malformed data for a single message in a mixed list: too many UIDs";
                return false;
            }
            try {
                items.d << new ImapMessageAttachmentItem(m_model, mailbox, uidValidity, uids.front());
            } catch (Imap::UnknownMessageIndex &) {
                return false;
            }

            break;
        }

        case AttachmentItem::ATTACHMENT_IMAP_PART:
        {
            QString mailbox;
            uint uidValidity;
            uint uid;
            QString trojitaPath;
            if (!validateDropImapPart(stream, mailbox, uidValidity, uid, trojitaPath))
                return false;
            try {
                items.d << new ImapPartAttachmentItem(m_model, mailbox, uidValidity, uid, trojitaPath);
            } catch (Imap::UnknownMessageIndex &) {
                return false;
            }

            break;
        }

        case AttachmentItem::ATTACHMENT_FILE:
        {
            QString fileName;
            stream >> fileName;
            items.d << new FileAttachmentItem(fileName);
            break;
        }

        default:
            qDebug() << "drag-and-drop: invalid kind of attachment";
            return false;
        }
    }

    beginInsertRows(QModelIndex(), m_attachments.size(), m_attachments.size() + items.d.size() - 1);
    Q_FOREACH(AttachmentItem *attachment, items.d) {
        if (m_shouldPreload)
            attachment->preload();
        m_attachments << attachment;
    }
    items.d.clear();
    endInsertRows();

    return true;
}

/** @short Check that the data representing a list of messages is correct */
bool MessageComposer::validateDropImapMessage(QDataStream &stream, QString &mailbox, uint &uidValidity, QList<uint> &uids) const
{
    if (stream.status() != QDataStream::Ok) {
        qDebug() << "drag-and-drop: stream failed:" << stream.status();
        return false;
    }

    Imap::Mailbox::TreeItemMailbox *mboxPtr = m_model->findMailboxByName(mailbox);
    if (!mboxPtr) {
        qDebug() << "drag-and-drop: mailbox not found";
        return false;
    }

    if (uids.size() < 1) {
        qDebug() << "drag-and-drop: no UIDs passed";
        return false;
    }
    if (!uidValidity) {
        qDebug() << "drag-and-drop: invalid UIDVALIDITY";
        return false;
    }

    return true;
}

/** @short Handle a drag-and-drop of a list of messages */
bool MessageComposer::dropImapMessage(QDataStream &stream)
{
    stream.setVersion(QDataStream::Qt_4_6);
    if (stream.atEnd()) {
        qDebug() << "drag-and-drop: cannot decode data: end of stream";
        return false;
    }
    QString mailbox;
    uint uidValidity;
    QList<uint> uids;
    stream >> mailbox >> uidValidity >> uids;
    if (!validateDropImapMessage(stream, mailbox, uidValidity, uids))
        return false;
    if (!stream.atEnd()) {
        qDebug() << "drag-and-drop: cannot decode data: too much data";
        return false;
    }

    WillDeleteAll<QList<AttachmentItem*>> items;
    Q_FOREACH(const uint uid, uids) {
        try {
            items.d << new ImapMessageAttachmentItem(m_model, mailbox, uidValidity, uid);
        } catch (Imap::UnknownMessageIndex &) {
            return false;
        }
        items.d.last()->setContentDispositionMode(CDN_INLINE);
    }
    beginInsertRows(QModelIndex(), m_attachments.size(), m_attachments.size() + uids.size() - 1);
    Q_FOREACH(AttachmentItem *attachment, items.d) {
        if (m_shouldPreload)
            attachment->preload();
        m_attachments << attachment;
    }
    items.d.clear();
    endInsertRows();

    return true;
}

/** @short Check that the data representing a single message part are correct */
bool MessageComposer::validateDropImapPart(QDataStream &stream, QString &mailbox, uint &uidValidity, uint &uid, QString &trojitaPath) const
{
    stream >> mailbox >> uidValidity >> uid >> trojitaPath;
    if (stream.status() != QDataStream::Ok) {
        qDebug() << "drag-and-drop: stream failed:" << stream.status();
        return false;
    }
    Imap::Mailbox::TreeItemMailbox *mboxPtr = m_model->findMailboxByName(mailbox);
    if (!mboxPtr) {
        qDebug() << "drag-and-drop: mailbox not found";
        return false;
    }

    if (!uidValidity || !uid || trojitaPath.isEmpty()) {
        qDebug() << "drag-and-drop: invalid data";
        return false;
    }
    return true;
}

/** @short Handle a drag-adn-drop of a list of message parts */
bool MessageComposer::dropImapPart(QDataStream &stream)
{
    stream.setVersion(QDataStream::Qt_4_6);
    if (stream.atEnd()) {
        qDebug() << "drag-and-drop: cannot decode data: end of stream";
        return false;
    }
    QString mailbox;
    uint uidValidity;
    uint uid;
    QString trojitaPath;
    if (!validateDropImapPart(stream, mailbox, uidValidity, uid, trojitaPath))
        return false;
    if (!stream.atEnd()) {
        qDebug() << "drag-and-drop: cannot decode data: too much data";
        return false;
    }

    AttachmentItem *item;
    try {
        item = new ImapPartAttachmentItem(m_model, mailbox, uidValidity, uid, trojitaPath);
    } catch (Imap::UnknownMessageIndex &) {
        return false;
    }

    beginInsertRows(QModelIndex(), m_attachments.size(), m_attachments.size());
    m_attachments << item;
    if (m_shouldPreload)
        m_attachments.back()->preload();
    endInsertRows();

    return true;
}

QStringList MessageComposer::mimeTypes() const
{
    return QStringList() << xTrojitaMessageList << xTrojitaImapPart << xTrojitaAttachmentList << QLatin1String("text/uri-list");
}

void MessageComposer::setFrom(const Imap::Message::MailAddress &from)
{
    m_from = from;
}

void MessageComposer::setRecipients(const QList<QPair<Composer::RecipientKind, Imap::Message::MailAddress> > &recipients)
{
    m_recipients = recipients;
}

/** @short Set the value for the In-Reply-To header as per RFC 5322, section 3.6.4

The expected values to be passed here do *not* contain the angle brackets. This is in accordance with
the very last sentence of that section which says that the angle brackets are not part of the msg-id.
*/
void MessageComposer::setInReplyTo(const QList<QByteArray> &inReplyTo)
{
    m_inReplyTo = inReplyTo;
}

/** @short Set the value for the References header as per RFC 5322, section 3.6.4

@see setInReplyTo
*/
void MessageComposer::setReferences(const QList<QByteArray> &references)
{
    m_references = references;
}

void MessageComposer::setTimestamp(const QDateTime &timestamp)
{
    m_timestamp = timestamp;
}

void MessageComposer::setSubject(const QString &subject)
{
    m_subject = subject;
}

void MessageComposer::setOrganization(const QString &organization)
{
    m_organization = organization;
}

void MessageComposer::setText(const QString &text)
{
    m_text = text;
}

bool MessageComposer::isReadyForSerialization() const
{
    Q_FOREACH(const AttachmentItem *attachment, m_attachments) {
        if (!attachment->isAvailableLocally())
            return false;
    }
    return true;
}

QByteArray MessageComposer::generateMessageId(const Imap::Message::MailAddress &sender)
{
    if (sender.host.isEmpty()) {
        // There's no usable domain, let's just bail out of here
        return QByteArray();
    }
    return QUuid::createUuid()
#if QT_VERSION >= 0x040800
            .toByteArray()
#else
            .toString().toAscii()
#endif
            .replace("{", "").replace("}", "") + "@" + sender.host.toUtf8();
}

/** @short Generate a random enough MIME boundary */
QByteArray MessageComposer::generateMimeBoundary()
{
    // Usage of "=_" is recommended by RFC2045 as it's guaranteed to never occur in a quoted-printable source
    return QByteArray("trojita=_") + QUuid::createUuid()
#if QT_VERSION >= 0x040800
            .toByteArray()
#else
            .toString().toAscii()
#endif
            .replace("{", "").replace("}", "");
}

QByteArray MessageComposer::encodeHeaderField(const QString &text)
{
    /* This encodes an "unstructured" header field */
    return Imap::encodeRFC2047StringWithAsciiPrefix(text);
}

namespace {

/** @short Write a list of recipients into an output buffer */
static void processListOfRecipientsIntoHeader(const QByteArray &prefix, const QList<QByteArray> &addresses, QByteArray &out)
{
    // Qt and STL are different, it looks like we cannot easily use something as simple as the ostream_iterator here :(
    if (!addresses.isEmpty()) {
        out.append(prefix);
        for (int i = 0; i < addresses.size() - 1; ++i)
            out.append(addresses[i]).append(",\r\n ");
        out.append(addresses.last()).append("\r\n");
    }
}

}

void MessageComposer::writeCommonMessageBeginning(QIODevice *target, const QByteArray boundary) const
{
    // The From header
    target->write(QByteArray("From: ").append(m_from.asMailHeader()).append("\r\n"));

    // All recipients
    // Got to group the headers so that both of (To, Cc) are present at most once
    QList<QByteArray> rcptTo, rcptCc;
    for (auto it = m_recipients.begin(); it != m_recipients.end(); ++it) {
        switch(it->first) {
        case Composer::ADDRESS_TO:
            rcptTo << it->second.asMailHeader();
            break;
        case Composer::ADDRESS_CC:
            rcptCc << it->second.asMailHeader();
            break;
        case Composer::ADDRESS_BCC:
            break;
        case Composer::ADDRESS_FROM:
        case Composer::ADDRESS_SENDER:
        case Composer::ADDRESS_REPLY_TO:
            // These should never ever be produced by Trojita for now
            Q_ASSERT(false);
            break;
        }
    }

    QByteArray recipientHeaders;
    processListOfRecipientsIntoHeader("To: ", rcptTo, recipientHeaders);
    processListOfRecipientsIntoHeader("Cc: ", rcptCc, recipientHeaders);
    target->write(recipientHeaders);

    // Other message metadata
    target->write(encodeHeaderField(QLatin1String("Subject: ") + m_subject).append("\r\n").
            append("Date: ").append(Imap::dateTimeToRfc2822(m_timestamp)).append("\r\n").
            append("User-Agent: ").append(
                QString::fromUtf8("Trojita/%1; %2")
                .arg(Common::Application::version, Imap::Mailbox::systemPlatformVersion()).toUtf8()
                ).append("\r\n").
            append("MIME-Version: 1.0\r\n"));
    QByteArray messageId = generateMessageId(m_from);
    if (!messageId.isEmpty()) {
        target->write(QByteArray("Message-ID: <").append(messageId).append(">\r\n"));
    }
    writeHeaderWithMsgIds(target, QByteArray("In-Reply-To"), m_inReplyTo);
    writeHeaderWithMsgIds(target, QByteArray("References"), m_references);
    if (!m_organization.isEmpty()) {
        target->write(encodeHeaderField(QLatin1String("Organization: ") + m_organization).append("\r\n"));
    }

    // Headers depending on actual message body data
    if (!m_attachments.isEmpty()) {
        target->write("Content-Type: multipart/mixed;\r\n\tboundary=\"" + boundary + "\"\r\n"
                      "\r\nThis is a multipart/mixed message in MIME format.\r\n\r\n"
                      "--" + boundary + "\r\n");
    }

    target->write("Content-Type: text/plain; charset=utf-8; format=flowed\r\n"
                  "Content-Transfer-Encoding: quoted-printable\r\n"
                  "\r\n");
    target->write(Imap::quotedPrintableEncode(Imap::wrapFormatFlowed(m_text).toUtf8()));
}

/** @short Write a header consisting of a list of message-ids

Empty headers will not be produced, and the result is wrapped at an appropriate length.

The header name must not contain the colon, it is added automatically.
*/
void MessageComposer::writeHeaderWithMsgIds(QIODevice *target, const QByteArray &headerName,
                                            const QList<QByteArray> &messageIds) const
{
    if (messageIds.isEmpty())
        return;

    target->write(headerName + ":");
    int charCount = headerName.length() + 1;
    for (int i = 0; i < messageIds.size(); ++i) {
        // Wrapping shall happen at 78 columns, three bytes are eaten by "space < >"
        if (i != 0 && charCount != 0 && charCount + messageIds[i].length() > 78 - 3) {
            // got to wrap the header to respect a reasonably small line size
            charCount = 0;
            target->write("\r\n");
        }
        // and now just append one more item
        target->write(" <" + messageIds[i] + ">");
        charCount += messageIds[i].length() + 3;
    }
    target->write("\r\n");
}

bool MessageComposer::writeAttachmentHeader(QIODevice *target, QString *errorMessage, const AttachmentItem *attachment, const QByteArray &boundary) const
{
    if (!attachment->isAvailableLocally() && attachment->imapUrl().isEmpty()) {
        *errorMessage = tr("Attachment %1 is not available").arg(attachment->caption());
        return false;
    }
    target->write("\r\n--" + boundary + "\r\n"
                  "Content-Type: " + attachment->mimeType() + "\r\n");
    target->write(attachment->contentDispositionHeader());

    switch (attachment->suggestedCTE()) {
    case AttachmentItem::CTE_BASE64:
        target->write("Content-Transfer-Encoding: base64\r\n");
        break;
    case AttachmentItem::CTE_7BIT:
        target->write("Content-Transfer-Encoding: 7bit\r\n");
        break;
    case AttachmentItem::CTE_8BIT:
        target->write("Content-Transfer-Encoding: 8bit\r\n");
        break;
    case AttachmentItem::CTE_BINARY:
        target->write("Content-Transfer-Encoding: binary\r\n");
        break;
    }

    target->write("\r\n");
    return true;
}

bool MessageComposer::writeAttachmentBody(QIODevice *target, QString *errorMessage, const AttachmentItem *attachment) const
{
    if (!attachment->isAvailableLocally()) {
        *errorMessage = tr("Attachment %1 is not available").arg(attachment->caption());
        return false;
    }
    QSharedPointer<QIODevice> io = attachment->rawData();
    if (!io) {
        *errorMessage = tr("Attachment %1 disappeared").arg(attachment->caption());
        return false;
    }
    while (!io->atEnd()) {
        switch (attachment->suggestedCTE()) {
        case AttachmentItem::CTE_BASE64:
            // Base64 maps 6bit chunks into a single byte. Output shall have no more than 76 characters per line
            // (not counting the CRLF pair).
            target->write(io->read(76*6/8).toBase64() + "\r\n");
            break;
        default:
            target->write(io->readAll());
        }
    }
    return true;
}

bool MessageComposer::asRawMessage(QIODevice *target, QString *errorMessage) const
{
    // We don't bother with checking that our boundary is not present in the individual parts. That's arguably wrong,
    // but we don't have much choice if we ever plan to use CATENATE.  It also looks like this is exactly how other MUAs
    // oeprate as well, so let's just join the universal dontcareism here.
    QByteArray boundary(generateMimeBoundary());

    writeCommonMessageBeginning(target, boundary);

    if (!m_attachments.isEmpty()) {
        Q_FOREACH(const AttachmentItem *attachment, m_attachments) {
            if (!writeAttachmentHeader(target, errorMessage, attachment, boundary))
                return false;
            if (!writeAttachmentBody(target, errorMessage, attachment))
                return false;
        }
        target->write("\r\n--" + boundary + "--\r\n");
    }
    return true;
}

bool MessageComposer::asCatenateData(QList<Imap::Mailbox::CatenatePair> &target, QString *errorMessage) const
{
    using namespace Imap::Mailbox;
    target.clear();
    QByteArray boundary(generateMimeBoundary());
    target.append(qMakePair(CATENATE_TEXT, QByteArray()));

    // write the initial data
    {
        QBuffer io(&target.back().second);
        io.open(QIODevice::ReadWrite);
        writeCommonMessageBeginning(&io, boundary);
    }

    if (!m_attachments.isEmpty()) {
        Q_FOREACH(const AttachmentItem *attachment, m_attachments) {
            if (target.back().first != CATENATE_TEXT) {
                target.append(qMakePair(CATENATE_TEXT, QByteArray()));
            }
            QBuffer io(&target.back().second);
            io.open(QIODevice::Append);

            if (!writeAttachmentHeader(&io, errorMessage, attachment, boundary))
                return false;

            QByteArray url = attachment->imapUrl();
            if (url.isEmpty()) {
                // Cannot use CATENATE here
                if (!writeAttachmentBody(&io, errorMessage, attachment))
                    return false;
            } else {
                target.append(qMakePair(CATENATE_URL, url));
            }
        }
        if (target.back().first != CATENATE_TEXT) {
            target.append(qMakePair(CATENATE_TEXT, QByteArray()));
        }
        QBuffer io(&target.back().second);
        io.open(QIODevice::Append);
        io.write("\r\n--" + boundary + "--\r\n");
    }
    return true;
}

QDateTime MessageComposer::timestamp() const
{
    return m_timestamp;
}

QList<QByteArray> MessageComposer::inReplyTo() const
{
    return m_inReplyTo;
}

QList<QByteArray> MessageComposer::references() const
{
    return m_references;
}

QByteArray MessageComposer::rawFromAddress() const
{
    return m_from.asSMTPMailbox();
}

QList<QByteArray> MessageComposer::rawRecipientAddresses() const
{
    QList<QByteArray> res;

    for (auto it = m_recipients.begin(); it != m_recipients.end(); ++it) {
        res << it->second.asSMTPMailbox();
    }

    return res;
}

bool MessageComposer::addFileAttachment(const QString &path)
{
    beginInsertRows(QModelIndex(), m_attachments.size(), m_attachments.size());
    QScopedPointer<AttachmentItem> attachment(new FileAttachmentItem(path));
    if (!attachment->isAvailableLocally())
        return false;
    if (m_shouldPreload)
        attachment->preload();
    m_attachments << attachment.take();
    endInsertRows();
    return true;
}

void MessageComposer::removeAttachment(const QModelIndex &index)
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= m_attachments.size())
        return;

    beginRemoveRows(QModelIndex(), index.row(), index.row());
    delete m_attachments.takeAt(index.row());
    endRemoveRows();
}

void MessageComposer::setAttachmentName(const QModelIndex &index, const QString &newName)
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= m_attachments.size())
        return;

    if (m_attachments[index.row()]->setPreferredFileName(newName))
        emit dataChanged(index, index);
}

void MessageComposer::setAttachmentContentDisposition(const QModelIndex &index, const ContentDisposition disposition)
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= m_attachments.size())
        return;

    if (m_attachments[index.row()]->setContentDispositionMode(disposition))
        emit dataChanged(index, index);
}

void MessageComposer::setPreloadEnabled(const bool preload)
{
    m_shouldPreload = preload;
}

void MessageComposer::setReplyingToMessage(const QModelIndex &index)
{
    m_replyingTo = index;
}

QModelIndex MessageComposer::replyingToMessage() const
{
    return m_replyingTo;
}

}
