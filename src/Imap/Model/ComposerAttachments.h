#ifndef IMAP_COMPOSERATTACHMENTS_H
#define IMAP_COMPOSERATTACHMENTS_H

#include <QIODevice>
#include <QPointer>

namespace Imap {
namespace Mailbox {

class Model;
class TreeItemMessage;
class TreeItemPart;

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

}
}

#endif // IMAP_COMPOSERATTACHMENTS_H
