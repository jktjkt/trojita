/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ComposerAttachments.h"
#include <QBuffer>
#include <QFileInfo>
#include <QMimeData>
#include <QProcess>
#include <QUrl>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#  include <QMimeDatabase>
#else
#  include "mimetypes-qt4/include/QMimeDatabase"
#endif
#include "Composer/MessageComposer.h"
#include "Imap/Encoders.h"
#include "Imap/Model/FullMessageCombiner.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/Utils.h"
#include "Imap/Network/MsgPartNetAccessManager.h"

using namespace Imap::Mailbox;

namespace Composer {

QByteArray contentDispositionToByteArray(const ContentDisposition cdn)
{
    switch (cdn) {
    case CDN_INLINE:
        return "inline";
    case CDN_ATTACHMENT:
        return "attachment";
    }
    Q_ASSERT(false);
    // failsafe from RFC 2183
    return "attachment";
}

/** @short Return a CTE suitable for transmission of the specified MIME container */
AttachmentItem::ContentTransferEncoding CTEForContainers(const QModelIndex &index)
{
    QByteArray cte = index.data(RolePartEncoding).toByteArray();
    if (cte == "7bit") {
        return AttachmentItem::CTE_7BIT;
    } else if (cte == "8bit") {
        return AttachmentItem::CTE_8BIT;
    } else if (cte == "binary") {
        return AttachmentItem::CTE_BINARY;
    } else {
        // Well, we're pretty screwed here :(, the original message is either gone now (which is the better outcome),
        // or it does not specify a valid an allowed content encoding.
        // The composite types, and message/rfc822 is one of them, are not allowed to be encoded in anything but
        // 7bit, 8bit and binary (http://tools.ietf.org/html/rfc2045#page-17).
        // Let's assume "7bit", which is the default in RFC 2045.
        return AttachmentItem::CTE_7BIT;
    }
}

AttachmentItem::AttachmentItem(): m_contentDisposition(CDN_ATTACHMENT)
{
}

AttachmentItem::~AttachmentItem()
{
}

QByteArray AttachmentItem::contentDispositionHeader() const
{
    // Looks like Thunderbird ignores attachments with funky MIME type sent with "Content-Disposition: attachment"
    // when they are not marked with the "filename" option.
    // Either I'm having a really, really bad day and I'm missing something, or they made a rather stupid bug.

    QString shortFileName = contentDispositionFilename();
    if (shortFileName.isEmpty())
        shortFileName = QLatin1String("attachment");
    return "Content-Disposition: " + contentDispositionToByteArray(m_contentDisposition) + ";\r\n\t" +
            Imap::encodeRfc2231Parameter("filename", shortFileName) + "\r\n";
}

ContentDisposition AttachmentItem::contentDispositionMode() const
{
    return m_contentDisposition;
}

bool AttachmentItem::setContentDispositionMode(const ContentDisposition contentDisposition)
{
    m_contentDisposition = contentDisposition;
    return true;
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
    QString realFileName = QFileInfo(fileName).fileName();
    if (!preferredName.isEmpty() && realFileName != preferredName) {
        return MessageComposer::tr("%1\n(from %2)").arg(preferredName, realFileName);
    } else {
        return realFileName;
    }
}

QString FileAttachmentItem::tooltip() const
{
    QFileInfo f(fileName);

    if (!f.exists())
        return MessageComposer::tr("File does not exist");

    if (!f.isReadable())
        return MessageComposer::tr("File is not readable");

    return MessageComposer::tr("%1: %2, %3")
            .arg(fileName, QString::fromUtf8(mimeType()), PrettySize::prettySize(f.size(), PrettySize::WITH_BYTES_SUFFIX));
}

bool FileAttachmentItem::isAvailableLocally() const
{
    QFileInfo info(fileName);
    return info.isFile() && info.isReadable();
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

    QMimeDatabase mimeDb;
    m_cachedMime = mimeDb.mimeTypeForFile(fileName).name().toUtf8();
    return m_cachedMime;
}

QString FileAttachmentItem::contentDispositionFilename() const
{
    if (!preferredName.isEmpty())
        return preferredName;
    QString shortFileName = QFileInfo(fileName).fileName();
    if (shortFileName.isEmpty())
        shortFileName = QLatin1String("attachment");
    return shortFileName;
}

bool FileAttachmentItem::setPreferredFileName(const QString &name)
{
    preferredName = name;
    return true;
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

void FileAttachmentItem::asDroppableMimeData(QDataStream &stream) const
{
    stream << ATTACHMENT_FILE << fileName;
}


ImapMessageAttachmentItem::ImapMessageAttachmentItem(Model *model, const QString &mailbox, const uint uidValidity, const uint uid):
    fullMessageCombiner(0)
{
    Q_ASSERT(model);
    TreeItemMailbox *mboxPtr = model->findMailboxByName(mailbox);
    if (!mboxPtr)
        throw Imap::UnknownMessageIndex("No such mailbox");

    if (mboxPtr->syncState.uidValidity() != uidValidity)
        throw Imap::UnknownMessageIndex("UIDVALIDITY mismatch");

    QList<TreeItemMessage*> messages = model->findMessagesByUids(mboxPtr, QList<uint>() << uid);
    if (messages.isEmpty())
        throw Imap::UnknownMessageIndex("No such UID");

    Q_ASSERT(messages.size() == 1);
    index = messages.front()->toIndex(model);
    fullMessageCombiner = new FullMessageCombiner(index);
}

ImapMessageAttachmentItem::~ImapMessageAttachmentItem()
{
    delete fullMessageCombiner;
}

QString ImapMessageAttachmentItem::caption() const
{
    if (!index.isValid())
        return MessageComposer::tr("Message not available");
    QString subject = index.data(RoleMessageSubject).toString();
    if (!preferredName.isEmpty() && subject + QLatin1String(".eml") != preferredName) {
        return MessageComposer::tr("%1\n(%2)").arg(preferredName, subject);
    } else {
        return subject;
    }
}

QString ImapMessageAttachmentItem::tooltip() const
{
    if (!index.isValid())
        return QString();
    return MessageComposer::tr("IMAP message %1").arg(QString::fromUtf8(imapUrl()));
}

QString ImapMessageAttachmentItem::contentDispositionFilename() const
{
    if (!preferredName.isEmpty())
        return preferredName;
    if (!index.isValid())
        return QLatin1String("attachment.eml");
    return index.data(RoleMessageSubject).toString() + QLatin1String(".eml");
}

bool ImapMessageAttachmentItem::setPreferredFileName(const QString &name)
{
    preferredName = name;
    return true;
}

QByteArray ImapMessageAttachmentItem::mimeType() const
{
    return "message/rfc822";
}

bool ImapMessageAttachmentItem::isAvailableLocally() const
{
    return fullMessageCombiner->loaded();
}

QSharedPointer<QIODevice> ImapMessageAttachmentItem::rawData() const
{
    if (!index.isValid())
        return QSharedPointer<QIODevice>();

    QSharedPointer<QIODevice> io(new QBuffer());
    // This can probably be optimized to allow zero-copy operation through a pair of two QIODevices
    static_cast<QBuffer*>(io.data())->setData(fullMessageCombiner->data());
    io->open(QIODevice::ReadOnly);
    return io;
}

AttachmentItem::ContentTransferEncoding ImapMessageAttachmentItem::suggestedCTE() const
{
    // The relevant thing is the CTE of the root MIME part, not the message itself.
    // It's not even supported by Trojita for TreeItemMessage.

    QModelIndex rootPart = index.child(0, 0);
    if (rootPart.data(RolePartIsTopLevelMultipart).toBool()) {
        // This was a desperate attempt; the BODYSTRUCTURE does *not* contain the body-fld-enc field for multiparts,
        // so if our message happens to have a top-level multipart, we're out of luck and will produce an invalid result.
        // See http://mailman2.u.washington.edu/pipermail/imap-protocol/2013-October/002109.html for details.
        // Let's try to "play it safe" and assume that the children *might* contain 8bit data. We are still hoping for
        // the best (i.e. if the message was actually using the "binary" CTE, we would be screwed), but I guess this is
        // better than potentially lying by claiming that this is just a 7bit message. Suggestions welcome.
        return CTE_8BIT;
    } else {
        return CTEForContainers(rootPart);
    }
}

QByteArray ImapMessageAttachmentItem::imapUrl() const
{
    return QString::fromUtf8("/%1;UIDVALIDITY=%2/;UID=%3").arg(
                QUrl::toPercentEncoding(index.data(RoleMailboxName).toString().toUtf8()),
                index.data(RoleMailboxUidValidity).toString(),
                index.data(RoleMessageUid).toString()).toUtf8();
}

void ImapMessageAttachmentItem::preload() const
{
    fullMessageCombiner->load();
}

void ImapMessageAttachmentItem::asDroppableMimeData(QDataStream &stream) const
{
    stream << ATTACHMENT_IMAP_MESSAGE << index.data(RoleMailboxName).toString() <<
              index.data(RoleMailboxUidValidity).toUInt() << (QList<uint>() <<index.data(RoleMessageUid).toUInt());
}


ImapPartAttachmentItem::ImapPartAttachmentItem(Model *model, const QString &mailbox, const uint uidValidity, const uint uid,
                                               const QString &trojitaPath)
{
    TreeItemMailbox *mboxPtr = model->findMailboxByName(mailbox);
    if (!mboxPtr)
        throw Imap::UnknownMessageIndex("No such mailbox");

    if (mboxPtr->syncState.uidValidity() != uidValidity)
        throw Imap::UnknownMessageIndex("UIDVALIDITY mismatch");

    QList<TreeItemMessage*> messages = model->findMessagesByUids(mboxPtr, QList<uint>() << uid);
    if (messages.isEmpty())
        throw Imap::UnknownMessageIndex("UID not found");

    Q_ASSERT(messages.size() == 1);

    TreeItemPart *part = Imap::Network::MsgPartNetAccessManager::pathToPart(messages.front()->toIndex(model), trojitaPath);
    if (!part)
        throw Imap::UnknownMessageIndex("No such part");

    if (part->mimeType().startsWith(QLatin1String("multipart/"))) {
        // Yes, we absolutely do abuse this exception now. Any better ideas?
        throw Imap::UnknownMessageIndex("Cannot attach multipart/* MIME containers");
    }
    index = part->toIndex(model);
}

ImapPartAttachmentItem::~ImapPartAttachmentItem()
{
}

QString ImapPartAttachmentItem::caption() const
{
    QString partName = index.data(RolePartFileName).toString();
    if (!index.isValid() || (preferredName.isEmpty() && partName.isEmpty())) {
        return MessageComposer::tr("IMAP part %1").arg(QString::fromUtf8(imapUrl()));
    } else if (!preferredName.isEmpty()) {
        return preferredName;
    } else {
        return partName;
    }
}

QString ImapPartAttachmentItem::tooltip() const
{
    if (!index.isValid())
        return QString();
    return MessageComposer::tr("%1, %2").arg(index.data(RolePartMimeType).toString(),
                                             PrettySize::prettySize(index.data(RolePartOctets).toUInt()));
}

QByteArray ImapPartAttachmentItem::mimeType() const
{
    return index.data(RolePartMimeType).toString().toUtf8();
}

QString ImapPartAttachmentItem::contentDispositionFilename() const
{
    if (!preferredName.isEmpty())
        return preferredName;
    QString res = index.data(RolePartFileName).toString();
    return res.isEmpty() ? QLatin1String("attachment") : res;
}

bool ImapPartAttachmentItem::setPreferredFileName(const QString &name)
{
    preferredName = name;
    return true;
}

AttachmentItem::ContentTransferEncoding ImapPartAttachmentItem::suggestedCTE() const
{
    if (index.data(RolePartMimeType).toString() == QLatin1String("message/rfc822")) {
        return CTEForContainers(index);
    } else {
        // FIXME: it would be cool to improve this so that we could e.g. use a quoted-printable for text files, etc
        return CTE_BASE64;
    }
}

QSharedPointer<QIODevice> ImapPartAttachmentItem::rawData() const
{
    if (!index.isValid() || !index.data(RoleIsFetched).toBool())
        return QSharedPointer<QIODevice>();

    QSharedPointer<QIODevice> io(new QBuffer());
    static_cast<QBuffer*>(io.data())->setData(index.data(RolePartData).toByteArray());
    io->open(QIODevice::ReadOnly);
    return io;
}

bool ImapPartAttachmentItem::isAvailableLocally() const
{
    return index.data(RoleIsFetched).toBool();
}

QByteArray ImapPartAttachmentItem::imapUrl() const
{
    Q_ASSERT(index.isValid());
    return QString::fromUtf8("/%1;UIDVALIDITY=%2/;UID=%3/;SECTION=%4").arg(
                QUrl::toPercentEncoding(index.data(RoleMailboxName).toString().toUtf8()),
                index.data(RoleMailboxUidValidity).toString(),
                index.data(RoleMessageUid).toString(),
                index.data(RolePartId).toString()).toUtf8();
}

void ImapPartAttachmentItem::preload() const
{
    index.data(RolePartData);
}

void ImapPartAttachmentItem::asDroppableMimeData(QDataStream &stream) const
{
    Q_ASSERT(index.isValid());
    stream << ATTACHMENT_IMAP_PART << index.data(RoleMailboxName).toString() << index.data(RoleMailboxUidValidity).toUInt() <<
              index.data(RoleMessageUid).toUInt() << index.data(RolePartPathToPart).toString();
}

}
