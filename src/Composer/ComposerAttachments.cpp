/* Copyright (C) 2006 - 2013 Jan Kundrát <jkt@flaska.net>

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
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/Utils.h"
#include "Imap/Network/MsgPartNetAccessManager.h"

namespace Composer {

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

    return MessageComposer::tr("%1: %2, %3").arg(fileName, QString::fromUtf8(mimeType()), QString::number(f.size()));
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

QByteArray FileAttachmentItem::contentDispositionHeader() const
{
    // Looks like Thunderbird ignores attachments with funky MIME type sent with "Content-Disposition: attachment"
    // when they are not marked with the "filename" option.
    // Either I'm having a really, really bad day and I'm missing something, or they made a rather stupid bug.

    QString shortFileName = QFileInfo(fileName).fileName();
    if (shortFileName.isEmpty())
        shortFileName = QLatin1String("attachment");
    return "Content-Disposition: attachment;\r\n\t" + Imap::encodeRfc2231Parameter("filename", shortFileName) + "\r\n";
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


ImapMessageAttachmentItem::ImapMessageAttachmentItem(Imap::Mailbox::Model *model, const QString &mailbox, const uint uidValidity, const uint uid):
    fullMessageCombiner(0), model(model), mailbox(mailbox), uidValidity(uidValidity), uid(uid)
{
    Q_ASSERT(model);
    Imap::Mailbox::TreeItemMessage *msg = messagePtr();
    Q_ASSERT(msg);
    fullMessageCombiner = new Imap::Mailbox::FullMessageCombiner(msg->toIndex(model));
}

ImapMessageAttachmentItem::~ImapMessageAttachmentItem()
{
    delete fullMessageCombiner;
}

QString ImapMessageAttachmentItem::caption() const
{
    Imap::Mailbox::TreeItemMessage *msg = messagePtr();
    if (!msg || !model)
        return MessageComposer::tr("Message not available: /%1;UIDVALIDITY=%2;UID=%3")
                .arg(mailbox, QString::number(uidValidity), QString::number(uid));
    return msg->envelope(model).subject;
}

QString ImapMessageAttachmentItem::tooltip() const
{
    Imap::Mailbox::TreeItemMessage *msg = messagePtr();
    if (!msg || !model)
        return QString();
    return MessageComposer::tr("IMAP message %1").arg(QString::fromUtf8(imapUrl()));
}

QByteArray ImapMessageAttachmentItem::contentDispositionHeader() const
{
    Imap::Mailbox::TreeItemMessage *msg = messagePtr();
    if (!msg || !model)
        return QByteArray();
    return "Content-Disposition: attachment;\r\n\t" +
            Imap::encodeRfc2231Parameter("filname", msg->envelope(model).subject + QLatin1String(".eml")) +
            "\r\n";
}

QByteArray ImapMessageAttachmentItem::mimeType() const
{
    return "message/rfc822";
}

bool ImapMessageAttachmentItem::isAvailableLocally() const
{
    Imap::Mailbox::TreeItemMessage *msg = messagePtr();
    if (!msg)
        return false;

    return fullMessageCombiner->loaded();
}

QSharedPointer<QIODevice> ImapMessageAttachmentItem::rawData() const
{
    Imap::Mailbox::TreeItemMessage *msg = messagePtr();
    if (!msg)
        return QSharedPointer<QIODevice>();

    QSharedPointer<QIODevice> io(new QBuffer());
    // This can probably be optimized to allow zero-copy operation through a pair of two QIODevices
    static_cast<QBuffer*>(io.data())->setData(fullMessageCombiner->data());
    io->open(QIODevice::ReadOnly);
    return io;
}

Imap::Mailbox::TreeItemMessage *ImapMessageAttachmentItem::messagePtr() const
{
    if (!model)
        return 0;

    Imap::Mailbox::TreeItemMailbox *mboxPtr = model->findMailboxByName(mailbox);
    if (!mboxPtr)
        return 0;

    if (mboxPtr->syncState.uidValidity() != uidValidity)
        return 0;

    QList<Imap::Mailbox::TreeItemMessage*> messages = model->findMessagesByUids(mboxPtr, QList<uint>() << uid);
    if (messages.isEmpty())
        return 0;

    Q_ASSERT(messages.size() == 1);
    return messages.front();
}

AttachmentItem::ContentTransferEncoding ImapMessageAttachmentItem::suggestedCTE() const
{
    // FIXME?
    return CTE_7BIT;
}

QByteArray ImapMessageAttachmentItem::imapUrl() const
{
    return QString::fromUtf8("/%1;UIDVALIDITY=%2/;UID=%3").arg(
                QUrl::toPercentEncoding(mailbox), QString::number(uidValidity), QString::number(uid)).toUtf8();
}

void ImapMessageAttachmentItem::preload() const
{
    fullMessageCombiner->load();
}

void ImapMessageAttachmentItem::asDroppableMimeData(QDataStream &stream) const
{
    stream << ATTACHMENT_IMAP_MESSAGE << mailbox << uidValidity << (QList<uint>() << uid);
}


ImapPartAttachmentItem::ImapPartAttachmentItem(Imap::Mailbox::Model *model, const QString &mailbox, const uint uidValidity, const uint uid,
                                               const QString &pathToPart, const QString &trojitaPath):
    model(model), mailbox(mailbox), uidValidity(uidValidity), uid(uid), imapPartId(pathToPart), trojitaPath(trojitaPath)
{
}

ImapPartAttachmentItem::~ImapPartAttachmentItem()
{
}

Imap::Mailbox::TreeItemPart *ImapPartAttachmentItem::partPtr() const
{
    if (!model)
        return 0;

    Imap::Mailbox::TreeItemMailbox *mboxPtr = model->findMailboxByName(mailbox);
    if (!mboxPtr)
        return 0;

    if (mboxPtr->syncState.uidValidity() != uidValidity)
        return 0;

    QList<Imap::Mailbox::TreeItemMessage*> messages = model->findMessagesByUids(mboxPtr, QList<uint>() << uid);
    if (messages.isEmpty())
        return 0;

    Q_ASSERT(messages.size() == 1);

    return Imap::Network::MsgPartNetAccessManager::pathToPart(messages.front()->toIndex(model), trojitaPath);
}

QString ImapPartAttachmentItem::caption() const
{
    Imap::Mailbox::TreeItemPart *part = partPtr();
    if (part && !part->fileName().isEmpty()) {
        return part->fileName();
    } else {
        return MessageComposer::tr("IMAP part %1").arg(QString::fromUtf8(imapUrl()));
    }
}

QString ImapPartAttachmentItem::tooltip() const
{
    Imap::Mailbox::TreeItemPart *part = partPtr();
    if (!part)
        return QString();
    return MessageComposer::tr("%1, %2").arg(part->mimeType(), Imap::Mailbox::PrettySize::prettySize(part->octets()));
}

QByteArray ImapPartAttachmentItem::mimeType() const
{
    Imap::Mailbox::TreeItemPart *part = partPtr();
    if (!part)
        return QByteArray();
    return part->mimeType().toUtf8();
}

QByteArray ImapPartAttachmentItem::contentDispositionHeader() const
{
    Imap::Mailbox::TreeItemPart *part = partPtr();
    if (!part)
        return QByteArray();
    return "Content-Disposition: attachment;\r\n\t" + Imap::encodeRfc2231Parameter("filename", part->fileName()) + "\r\n";
}

AttachmentItem::ContentTransferEncoding ImapPartAttachmentItem::suggestedCTE() const
{
    // FIXME?
    return CTE_BASE64;
}

QSharedPointer<QIODevice> ImapPartAttachmentItem::rawData() const
{
    Imap::Mailbox::TreeItemPart *part = partPtr();
    if (!part || !part->fetched())
        return QSharedPointer<QIODevice>();

    QSharedPointer<QIODevice> io(new QBuffer());
    static_cast<QBuffer*>(io.data())->setData(*(part->dataPtr()));
    io->open(QIODevice::ReadOnly);
    return io;
}

bool ImapPartAttachmentItem::isAvailableLocally() const
{
    Imap::Mailbox::TreeItemPart *part = partPtr();
    return part ? part->fetched() : false;
}

QByteArray ImapPartAttachmentItem::imapUrl() const
{
    return QString::fromUtf8("/%1;UIDVALIDITY=%2/;UID=%3/;SECTION=%4").arg(
                QUrl::toPercentEncoding(mailbox), QString::number(uidValidity), QString::number(uid), imapPartId).toUtf8();
}

void ImapPartAttachmentItem::preload() const
{
    Imap::Mailbox::TreeItemPart *part = partPtr();
    if (part) {
        part->fetch(model);
    }
}

void ImapPartAttachmentItem::asDroppableMimeData(QDataStream &stream) const
{
    stream << ATTACHMENT_IMAP_PART << mailbox << uidValidity << uid << imapPartId << trojitaPath;
}

}
