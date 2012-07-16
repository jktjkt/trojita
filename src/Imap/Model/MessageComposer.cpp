#include "MessageComposer.h"
#include <QCoreApplication>
#include <QMimeData>
#include <QUuid>
#include "Imap/Encoders.h"
#include "Imap/Model/ComposerAttachments.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/Utils.h"

namespace Imap {
namespace Mailbox {

MessageComposer::MessageComposer(Model *model, QObject *parent) :
    QAbstractListModel(parent), m_model(model)
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
    }
    return QVariant();
}

Qt::DropActions MessageComposer::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
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

bool MessageComposer::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction)
        return true;

    if (!data->hasFormat(QLatin1String("application/x-trojita-message-list")))
        return false;

    if (column > 0)
        return false;

    if (!m_model)
        return false;

    QByteArray encodedData = data->data("application/x-trojita-message-list");
    QDataStream stream(&encodedData, QIODevice::ReadOnly);

    Q_ASSERT(!stream.atEnd());
    QString mailbox;
    uint uidValidity;
    QList<uint> uids;
    stream >> mailbox >> uidValidity >> uids;
    Q_ASSERT(stream.atEnd());

    TreeItemMailbox *mboxPtr = m_model->findMailboxByName(mailbox);
    if (!mboxPtr) {
        qDebug() << "drag-and-drop: mailbox not found";
        return false;
    }

    if (uids.size() < 1)
        return false;

    beginInsertRows(QModelIndex(), m_attachments.size(), m_attachments.size() + uids.size() - 1);
    Q_FOREACH(const uint uid, uids) {
        m_attachments << new ImapMessageAttachmentItem(m_model, mailbox, uidValidity, uid);
    }
    endInsertRows();

    return true;
}

QStringList MessageComposer::mimeTypes() const
{
    return QStringList() << QLatin1String("application/x-trojita-message-list");
}

void MessageComposer::setFrom(const Message::MailAddress &from)
{
    m_from = from;
}

void MessageComposer::setRecipients(const QList<QPair<RecipientKind, Message::MailAddress> > &recipients)
{
    m_recipients = recipients;
}

void MessageComposer::setInReplyTo(const QByteArray &inReplyTo)
{
    m_inReplyTo = inReplyTo;
}

void MessageComposer::setTimestamp(const QDateTime &timestamp)
{
    m_timestamp = timestamp;
}

void MessageComposer::setSubject(const QString &subject)
{
    m_subject = subject;
}

void MessageComposer::setText(const QString &text)
{
    m_text = text;
}

bool MessageComposer::isReadyForSerialization() const
{
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
            .replace("{", "").replace("}", "") + "@" + sender.host.toAscii();
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
    /* FIXME: Don't apply RFC2047 if it isn't needed */
    return Imap::encodeRFC2047String(text);
}

QByteArray MessageComposer::asRawMessage() const
{
    QByteArray res;

    // The From header
    res.append("From: ").append(m_from.asMailHeader()).append("\r\n");

    // All recipients
    QByteArray recipientHeaders;
    for (QList<QPair<RecipientKind,Imap::Message::MailAddress> >::const_iterator it = m_recipients.begin();
         it != m_recipients.end(); ++it) {
        switch(it->first) {
        case Recipient_To:
            recipientHeaders.append("To: ").append(it->second.asMailHeader()).append("\r\n");
            break;
        case Recipient_Cc:
            recipientHeaders.append("Cc: ").append(it->second.asMailHeader()).append("\r\n");
            break;
        case Recipient_Bcc:
            break;
        }
    }
    res.append(recipientHeaders);

    // Other message metadata
    res.append("Subject: ").append(encodeHeaderField(m_subject)).append("\r\n");
    res.append("Date: ").append(Imap::dateTimeToRfc2822(m_timestamp)).append("\r\n");
    res.append("User-Agent: ").append(
                QString::fromAscii("%1/%2; %3")
                .arg(qApp->applicationName(), qApp->applicationVersion(), Imap::Mailbox::systemPlatformVersion()).toAscii()
    ).append("\r\n");
    res.append("MIME-Version: 1.0\r\n");
    QByteArray messageId = generateMessageId(m_from);
    if (!messageId.isEmpty()) {
        res.append("Message-ID: <").append(messageId).append(">\r\n");
    }
    if (!m_inReplyTo.isEmpty()) {
        res.append("In-Reply-To: ").append(m_inReplyTo).append("\r\n");
    }

    const bool hasAttachments = !m_attachments.isEmpty();

    // We don't bother with checking that our boundary is not present in the individual parts. That's arguably wrong,
    // but we don't have much choice if we ever plan to use CATENATE.  It also looks like this is exactly how other MUAs
    // oeprate as well, so let's just join the universal dontcareism here.
    QByteArray boundary(generateMimeBoundary());

    if (hasAttachments) {
        res.append("Content-Type: multipart/mixed;\r\n\tboundary=\"" + boundary + "\"\r\n");
        res.append("\r\nThis is a multipart/mixed message in MIME format.\r\n\r\n");
        res.append("--" + boundary + "\r\n");
    }

    res.append("Content-Type: text/plain; charset=utf-8\r\n"
               "Content-Transfer-Encoding: quoted-printable\r\n");
    res.append("\r\n");
    res.append(Imap::quotedPrintableEncode(m_text.toUtf8()));

    if (hasAttachments) {
        Q_FOREACH(const AttachmentItem *attachment, m_attachments) {
            // FIXME: this assert can fail very, *very* easily when it comes to IMAP-based attachments...
            Q_ASSERT(attachment->isAvailable());
            res.append("\r\n--" + boundary + "\r\n");
            res.append("Content-Type: " + attachment->mimeType() + "\r\n");
            res.append(attachment->contentDispositionHeader());

            AttachmentItem::ContentTransferEncoding cte = attachment->suggestedCTE();
            switch (cte) {
            case AttachmentItem::CTE_BASE64:
                res.append("Content-Transfer-Encoding: base64\r\n");
                break;
            case AttachmentItem::CTE_7BIT:
                res.append("Content-Transfer-Encoding: 7bit\r\n");
                break;
            case AttachmentItem::CTE_8BIT:
                res.append("Content-Transfer-Encoding: 8bit\r\n");
                break;
            case AttachmentItem::CTE_BINARY:
                res.append("Content-Transfer-Encoding: binary\r\n");
                break;
            }

            res.append("\r\n");

            QSharedPointer<QIODevice> io = attachment->rawData();
            while (!io->atEnd()) {
                switch (cte) {
                case AttachmentItem::CTE_BASE64:
                    // Base64 maps 6bit chunks into a single byte. Output shall have no more than 76 characters per line
                    // (not counting the CRLF pair).
                    res.append(io->read(76*6/8).toBase64() + "\r\n");
                    break;
                default:
                    res.append(io->readAll());
                }
            }
        }
        res.append("\r\n--" + boundary + "--\r\n");
    }
    return res;
}

QDateTime MessageComposer::timestamp() const
{
    return m_timestamp;
}

QByteArray MessageComposer::rawFromAddress() const
{
    return m_from.asSMTPMailbox();
}

QList<QByteArray> MessageComposer::rawRecipientAddresses() const
{
    QList<QByteArray> res;

    for (QList<QPair<RecipientKind,Imap::Message::MailAddress> >::const_iterator it = m_recipients.begin();
         it != m_recipients.end(); ++it) {
        res << it->second.asSMTPMailbox();
    }

    return res;
}

void MessageComposer::addFileAttachment(const QString &path)
{
    beginInsertRows(QModelIndex(), m_attachments.size(), m_attachments.size());
    m_attachments << new FileAttachmentItem(path);
    endInsertRows();
}

}
}
