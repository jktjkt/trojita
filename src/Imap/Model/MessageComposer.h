#ifndef IMAP_MESSAGECOMPOSER_H
#define IMAP_MESSAGECOMPOSER_H

#include <QAbstractListModel>

#include "Imap/Parser/Message.h"

namespace Imap {
namespace Mailbox {

/** @short Model storing individual parts of a composed message */
class MessageComposer : public QAbstractListModel
{
    Q_OBJECT
public:

    /** @short Recipients */
    typedef enum {
        Recipient_To,
        Recipient_Cc,
        Recipient_Bcc
    } RecipientKind;

    explicit MessageComposer(QObject *parent = 0);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    void setFrom(const Message::MailAddress &from);
    void setRecipients(const QList<QPair<RecipientKind,Message::MailAddress> > &recipients);
    void setInReplyTo(const QByteArray &inReplyTo);
    void setTimestamp(const QDateTime &timestamp);
    void setSubject(const QString &subject);
    void setText(const QString &text);

    bool isReadyForSerialization() const;
    QByteArray asRawMessage() const;

    QDateTime timestamp() const;
    QByteArray rawFromAddress() const;
    QList<QByteArray> rawRecipientAddresses() const;

private:
    static QByteArray generateMessageId(const Imap::Message::MailAddress &sender);
    static QByteArray encodeHeaderField(const QString &text);
    static QByteArray generateMimeBoundary();

    Message::MailAddress m_from;
    QList<QPair<RecipientKind,Message::MailAddress> > m_recipients;
    QByteArray m_inReplyTo;
    QDateTime m_timestamp;
    QString m_subject;
    QString m_text;

};

}
}

#endif // IMAP_MESSAGECOMPOSER_H
