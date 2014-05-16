/* Copyright (C) 2014-2015 Stephan Platz <trojita@paalsteek.de>
   Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

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

#include <sstream>
#include <mimetic/mimetic.h>
#include "Cryptography/MimeticUtils.h"
#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Parser/Message.h"
#include "Imap/Parser/Rfc5322HeaderParser.h"

namespace Cryptography {

void MimeticUtils::storeInterestingFields(const mimetic::MimeEntity &me, LocalMessagePart *part)
{
    part->setCharset(me.header().contentType().param("charset").c_str());
    QByteArray format = me.header().contentType().param("format").c_str();
    if (!format.isEmpty()) {
        part->setContentFormat(format.toLower());
        part->setDelSp(me.header().contentType().param("delsp").c_str());
    }
    const mimetic::ContentDisposition &cd = me.header().contentDisposition();
    QByteArray bodyDisposition = cd.type().c_str();
    QString filename;
    if (!bodyDisposition.isEmpty()) {
        part->setBodyDisposition(bodyDisposition);
        const std::list<mimetic::ContentDisposition::Param> l = cd.paramList();
        QMap<QByteArray, QByteArray> paramMap;
        for (auto it = l.begin(); it != l.end(); ++it) {
            const mimetic::ContentDisposition::Param &p = *it;
            paramMap.insert(p.name().c_str(), p.value().c_str());
        }
        filename = Imap::extractRfc2231Param(paramMap, "filename");
    }
    if (filename.isEmpty()) {
        const std::list<mimetic::ContentType::Param> l = me.header().contentType().paramList();
        QMap<QByteArray, QByteArray> paramMap;
        for (auto it = l.begin(); it != l.end(); ++it) {
            const mimetic::ContentType::Param &p = *it;
            paramMap.insert(p.name().c_str(), p.value().c_str());
        }
        filename = Imap::extractRfc2231Param(paramMap, "name");
    }

    if (!filename.isEmpty()) {
        part->setFilename(filename);
    }

    // The following header fields do not make sense for multiparts
    if (!me.header().contentType().isMultipart()) {
        part->setEncoding(me.header().contentTransferEncoding().str().c_str());
        part->setBodyFldId(me.header().contentId().str().c_str());
    }

    // The number of octets doesn't have an equivalent in headers, so we aren't storing it
}

QList<Imap::Message::MailAddress> MimeticUtils::mailboxListToQList(const mimetic::MailboxList &list)
{
    QList<Imap::Message::MailAddress> result;
    for (const mimetic::Mailbox &addr: list)
    {
        result.append(Imap::Message::MailAddress(
                          Imap::decodeRFC2047String(addr.label().data()),
                          QString::fromStdString(addr.sourceroute()),
                          QString::fromStdString(addr.mailbox()),
                          QString::fromStdString(addr.domain())));
    }
    return result;
}

QList<Imap::Message::MailAddress> MimeticUtils::addressListToQList(const mimetic::AddressList &list)
{
    QList<Imap::Message::MailAddress> result;
    for (const mimetic::Address &addr: list)
    {
        if (addr.isGroup()) {
            const mimetic::Group group = addr.group();
            for (mimetic::Group::const_iterator it = group.begin(), end = group.end(); it != end; it++) {
                const mimetic::Mailbox mb = *it;
                result.append(Imap::Message::MailAddress(
                                  Imap::decodeRFC2047String(mb.label().data()),
                                  QString::fromStdString(mb.sourceroute()),
                                  QString::fromStdString(mb.mailbox()),
                                  QString::fromStdString(mb.domain())));
            }
        } else {
            const mimetic::Mailbox mb = addr.mailbox();
            result.append(Imap::Message::MailAddress(
                              Imap::decodeRFC2047String(mb.label().data()),
                              QString::fromStdString(mb.sourceroute()),
                              QString::fromStdString(mb.mailbox()),
                              QString::fromStdString(mb.domain())));
        }
    }
    return result;
}

QByteArray dumpMimeHeader(const mimetic::Header &h)
{
    std::stringstream ss;
    for (auto it = h.begin(), end = h.end(); it != end; ++it) {
        it->write(ss) << "\r\n";
    }
    return ss.str().c_str();
}

MessagePart::Ptr MimeticUtils::mimeEntityToPart(const mimetic::MimeEntity &me, MessagePart *parent, const int row)
{
    QByteArray type = QByteArray(me.header().contentType().type().c_str()).toLower();
    QByteArray subtype = QByteArray(me.header().contentType().subtype().c_str()).toLower();
    std::unique_ptr<LocalMessagePart> part(new LocalMessagePart(parent, row, QByteArray(type + "/" + subtype)));
    if (me.body().parts().size() > 0) {
        if (type == QByteArray("message") && subtype == QByteArray("rfc822")) {
            const mimetic::Header h = (*(me.body().parts().begin()))->header();
            QDateTime date = QDateTime::fromString(QString::fromStdString(h.field("date").value()));
            QString subject = Imap::decodeRFC2047String(h.subject().data());
            QList<Imap::Message::MailAddress> from, sender, replyTo, to, cc, bcc;
            from = mailboxListToQList(h.from());
            if (h.hasField("sender")) {
                sender.append(Imap::Message::MailAddress(
                                  Imap::decodeRFC2047String(h.sender().label().data()),
                                  QString::fromStdString(h.sender().sourceroute()),
                                  QString::fromStdString(h.sender().mailbox()),
                                  QString::fromStdString(h.sender().domain())));
            }
            replyTo = addressListToQList(h.replyto());
            to = addressListToQList(h.to());
            cc = addressListToQList(h.cc());
            bcc = addressListToQList(h.bcc());
            Imap::LowLevelParser::Rfc5322HeaderParser headerParser;
            QByteArray rawHeader = dumpMimeHeader(h);
            bool ok = headerParser.parse(rawHeader);
            if (!ok) {
                qDebug() << QLatin1String("Unspecified error during RFC5322 header parsing");
            } else {
                part->setHdrReferences(headerParser.references);
                if (!headerParser.listPost.isEmpty()) {
                    QList<QUrl> listPost;
                    Q_FOREACH(const QByteArray &item, headerParser.listPost)
                        listPost << QUrl(QString::fromUtf8(item));
                    part->setHdrListPost(listPost);
                }
                if (headerParser.listPostNo)
                    part->setHdrListPostNo(true);
            }
            QByteArray messageId = headerParser.messageId.size() == 1 ? headerParser.messageId.front() : QByteArray();
            part->setEnvelope(std::unique_ptr<Imap::Message::Envelope>(
                                  new Imap::Message::Envelope(date, subject, from, sender, replyTo, to, cc, bcc, headerParser.inReplyTo, messageId)));
        }
        int i = 0;
        for (const mimetic::MimeEntity* child: me.body().parts()) {
            part->setChild(i, mimeEntityToPart(*child, part.get(), i));
            ++i;
        }
        // Remember full part data for part download
        std::stringstream ss;
        ss << me;
        part->setData(ss.str().c_str());
    } else {
        storeInterestingFields(me, part.get());
        QByteArray data;
        Imap::decodeContentTransferEncoding(QByteArray(me.body().data(), me.body().size()), part->data(Imap::Mailbox::RolePartEncoding).toByteArray().toLower(), &data);
        part->setData(data);
    }
    part->setOctets(me.size());
    // FIXME: setSpecialParts()
    return MessagePart::Ptr(std::move(part));
}

}
