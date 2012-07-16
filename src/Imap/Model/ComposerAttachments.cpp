#include "ComposerAttachments.h"
#include <QBuffer>
#include <QFileInfo>
#include <QMimeData>
#include <QProcess>
#include "Imap/Encoders.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MessageComposer.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/Utils.h"

namespace Imap {
namespace Mailbox {

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
    TreeItemPart *part = partPtr();
    if (part) {
        part->fetch(model);
    }
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
    TreeItemPart *part = partPtr();
    return part ? part->fetched() : false;
}

QIODevice *ImapMessageAttachmentItem::rawData() const
{
    if (m_io)
        return m_io;

    TreeItemMessage *msg = messagePtr();
    if (!msg)
        return 0;
    TreeItemPart *part = partPtr();
    if (!part)
        return 0;

    m_io = new QBuffer(part->dataPtr());
    m_io->open(QIODevice::ReadOnly);
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

TreeItemPart *ImapMessageAttachmentItem::partPtr() const
{
    TreeItemMessage *msg = messagePtr();
    if (!msg)
        return 0;
    return dynamic_cast<TreeItemPart*>(msg->specialColumnPtr(0, TreeItemMessage::OFFSET_TEXT));
}

}
}
