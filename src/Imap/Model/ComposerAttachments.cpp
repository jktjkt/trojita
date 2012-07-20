#include "ComposerAttachments.h"
#include <QBuffer>
#include <QFileInfo>
#include <QMimeData>
#include <QProcess>
#include <QUrl>
#include "Imap/Encoders.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MessageComposer.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/Utils.h"
#include "Imap/Network/MsgPartNetAccessManager.h"

namespace Imap {
namespace Mailbox {

AttachmentItem::~AttachmentItem()
{
}

FileAttachmentItem::FileAttachmentItem(const QString &fileName):
    fileName(fileName)
{
}

FileAttachmentItem::~FileAttachmentItem()
{
}

QString FileAttachmentItem::caption() const
{
    return QFileInfo(fileName).fileName();
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

bool FileAttachmentItem::isAvailableLocally() const
{
    return QFileInfo(fileName).isReadable();
}

QSharedPointer<QIODevice> FileAttachmentItem::rawData() const
{
    QSharedPointer<QIODevice> io(new QFile(fileName));
    io->open(QIODevice::ReadOnly);
    return io;
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

AttachmentItem::ContentTransferEncoding FileAttachmentItem::suggestedCTE() const
{
    return CTE_BASE64;
}

QByteArray FileAttachmentItem::imapUrl() const
{
    // It's a local item, it cannot really be on an IMAP server
    return QByteArray();
}

void FileAttachmentItem::preload() const
{
    // Don't need to do anything
    // We could possibly leave this file open to prevent eventual deletion, but it's probably not worth the effort.
}


ImapMessageAttachmentItem::ImapMessageAttachmentItem(Model *model, const QString &mailbox, const uint uidValidity, const uint uid):
    model(model), mailbox(mailbox), uidValidity(uidValidity), uid(uid)
{
}

ImapMessageAttachmentItem::~ImapMessageAttachmentItem()
{
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
    return MessageComposer::tr("IMAP message %1").arg(QString::fromAscii(imapUrl()));
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

bool ImapMessageAttachmentItem::isAvailableLocally() const
{
    TreeItemMessage *msg = messagePtr();
    if (!msg)
        return false;
    TreeItemPart *headerPart = headerPartPtr();
    TreeItemPart *bodyPart = bodyPartPtr();
    return headerPart && bodyPart && headerPart->fetched() && bodyPart->fetched();
}

QSharedPointer<QIODevice> ImapMessageAttachmentItem::rawData() const
{
    TreeItemMessage *msg = messagePtr();
    if (!msg)
        return QSharedPointer<QIODevice>();
    TreeItemPart *headerPart = headerPartPtr();
    if (!headerPart || !headerPart->fetched())
        return QSharedPointer<QIODevice>();
    TreeItemPart *bodyPart = bodyPartPtr();
    if (!bodyPart || !bodyPart->fetched())
        return QSharedPointer<QIODevice>();

    QSharedPointer<QIODevice> io(new QBuffer());
    // This can probably be optimized to allow zero-copy operation through a pair of two QIODevices
    static_cast<QBuffer*>(io.data())->setData(*(headerPart->dataPtr()) + *(bodyPart->dataPtr()));
    io->open(QIODevice::ReadOnly);
    return io;
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

TreeItemPart *ImapMessageAttachmentItem::headerPartPtr() const
{
    TreeItemMessage *msg = messagePtr();
    if (!msg)
        return 0;
    return dynamic_cast<TreeItemPart*>(msg->specialColumnPtr(0, TreeItemMessage::OFFSET_HEADER));
}

TreeItemPart *ImapMessageAttachmentItem::bodyPartPtr() const
{
    TreeItemMessage *msg = messagePtr();
    if (!msg)
        return 0;
    return dynamic_cast<TreeItemPart*>(msg->specialColumnPtr(0, TreeItemMessage::OFFSET_TEXT));
}

AttachmentItem::ContentTransferEncoding ImapMessageAttachmentItem::suggestedCTE() const
{
    // FIXME?
    return CTE_7BIT;
}

QByteArray ImapMessageAttachmentItem::imapUrl() const
{
    return QString::fromAscii("/%1;UIDVALIDITY=%2/;UID=%3").arg(
                QUrl::toPercentEncoding(mailbox), QString::number(uidValidity), QString::number(uid)).toAscii();
}

void ImapMessageAttachmentItem::preload() const
{
    TreeItemPart *part = headerPartPtr();
    if (part) {
        part->fetch(model);
    }
    part = bodyPartPtr();
    if (part)
        part->fetch(model);
}


ImapPartAttachmentItem::ImapPartAttachmentItem(Model *model, const QString &mailbox, const uint uidValidity, const uint uid,
                                               const QString &pathToPart, const QString &trojitaPath):
    model(model), mailbox(mailbox), uidValidity(uidValidity), uid(uid), imapPartId(pathToPart), trojitaPath(trojitaPath)
{
}

ImapPartAttachmentItem::~ImapPartAttachmentItem()
{
}

TreeItemPart *ImapPartAttachmentItem::partPtr() const
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

    return Imap::Network::MsgPartNetAccessManager::pathToPart(messages.front()->toIndex(model), trojitaPath);
}

QString ImapPartAttachmentItem::caption() const
{
    TreeItemPart *part = partPtr();
    if (part && !part->fileName().isEmpty()) {
        return part->fileName();
    } else {
        return MessageComposer::tr("IMAP part %1").arg(QString::fromAscii(imapUrl()));
    }
}

QString ImapPartAttachmentItem::tooltip() const
{
    TreeItemPart *part = partPtr();
    if (!part)
        return QString();
    return MessageComposer::tr("%1, %2").arg(part->mimeType(), PrettySize::prettySize(part->octets()));
}

QByteArray ImapPartAttachmentItem::mimeType() const
{
    TreeItemPart *part = partPtr();
    if (!part)
        return QByteArray();
    return part->mimeType().toAscii();
}

QByteArray ImapPartAttachmentItem::contentDispositionHeader() const
{
    TreeItemPart *part = partPtr();
    if (!part)
        return QByteArray();
    return "Content-Disposition: attachment;\r\n\tfilename=\"" + part->fileName().toAscii() + "\"\r\n";
}

AttachmentItem::ContentTransferEncoding ImapPartAttachmentItem::suggestedCTE() const
{
    // FIXME?
    return CTE_BASE64;
}

QSharedPointer<QIODevice> ImapPartAttachmentItem::rawData() const
{
    TreeItemPart *part = partPtr();
    if (!part || !part->fetched())
        return QSharedPointer<QIODevice>();

    QSharedPointer<QIODevice> io(new QBuffer());
    static_cast<QBuffer*>(io.data())->setData(*(part->dataPtr()));
    io->open(QIODevice::ReadOnly);
    return io;
}

bool ImapPartAttachmentItem::isAvailableLocally() const
{
    TreeItemPart *part = partPtr();
    return part ? part->fetched() : false;
}

QByteArray ImapPartAttachmentItem::imapUrl() const
{
    return QString::fromAscii("/%1;UIDVALIDITY=%2/;UID=%3/;SECTION=%4").arg(
                QUrl::toPercentEncoding(mailbox), QString::number(uidValidity), QString::number(uid), imapPartId).toAscii();
}

void ImapPartAttachmentItem::preload() const
{
    TreeItemPart *part = partPtr();
    if (part) {
        part->fetch(model);
    }
}

}
}
