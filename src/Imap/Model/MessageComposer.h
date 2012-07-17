#ifndef IMAP_MESSAGECOMPOSER_H
#define IMAP_MESSAGECOMPOSER_H

#include <QAbstractListModel>
#include <QPointer>

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

    /** @short Recipients */
    typedef enum {
        Recipient_To,
        Recipient_Cc,
        Recipient_Bcc
    } RecipientKind;

    explicit MessageComposer(Model *model, QObject *parent = 0);
    ~MessageComposer();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    virtual Qt::DropActions supportedDropActions() const;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const;
    virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
    virtual QStringList mimeTypes() const;

    void setFrom(const Message::MailAddress &from);
    void setRecipients(const QList<QPair<RecipientKind,Message::MailAddress> > &recipients);
    void setInReplyTo(const QByteArray &inReplyTo);
    void setTimestamp(const QDateTime &timestamp);
    void setSubject(const QString &subject);
    void setText(const QString &text);

    bool isReadyForSerialization() const;
    bool asRawMessage(QIODevice *target, QString *errorMessage) const;

    QDateTime timestamp() const;
    QByteArray rawFromAddress() const;
    QList<QByteArray> rawRecipientAddresses() const;

    void addFileAttachment(const QString &path);
    void removeAttachment(const QModelIndex &index);

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

    QList<AttachmentItem *> m_attachments;
    QPointer<Model> m_model;
};

}
}

#endif // IMAP_MESSAGECOMPOSER_H
