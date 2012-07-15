#ifndef IMAP_MESSAGECOMPOSER_H
#define IMAP_MESSAGECOMPOSER_H

#include <QAbstractListModel>

#include "Imap/Parser/Message.h"

namespace Imap {
namespace Mailbox {

/** @short A generic item to be used as an attachment */
struct AttachmentItem {
    virtual ~AttachmentItem();

    virtual QString caption() const = 0;
    virtual QString tooltip() const = 0;
    virtual QByteArray mimeType() const = 0;
    virtual QByteArray contentDispositionHeader() const = 0;
    virtual QIODevice *rawData() const = 0;
    virtual bool isAvailable() const = 0;
};

#if 0
/** @short Part of a message stored in an IMAP server */
struct ImapPartAttachmentItem: public AttachmentItem {
    QPersistentModelIndex messagePart;

    ImapPartAttachmentItem(const QPersistentModelIndex &messagePart);
    ~ImapPartAttachmentItem();

    virtual QString caption() const;
    virtual QString tooltip() const;
    virtual QByteArray mimeType() const;
    virtual QByteArray contentDispositionHeader() const;
    virtual QIODevice *rawData() const;
    virtual bool isAvailable() const;
};
#endif

/** @short On-disk file */
struct FileAttachmentItem: public AttachmentItem {
    QString fileName;

    FileAttachmentItem(const QString &fileName);
    ~FileAttachmentItem();

    virtual QString caption() const;
    virtual QString tooltip() const;
    virtual QByteArray mimeType() const;
    virtual QByteArray contentDispositionHeader() const;
    virtual QIODevice *rawData() const;
    virtual bool isAvailable() const;
private:
    mutable QIODevice *m_io;
    mutable QByteArray m_cachedMime;
};

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
    ~MessageComposer();

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

    void addFileAttachment(const QString &path);

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

};

}
}

#endif // IMAP_MESSAGECOMPOSER_H
