#include "MessageComposer.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QUuid>
#include "Imap/Encoders.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/Utils.h"

namespace Imap {
namespace Mailbox {

MessageComposer::MessageComposer(QObject *parent) :
    QAbstractListModel(parent)
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
            Q_ASSERT(attachment->isAvailable());
            res.append("\r\n--" + boundary + "\r\n");
            res.append("Content-Type: " + attachment->mimeType() + "\r\n");
            res.append(attachment->contentDispositionHeader());
            res.append("Content-Transfer-Encoding: base64\r\n"
                       "\r\n");
            attachment->rawData()->seek(0);
            while (!attachment->rawData()->atEnd()) {
                // Base64 maps 6bit chunks into a single byte. Output shall have no more than 76 characters per line
                // (not counting the CRLF pair).
                res.append(attachment->rawData()->read(76*6/8).toBase64() + "\r\n");
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

AttachmentItem::~AttachmentItem()
{
}

FileAttachmentItem::FileAttachmentItem(const QString &fileName):
    fileName(fileName), m_io(0)
{
}

FileAttachmentItem::~FileAttachmentItem()
{
    delete m_io;
}

QString FileAttachmentItem::caption() const
{
    return fileName;
}

QString FileAttachmentItem::tooltip() const
{
    QFileInfo f(fileName);

    if (!f.exists())
        return MessageComposer::tr("File does not exist");

    if (!f.isReadable())
        return MessageComposer::tr("File is not readable");

    return MessageComposer::tr("%1: %2, %3").arg(fileName, QString::fromAscii(mimeType()), QString::number(f.size()));
}

bool FileAttachmentItem::isAvailable() const
{
    return QFileInfo(fileName).isReadable();
}

QIODevice *FileAttachmentItem::rawData() const
{
    if (!m_io) {
        m_io = new QFile(fileName);
        m_io->open(QIODevice::ReadOnly);
    }
    return m_io;
}

QByteArray FileAttachmentItem::mimeType() const
{
    if (!m_cachedMime.isEmpty())
        return m_cachedMime;

    // At first, try to guess through the xdg-mime lookup
    QProcess p;
    p.start(QLatin1String("xdg-mime"), QStringList() << QLatin1String("query") << QLatin1String("filetype") << fileName);
    p.waitForFinished();
    m_cachedMime = p.readAllStandardOutput();

    // If the lookup fails, consult a list of hard-coded extensions (nope, I'm not going to bundle mime.types with Trojita)
    if (m_cachedMime.isEmpty()) {
        QFileInfo info(fileName);

        QMap<QByteArray,QByteArray> knownTypes;
        if (knownTypes.isEmpty()) {
            knownTypes["txt"] = "text/plain";
            knownTypes["jpg"] = "image/jpeg";
            knownTypes["jpeg"] = "image/jpeg";
            knownTypes["png"] = "image/png";
        }
        QMap<QByteArray,QByteArray>::const_iterator it = knownTypes.constFind(info.suffix().toLower().toLocal8Bit());

        if (it == knownTypes.constEnd()) {
            // A catch-all thing
            m_cachedMime = "application/octet-stream";
        } else {
            m_cachedMime = *it;
        }
    } else {
        m_cachedMime = m_cachedMime.split('\n')[0].trimmed();
    }
    return m_cachedMime;
}

QByteArray FileAttachmentItem::contentDispositionHeader() const
{
    // Looks like Thunderbird ignores attachments with funky MIME type sent with "Content-Disposition: attachment"
    // when they are not marked with the "filename" option.
    // Either I'm having a really, really bad day and I'm missing something, or they made a rather stupid bug.

    // FIXME: support RFC 2231 and its internationalized file names
    QByteArray shortFileName = QFileInfo(fileName).fileName().toAscii();
    if (shortFileName.isEmpty())
        shortFileName = "attachment";
    return "Content-Disposition: attachment;\r\n\tfilename=\"" + shortFileName + "\"\r\n";
}


ImapMessageAttachmentItem::ImapMessageAttachmentItem(Model *model, const QString &mailbox, const uint uidValidity, const uint uid):
    model(model), mailbox(mailbox), uidValidity(uidValidity), uid(uid), m_io(0)
{
}

ImapMessageAttachmentItem::~ImapMessageAttachmentItem()
{
    delete m_io;
}

QString ImapMessageAttachmentItem::caption() const
{
    TreeItemMessage *msg = messagePtr();
    if (!msg || !model)
        return MessageComposer::tr("Message not available: /%1;UIDVALIDITY=%2;UID=%3")
                .arg(mailbox, QString::number(uidValidity), QString::number(uid));
    return msg->envelope(model).subject;
}

QString ImapMessageAttachmentItem::tooltip() const
{
    TreeItemMessage *msg = messagePtr();
    if (!msg || !model)
        return QString();
    return MessageComposer::tr("IMAP message /%1;UIDVALIDITY=%2;UID=%3")
            .arg(mailbox, QString::number(uidValidity), QString::number(uid));
}

QByteArray ImapMessageAttachmentItem::contentDispositionHeader() const
{
    TreeItemMessage *msg = messagePtr();
    if (!msg || !model)
        return QByteArray();
    // FIXME: this header "sanitization" is so crude, ugly, buggy and non-compliant that I shall feel deeply ashamed
    return "Content-Disposition: attachment;\r\n\tfilename=\"" +
            msg->envelope(model).subject.toAscii().replace("\"", "'") + ".eml\"\r\n";
}

QByteArray ImapMessageAttachmentItem::mimeType() const
{
    return "message/rfc822";
}

bool ImapMessageAttachmentItem::isAvailable() const
{
    TreeItemMessage *msg = messagePtr();
    if (!msg)
        return false;

    return msg->specialColumnPtr(0, TreeItemMessage::OFFSET_TEXT)->fetched();
}

QIODevice *ImapMessageAttachmentItem::rawData() const
{
    if (m_io)
        return m_io;

    TreeItemMessage *msg = messagePtr();
    if (!msg)
        return 0;
    TreeItemPart *part = dynamic_cast<TreeItemPart*>(msg->specialColumnPtr(0, TreeItemMessage::OFFSET_TEXT));
    if (!part)
        return 0;

    m_io = new QBuffer(part->dataPtr());
    return m_io;
}

TreeItemMessage *ImapMessageAttachmentItem::messagePtr() const
{
    if (!model)
        return 0;

    TreeItemMailbox *mboxPtr = model->findMailboxByName(mailbox);
    if (!mboxPtr)
        return 0;

    if (mboxPtr->syncState.uidValidity() != uidValidity)
        return 0;

    QList<TreeItemMessage*> messages = model->findMessagesByUids(mboxPtr, QList<uint>() << uid);
    if (messages.isEmpty())
        return 0;

    Q_ASSERT(messages.size() == 1);
    return messages.front();
}

}
}
