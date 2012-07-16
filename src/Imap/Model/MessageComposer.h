#ifndef IMAP_MESSAGECOMPOSER_H
#define IMAP_MESSAGECOMPOSER_H

#include <QAbstractListModel>
#include <QPointer>

#include "Imap/Parser/Message.h"

namespace Imap {
namespace Mailbox {

class Model;
class TreeItemMessage;

/** @short A generic item to be used as an attachment */
class AttachmentItem {
public:
    virtual ~AttachmentItem();

    virtual QString caption() const = 0;
    virtual QString tooltip() const = 0;
    virtual QByteArray mimeType() const = 0;
    virtual QByteArray contentDispositionHeader() const = 0;
    virtual QIODevice *rawData() const = 0;
    virtual bool isAvailable() const = 0;
};

/** @short Part of a message stored in an IMAP server */
class ImapMessageAttachmentItem: public AttachmentItem {
public:
    ImapMessageAttachmentItem(Model *model, const QString &mailbox, const uint uidValidity, const uint uid);
    ~ImapMessageAttachmentItem();

    virtual QString caption() const;
    virtual QString tooltip() const;
    virtual QByteArray mimeType() const;
    virtual QByteArray contentDispositionHeader() const;
    virtual QIODevice *rawData() const;
    virtual bool isAvailable() const;
private:
    TreeItemMessage *messagePtr() const;
    TreeItemPart *partPtr() const;

    QPointer<Model> model;
    QString mailbox;
    uint uidValidity;
    uint uid;

    mutable QIODevice *m_io;
};

/** @short On-disk file */
class FileAttachmentItem: public AttachmentItem {
public:
    FileAttachmentItem(const QString &fileName);
    ~FileAttachmentItem();

    virtual QString caption() const;
    virtual QString tooltip() const;
    virtual QByteArray mimeType() const;
    virtual QByteArray contentDispositionHeader() const;
    virtual QIODevice *rawData() const;
    virtual bool isAvailable() const;
private:
    QString fileName;
    mutable QByteArray m_cachedMime;
    mutable QIODevice *m_io;
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
    QPointer<Model> m_model;
};

}
}

#endif // IMAP_MESSAGECOMPOSER_H
