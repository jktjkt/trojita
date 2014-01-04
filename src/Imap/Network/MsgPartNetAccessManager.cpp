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
#include <QNetworkRequest>
#include <QStringList>
#include <QDebug>

#include "MsgPartNetAccessManager.h"
#include "ForbiddenReply.h"
#include "MsgPartNetworkReply.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

namespace Imap
{

/** @short Classes associated with the implementation of the QNetworkAccessManager */
namespace Network
{

MsgPartNetAccessManager::MsgPartNetAccessManager(QObject *parent):
    QNetworkAccessManager(parent), externalsEnabled(false)
{
    // The "image/pjpeg" nonsense is non-standard kludge produced by Micorosft Internet Explorer
    // (http://msdn.microsoft.com/en-us/library/ms775147(VS.85).aspx#_replace). As of May 2011, it is not listed in
    // the official list of assigned MIME types (http://www.iana.org/assignments/media-types/image/index.html), but generated
    // by MSIE nonetheless. Users of e-mail can see it for example in messages produced by webmails which do not check the
    // client-provided MIME types. QWebView would (arguably correctly) refuse to display such a blob, but the damned users
    // typically want to see their images (I certainly do), even though they are not standards-compliant. Hence we fix the
    // header here.
    registerMimeTypeTranslation(QLatin1String("image/pjpeg"), QLatin1String("image/jpeg"));
}

void MsgPartNetAccessManager::setModelMessage(const QModelIndex &message_)
{
    message = message_;
}

/** @short Prepare a network request

This function handles delegating access to the other body parts using various schemes (ie. the special trojita-imap:// one used
by Trojita for internal purposes and the cid: one for referencing to other body parts).  Policy checks for filtering access to
the public Internet are also performed at this level.
*/
QNetworkReply *MsgPartNetAccessManager::createRequest(Operation op, const QNetworkRequest &req, QIODevice *outgoingData)
{
    Q_UNUSED(op);
    Q_UNUSED(outgoingData);

    if (!message.isValid()) {
        // Our message got removed in the meanwhile
        // FIXME: add a better class here
        return new Imap::Network::ForbiddenReply(this);
    }

    Q_ASSERT(message.isValid());
    const Mailbox::Model *constModel = 0;
    Mailbox::Model::realTreeItem(message, &constModel);
    Q_ASSERT(constModel);
    Mailbox::Model *model = const_cast<Mailbox::Model *>(constModel);
    Q_ASSERT(model);
    Imap::Mailbox::TreeItemPart *part = pathToPart(message, req.url().path());
    QModelIndex partIndex = part ? part->toIndex(model) : QModelIndex();

    if (req.url().scheme() == QLatin1String("trojita-imap") && req.url().host() == QLatin1String("msg")) {
        // Internal Trojita reference
        if (part) {
            return new Imap::Network::MsgPartNetworkReply(this, partIndex);
        } else {
            qDebug() << "No such part:" << req.url();
            return new Imap::Network::ForbiddenReply(this);
        }
    } else if (req.url().scheme() == QLatin1String("cid")) {
        // The cid: scheme for cross-part references
        QByteArray cid = req.url().path().toUtf8();
        if (!cid.startsWith("<"))
            cid = QByteArray("<") + cid;
        if (!cid.endsWith(">"))
            cid += ">";
        Imap::Mailbox::TreeItemPart *target = cidToPart(cid, model, model->realTreeItem(message));
        if (target) {
            return new Imap::Network::MsgPartNetworkReply(this, target->toIndex(model));
        } else {
            qDebug() << "Content-ID not found" << cid;
            return new Imap::Network::ForbiddenReply(this);
        }
    } else if (req.url() == QUrl(QLatin1String("about:blank"))) {
        // about:blank is a relatively harmless URL which is used for opening an empty page
        return QNetworkAccessManager::createRequest(op, req, outgoingData);
    } else if (req.url().scheme() == QLatin1String("data")) {
        // data: scheme shall be safe, it's just a method of local access after all
        return QNetworkAccessManager::createRequest(op, req, outgoingData);
    } else {
        // Regular access -- we've got to check policy here
        if (req.url().scheme() == QLatin1String("http") || req.url().scheme() == QLatin1String("https")) {
            if (externalsEnabled) {
                return QNetworkAccessManager::createRequest(op, req, outgoingData);
            } else {
                emit requestingExternal(req.url());
                return new Imap::Network::ForbiddenReply(this);
            }
        } else {
            qDebug() << "Forbidden per policy:" << req.url();
            return new Imap::Network::ForbiddenReply(this);
        }
    }
}

/** @short Find a message body part through its slash-separated string path */
Imap::Mailbox::TreeItemPart *MsgPartNetAccessManager::pathToPart(const QModelIndex &message, const QString &path)
{
    QStringList items = path.split('/', QString::SkipEmptyParts);
    const Mailbox::Model *model = 0;
    Imap::Mailbox::TreeItem *target = Mailbox::Model::realTreeItem(message, &model);
    Q_ASSERT(model);
    Q_ASSERT(target);
    bool ok = ! items.isEmpty(); // if it's empty, it's a bogous URL

    for (QStringList::const_iterator it = items.constBegin(); it != items.constEnd(); ++it) {
        uint offset = it->toUInt(&ok);
        if (!ok) {
            // special case, we have to dive into that funny, irregular special parts now
            if (*it == QLatin1String("HEADER"))
                target = target->specialColumnPtr(0, Imap::Mailbox::TreeItem::OFFSET_HEADER);
            else if (*it == QLatin1String("TEXT"))
                target = target->specialColumnPtr(0, Imap::Mailbox::TreeItem::OFFSET_TEXT);
            else if (*it == QLatin1String("MIME"))
                target = target->specialColumnPtr(0, Imap::Mailbox::TreeItem::OFFSET_MIME);
            else
                return 0;
            continue;
        }
        if (offset >= target->childrenCount(const_cast<Mailbox::Model *>(model))) {
            return 0;
        }
        target = target->child(offset, const_cast<Mailbox::Model *>(model));
    }
    return dynamic_cast<Imap::Mailbox::TreeItemPart *>(target);
}

/** @short Convert a CID-formatted specification of a MIME part to a TreeItemPart*

The MIME messages contain a scheme which can be used to provide a reference from one message part to another using the content id
headers.  This function walks the MIME tree and tries to find a MIME part whose ID matches the requested item.
*/
Imap::Mailbox::TreeItemPart *MsgPartNetAccessManager::cidToPart(const QByteArray &cid, Mailbox::Model *model, Mailbox::TreeItem *root)
{
    // A DFS search through the MIME parts tree of the current message which tries to check for a matching body part
    for (uint i = 0; i < root->childrenCount(model); ++i) {
        Imap::Mailbox::TreeItemPart *part = dynamic_cast<Imap::Mailbox::TreeItemPart *>(root->child(i, model));
        Q_ASSERT(part);
        if (part->bodyFldId() == cid)
            return part;
        part = cidToPart(cid, model, part);
        if (part)
            return part;
    }
    return 0;
}

/** @short Enable/disable fetching of external items

The external items are anything on the Internet which is outside of the scope of the current message.  At this time, we support
fetching contents via HTTP and FTP protocols.
*/
void MsgPartNetAccessManager::setExternalsEnabled(bool enabled)
{
    externalsEnabled = enabled;
}

/** @short Look for registered translations of MIME types

Certain renderers (the QWebView, most notably) are rather picky about the content they can render.
For example, a C++ header file's MIME type inherits from text/plain, but QWebView would still treat
it as a file to download. The image/pjpeg "type" is another example.

This MIME type translation apparently has to happen at the QNetworkReply layer, so it makes sense to
track the registered translations within the QNAM subclass.
*/
QString MsgPartNetAccessManager::translateToSupportedMimeType(const QString &originalMimeType) const
{
    QMap<QString, QString>::const_iterator it = m_mimeTypeFixups.constFind(originalMimeType);
    return it == m_mimeTypeFixups.constEnd() ? originalMimeType : *it;
}

/** @short Register a MIME type for an automatic translation to one that is recognized by the renderers */
void MsgPartNetAccessManager::registerMimeTypeTranslation(const QString &originalMimeType, const QString &translatedMimeType)
{
    m_mimeTypeFixups[originalMimeType] = translatedMimeType;
}

}
}


