#include "MessageComposer.h"
#include <QCoreApplication>
#include <QUuid>
#include "Imap/Encoders.h"
#include "Imap/Model/Utils.h"

namespace Imap {
namespace Mailbox {

MessageComposer::MessageComposer(QObject *parent) :
    QAbstractListModel(parent)
{
}

int MessageComposer::rowCount(const QModelIndex &parent) const
{
    return 0;
}

QVariant MessageComposer::data(const QModelIndex &index, int role) const
{
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

    res.append("Content-Type: text/plain; charset=utf-8\r\n"
                            "Content-Transfer-Encoding: quoted-printable\r\n");
    res.append("\r\n");
    res.append(Imap::quotedPrintableEncode(m_text.toUtf8()));
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

}
}
