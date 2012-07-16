#ifndef IMAP_COMPOSERATTACHMENTS_H
#define IMAP_COMPOSERATTACHMENTS_H

#include <QIODevice>
#include <QPointer>
#include <QSharedPointer>

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

    /** @short Return shared pointer to QIODevice which is ready to return data for this part

    The underlying QIODevice MUST NOT be stored for future use.  It is no longer valid after the source
    AttachmentItem is destroyed.

    The QIODevice MAY support only a single reading pass.  If the caller wants to read data multiple times, they should
    obtain another copy through calling rawData again.

    This funciton MAY return a null pointer if the data is not ready yet. Always use isAvailable() to make sure that
    the funciton will return correct data AND check the return value due to a possible TOCTOU issue.

    When the event loop is renetered, the QIODevice MAY become invalid and MUST NOT be used anymore.

    (I really, really like the RFC way of expression constraints :). )
    */
    virtual QSharedPointer<QIODevice> rawData() const = 0;

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
    virtual QSharedPointer<QIODevice> rawData() const;
    virtual bool isAvailable() const;
private:
    TreeItemMessage *messagePtr() const;
    TreeItemPart *headerPartPtr() const;
    TreeItemPart *bodyPartPtr() const;

    QPointer<Model> model;
    QString mailbox;
    uint uidValidity;
    uint uid;
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
    virtual QSharedPointer<QIODevice> rawData() const;
    virtual bool isAvailable() const;
private:
    QString fileName;
    mutable QByteArray m_cachedMime;
};

}
}

#endif // IMAP_COMPOSERATTACHMENTS_H
