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

#include <typeinfo>

#include <QTextDocument>
#include <QUrl>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QUrlQuery>
#endif
#include <QTextCodec>
#include "Message.h"
#include "MailAddress.h"
#include "LowLevelParser.h"
#include "../Model/MailboxTree.h"
#include "../Encoders.h"
#include "../Parser/Rfc5322HeaderParser.h"

namespace Imap
{
namespace Message
{

/* A simple regexp to match an address typed into the input field. */
static QRegExp mailishRx(QLatin1String("(?:\\b|\\<)([\\w_.-+]+)\\s*\\@\\s*([\\w_.-]+|(?:\\[[^][\\\\\\\"\\s]+\\]))(?:\\b|\\>)"));

QList<MailAddress> Envelope::getListOfAddresses(const QVariant &in, const QByteArray &line, const int start)
{
    if (in.type() == QVariant::ByteArray) {
        if (! in.toByteArray().isNull())
            throw UnexpectedHere("getListOfAddresses: byte array not null", line, start);
    } else if (in.type() != QVariant::List) {
        throw ParseError("getListOfAddresses: not a list", line, start);
    }

    QVariantList list = in.toList();
    QList<MailAddress> res;
    for (QVariantList::const_iterator it = list.constBegin(); it != list.constEnd(); ++it) {
        if (it->type() != QVariant::List)
            throw UnexpectedHere("getListOfAddresses: split item not a list", line, start);   // FIXME: wrong offset
        res.append(MailAddress(it->toList(), line, start));
    }
    return res;
}

Envelope Envelope::fromList(const QVariantList &items, const QByteArray &line, const int start)
{
    if (items.size() != 10)
        throw ParseError("Envelope::fromList: size != 10", line, start);   // FIXME: wrong offset

    // date
    QDateTime date;
    if (items[0].type() == QVariant::ByteArray) {
        QByteArray dateStr = items[0].toByteArray();
        if (! dateStr.isEmpty()) {
            try {
                date = LowLevelParser::parseRFC2822DateTime(dateStr);
            } catch (ParseError &) {
                // FIXME: log this
                //throw ParseError( e.what(), line, start );
            }
        }
    }
    // Otherwise it's "invalid", null.

    QString subject = Imap::decodeRFC2047String(items[1].toByteArray());

    QList<MailAddress> from, sender, replyTo, to, cc, bcc;
    from = Envelope::getListOfAddresses(items[2], line, start);
    sender = Envelope::getListOfAddresses(items[3], line, start);
    replyTo = Envelope::getListOfAddresses(items[4], line, start);
    to = Envelope::getListOfAddresses(items[5], line, start);
    cc = Envelope::getListOfAddresses(items[6], line, start);
    bcc = Envelope::getListOfAddresses(items[7], line, start);

    LowLevelParser::Rfc5322HeaderParser headerParser;

    if (items[8].type() != QVariant::ByteArray)
        throw UnexpectedHere("Envelope::fromList: inReplyTo not a QByteArray", line, start);
    QByteArray inReplyTo = items[8].toByteArray();

    if (items[9].type() != QVariant::ByteArray)
        throw UnexpectedHere("Envelope::fromList: messageId not a QByteArray", line, start);
    QByteArray messageId = items[9].toByteArray();

    QByteArray buf;
    if (!messageId.isEmpty())
        buf += "Message-Id: " + messageId + "\r\n";
    if (!inReplyTo.isEmpty())
        buf += "In-Reply-To: " + inReplyTo + "\r\n";
    if (!buf.isEmpty()) {
        bool ok = headerParser.parse(buf);
        if (!ok) {
            qDebug() << "Envelope::fromList: malformed headers";
        }
    }
    // If the Message-Id fails to parse, well, bad luck. This enforced sanitizaion is hopefully better than
    // generating garbage in outgoing e-mails.
    messageId = headerParser.messageId.size() == 1 ? headerParser.messageId.front() : QByteArray();

    return Envelope(date, subject, from, sender, replyTo, to, cc, bcc, headerParser.inReplyTo, messageId);
}

void Envelope::clear()
{
    date = QDateTime();
    subject.clear();
    from.clear();
    sender.clear();
    replyTo.clear();
    to.clear();
    cc.clear();
    bcc.clear();
    inReplyTo.clear();
    messageId.clear();
}

bool OneMessage::eq(const AbstractData &other) const
{
    try {
        const OneMessage &o = dynamic_cast<const OneMessage &>(other);
        return o.mediaType == mediaType && mediaSubType == o.mediaSubType &&
               bodyFldParam == o.bodyFldParam && bodyFldId == o.bodyFldId &&
               bodyFldDesc == o.bodyFldDesc && bodyFldEnc == o.bodyFldEnc &&
               bodyFldOctets == o.bodyFldOctets && bodyFldMd5 == o.bodyFldMd5 &&
               bodyFldDsp == o.bodyFldDsp && bodyFldLang == o.bodyFldLang &&
               bodyFldLoc == o.bodyFldLoc && bodyExtension == o.bodyExtension;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool TextMessage::eq(const AbstractData &other) const
{
    try {
        const TextMessage &o = dynamic_cast<const TextMessage &>(other);
        return OneMessage::eq(o) && bodyFldLines == o.bodyFldLines;
    } catch (std::bad_cast &) {
        return false;
    }
}

QTextStream &TextMessage::dump(QTextStream &s, const int indent) const
{
    QByteArray i(indent + 1, ' ');
    QByteArray lf("\n");

    return s << QByteArray(indent, ' ') << "TextMessage( " << mediaType << "/" << mediaSubType << lf <<
           i << "body-fld-param: " << bodyFldParam << lf <<
           i << "body-fld-id: " << bodyFldId << lf <<
           i << "body-fld-desc: " << bodyFldDesc << lf <<
           i << "body-fld-enc: " << bodyFldEnc << lf <<
           i << "body-fld-octets: " << bodyFldOctets << lf <<
           i << "bodyFldMd5: " << bodyFldMd5 << lf <<
           i << "body-fld-dsp: " << bodyFldDsp << lf <<
           i << "body-fld-lang: " << bodyFldLang << lf <<
           i << "body-fld-loc: " << bodyFldLoc << lf <<
           i << "body-extension is " << bodyExtension.typeName() << lf <<
           i << "body-fld-lines: " << bodyFldLines << lf <<
           QByteArray(indent, ' ') << ")";
    // FIXME: operator<< for QVariant...
}

bool MsgMessage::eq(const AbstractData &other) const
{
    try {
        const MsgMessage &o = dynamic_cast<const MsgMessage &>(other);
        if (o.body) {
            if (body) {
                if (*body != *o.body) {
                    return false;
                }
            } else {
                return false;
            }
        } else if (body) {
            return false;
        }

        return OneMessage::eq(o) && bodyFldLines == o.bodyFldLines &&
               envelope == o.envelope;

    } catch (std::bad_cast &) {
        return false;
    }
}

QTextStream &MsgMessage::dump(QTextStream &s, const int indent) const
{
    QByteArray i(indent + 1, ' ');
    QByteArray lf("\n");

    s << QByteArray(indent, ' ') << "MsgMessage(" << lf;
    envelope.dump(s, indent + 1);
    s <<
      i << "body-fld-lines " << bodyFldLines << lf <<
      i << "body:" << lf;

    s <<
      i << "body-fld-param: " << bodyFldParam << lf <<
      i << "body-fld-id: " << bodyFldId << lf <<
      i << "body-fld-desc: " << bodyFldDesc << lf <<
      i << "body-fld-enc: " << bodyFldEnc << lf <<
      i << "body-fld-octets: " << bodyFldOctets << lf <<
      i << "bodyFldMd5: " << bodyFldMd5 << lf <<
      i << "body-fld-dsp: " << bodyFldDsp << lf <<
      i << "body-fld-lang: " << bodyFldLang << lf <<
      i << "body-fld-loc: " << bodyFldLoc << lf <<
      i << "body-extension is " << bodyExtension.typeName() << lf;

    if (body)
        body->dump(s, indent + 2);
    else
        s << i << " (null)";
    return s << lf << QByteArray(indent, ' ') << ")";
}

QTextStream &BasicMessage::dump(QTextStream &s, const int indent) const
{
    QByteArray i(indent + 1, ' ');
    QByteArray lf("\n");

    return s << QByteArray(indent, ' ') << "BasicMessage( " << mediaType << "/" << mediaSubType << lf <<
           i << "body-fld-param: " << bodyFldParam << lf <<
           i << "body-fld-id: " << bodyFldId << lf <<
           i << "body-fld-desc: " << bodyFldDesc << lf <<
           i << "body-fld-enc: " << bodyFldEnc << lf <<
           i << "body-fld-octets: " << bodyFldOctets << lf <<
           i << "bodyFldMd5: " << bodyFldMd5 << lf <<
           i << "body-fld-dsp: " << bodyFldDsp << lf <<
           i << "body-fld-lang: " << bodyFldLang << lf <<
           i << "body-fld-loc: " << bodyFldLoc << lf <<
           i << "body-extension is " << bodyExtension.typeName() << lf <<
           QByteArray(indent, ' ') << ")";
    // FIXME: operator<< for QVariant...
}

bool MultiMessage::eq(const AbstractData &other) const
{
    try {
        const MultiMessage &o = dynamic_cast<const MultiMessage &>(other);

        if (bodies.count() != o.bodies.count()) {
            return false;
        }

        for (int i = 0; i < bodies.count(); ++i) {
            if (bodies[i]) {
                if (o.bodies[i]) {
                    if (*bodies[i] != *o.bodies[i]) {
                        return false;
                    }
                } else {
                    return false;
                }
            } else if (! o.bodies[i]) {
                return false;
            }
        }

        return mediaSubType == o.mediaSubType && bodyFldParam == o.bodyFldParam &&
               bodyFldDsp == o.bodyFldDsp && bodyFldLang == o.bodyFldLang &&
               bodyFldLoc == o.bodyFldLoc && bodyExtension == o.bodyExtension;

    } catch (std::bad_cast &) {
        return false;
    }
}

QTextStream &MultiMessage::dump(QTextStream &s, const int indent) const
{
    QByteArray i(indent + 1, ' ');
    QByteArray lf("\n");

    s << QByteArray(indent, ' ') << "MultiMessage( multipart/" << mediaSubType << lf <<
      i << "body-fld-param " << bodyFldParam << lf <<
      i << "body-fld-dsp " << bodyFldDsp << lf <<
      i << "body-fld-lang " << bodyFldLang << lf <<
      i << "body-fld-loc " << bodyFldLoc << lf <<
      i << "bodyExtension is " << bodyExtension.typeName() << lf <<
      i << "bodies: [ " << lf;

    for (QList<QSharedPointer<AbstractMessage> >::const_iterator it = bodies.begin(); it != bodies.end(); ++it)
        if (*it) {
            (**it).dump(s, indent + 2);
            s << lf;
        } else
            s << i << " (null)" << lf;

    return s << QByteArray(indent, ' ') << "] )";
}

AbstractMessage::bodyFldParam_t AbstractMessage::makeBodyFldParam(const QVariant &input, const QByteArray &line, const int start)
{
    bodyFldParam_t map;
    if (input.type() != QVariant::List) {
        if (input.type() == QVariant::ByteArray && input.toByteArray().isNull())
            return map;
        throw UnexpectedHere("body-fld-param: not a list / nil", line, start);
    }
    QVariantList list = input.toList();
    if (list.size() % 2)
        throw UnexpectedHere("body-fld-param: wrong number of entries", line, start);
    for (int j = 0; j < list.size(); j += 2)
        if (list[j].type() != QVariant::ByteArray || list[j+1].type() != QVariant::ByteArray)
            throw UnexpectedHere("body-fld-param: string not found", line, start);
        else
            map[ list[j].toByteArray().toUpper() ] = list[j+1].toByteArray();
    return map;
}

AbstractMessage::bodyFldDsp_t AbstractMessage::makeBodyFldDsp(const QVariant &input, const QByteArray &line, const int start)
{
    bodyFldDsp_t res;

    if (input.type() != QVariant::List) {
        if (input.type() == QVariant::ByteArray && input.toByteArray().isNull())
            return res;
        throw UnexpectedHere("body-fld-dsp: not a list / nil", line, start);
    }
    QVariantList list = input.toList();
    if (list.size() != 2)
        throw ParseError("body-fld-dsp: wrong number of entries in the list", line, start);
    if (list[0].type() != QVariant::ByteArray)
        throw UnexpectedHere("body-fld-dsp: first item is not a string", line, start);
    res.first = list[0].toByteArray();
    res.second = makeBodyFldParam(list[1], line, start);
    return res;
}

QList<QByteArray> AbstractMessage::makeBodyFldLang(const QVariant &input, const QByteArray &line, const int start)
{
    QList<QByteArray> res;
    if (input.type() == QVariant::ByteArray) {
        if (input.toByteArray().isNull())   // handle NIL
            return res;
        res << input.toByteArray();
    } else if (input.type() == QVariant::List) {
        QVariantList list = input.toList();
        for (QVariantList::const_iterator it = list.constBegin(); it != list.constEnd(); ++it)
            if (it->type() != QVariant::ByteArray)
                throw UnexpectedHere("body-fld-lang has wrong structure", line, start);
            else
                res << it->toByteArray();
    } else
        throw UnexpectedHere("body-fld-lang not found", line, start);
    return res;
}

uint AbstractMessage::extractUInt(const QVariant &var, const QByteArray &line, const int start)
{
    if (var.type() == QVariant::UInt)
        return var.toUInt();
    if (var.type() == QVariant::ByteArray) {
        bool ok = false;
        int number = var.toInt(&ok);
        if (ok) {
            if (number >= 0) {
                return number;
            } else {
                qDebug() << "Parser warning:" << number << "is not an unsigned int";
                return 0;
            }
        } else if (var.toByteArray().isEmpty()) {
            qDebug() << "Parser warning: expected unsigned int, but got NIL or an empty string instead, yuck";
            return 0;
        } else {
            throw UnexpectedHere("extractUInt: not a number", line, start);
        }
    }
    throw UnexpectedHere("extractUInt: weird data type", line, start);
}


QSharedPointer<AbstractMessage> AbstractMessage::fromList(const QVariantList &items, const QByteArray &line, const int start)
{
    if (items.size() < 2)
        throw NoData("AbstractMessage::fromList: no data", line, start);

    if (items[0].type() == QVariant::ByteArray) {
        // it's a single-part message, hurray

        int i = 0;
        QString mediaType = items[i].toString().toLower();
        ++i;
        QString mediaSubType = items[i].toString().toLower();
        ++i;

        if (items.size() < 7) {
            qDebug() << "AbstractMessage::fromList(): body-type-basic(?): yuck, too few items, using what we've got";
        }

        bodyFldParam_t bodyFldParam;
        if (i < items.size()) {
            bodyFldParam = makeBodyFldParam(items[i], line, start);
            ++i;
        }

        QByteArray bodyFldId;
        if (i < items.size()) {
            if (items[i].type() != QVariant::ByteArray)
                throw UnexpectedHere("body-fld-id not recognized as a ByteArray", line, start);
            bodyFldId = items[i].toByteArray();
            ++i;
        }

        QByteArray bodyFldDesc;
        if (i < items.size()) {
            if (items[i].type() != QVariant::ByteArray)
                throw UnexpectedHere("body-fld-desc not recognized as a ByteArray", line, start);
            bodyFldDesc = items[i].toByteArray();
            ++i;
        }

        QByteArray bodyFldEnc;
        if (i < items.size()) {
            if (items[i].type() != QVariant::ByteArray)
                throw UnexpectedHere("body-fld-enc not recognized as a ByteArray", line, start);
            bodyFldEnc = items[i].toByteArray();
            ++i;
        }

        uint bodyFldOctets = 0;
        if (i < items.size()) {
            bodyFldOctets = extractUInt(items[i], line, start);
            ++i;
        }

        uint bodyFldLines = 0;
        Envelope envelope;
        QSharedPointer<AbstractMessage> body;

        enum { MESSAGE, TEXT, BASIC} kind;

        if (mediaType == QLatin1String("message") && mediaSubType == QLatin1String("rfc822")) {
            // extract envelope, body, body-fld-lines

            if (items.size() < 10)
                throw NoData("too few fields for a Message-message", line, start);

            kind = MESSAGE;
            if (items[i].type() == QVariant::ByteArray && items[i].toByteArray().isEmpty()) {
                // ENVELOPE is NIL, this shouldn't really happen
                qDebug() << "AbstractMessage::fromList(): message/rfc822: yuck, got NIL for envelope";
            } else if (items[i].type() != QVariant::List) {
                throw UnexpectedHere("message/rfc822: envelope not a list", line, start);
            } else {
                envelope = Envelope::fromList(items[i].toList(), line, start);
            }
            ++i;

            if (items[i].type() != QVariant::List)
                throw UnexpectedHere("message/rfc822: body not recognized as a list", line, start);
            body = AbstractMessage::fromList(items[i].toList(), line, start);
            ++i;

            try {
                bodyFldLines = extractUInt(items[i], line, start);
            } catch (const UnexpectedHere &) {
                qDebug() << "AbstractMessage::fromList(): message/rfc822: yuck, invalid body-fld-lines";
            }
            ++i;

        } else if (mediaType == QLatin1String("text")) {
            kind = TEXT;
            if (i < items.size()) {
                // extract body-fld-lines
                bodyFldLines = extractUInt(items[i], line, start);
                ++i;
            }
        } else {
            // don't extract anything as we're done here
            kind = BASIC;
        }

        // extract body-ext-1part

        // body-fld-md5
        QByteArray bodyFldMd5;
        if (i < items.size()) {
            if (items[i].type() != QVariant::ByteArray)
                throw UnexpectedHere("body-fld-md5 not a ByteArray", line, start);
            bodyFldMd5 = items[i].toByteArray();
            ++i;
        }

        // body-fld-dsp
        bodyFldDsp_t bodyFldDsp;
        if (i < items.size()) {
            bodyFldDsp = makeBodyFldDsp(items[i], line, start);
            ++i;
        }

        // body-fld-lang
        QList<QByteArray> bodyFldLang;
        if (i < items.size()) {
            bodyFldLang = makeBodyFldLang(items[i], line, start);
            ++i;
        }

        // body-fld-loc
        QByteArray bodyFldLoc;
        if (i < items.size()) {
            if (items[i].type() != QVariant::ByteArray)
                throw UnexpectedHere("body-fld-loc not found", line, start);
            bodyFldLoc = items[i].toByteArray();
            ++i;
        }

        // body-extension
        QVariant bodyExtension;
        if (i < items.size()) {
            if (i == items.size() - 1) {
                bodyExtension = items[i];
                ++i;
            } else {
                QVariantList list;
                for (; i < items.size(); ++i)
                    list << items[i];
                bodyExtension = list;
            }
        }

        switch (kind) {
        case MESSAGE:
            return QSharedPointer<AbstractMessage>(
                       new MsgMessage(mediaType, mediaSubType, bodyFldParam,
                                      bodyFldId, bodyFldDesc, bodyFldEnc, bodyFldOctets,
                                      bodyFldMd5, bodyFldDsp, bodyFldLang, bodyFldLoc,
                                      bodyExtension, envelope, body, bodyFldLines)
                   );
        case TEXT:
            return QSharedPointer<AbstractMessage>(
                       new TextMessage(mediaType, mediaSubType, bodyFldParam,
                                       bodyFldId, bodyFldDesc, bodyFldEnc, bodyFldOctets,
                                       bodyFldMd5, bodyFldDsp, bodyFldLang, bodyFldLoc,
                                       bodyExtension, bodyFldLines)
                   );
        case BASIC:
        default:
            return QSharedPointer<AbstractMessage>(
                       new BasicMessage(mediaType, mediaSubType, bodyFldParam,
                                        bodyFldId, bodyFldDesc, bodyFldEnc, bodyFldOctets,
                                        bodyFldMd5, bodyFldDsp, bodyFldLang, bodyFldLoc,
                                        bodyExtension)
                   );
        }

    } else if (items[0].type() == QVariant::List) {

        if (items.size() < 2)
            throw ParseError("body-type-mpart: structure should be \"body* string\"", line, start);

        int i = 0;

        QList<QSharedPointer<AbstractMessage> > bodies;
        while (items[i].type() == QVariant::List) {
            bodies << fromList(items[i].toList(), line, start);
            ++i;
        }

        if (items[i].type() != QVariant::ByteArray)
            throw UnexpectedHere("body-type-mpart: media-subtype not recognized", line, start);
        QString mediaSubType = items[i].toString().toLower();
        ++i;

        // body-ext-mpart

        // body-fld-param
        bodyFldParam_t bodyFldParam;
        if (i < items.size()) {
            bodyFldParam = makeBodyFldParam(items[i], line, start);
            ++i;
        }

        // body-fld-dsp
        bodyFldDsp_t bodyFldDsp;
        if (i < items.size()) {
            bodyFldDsp = makeBodyFldDsp(items[i], line, start);
            ++i;
        }

        // body-fld-lang
        QList<QByteArray> bodyFldLang;
        if (i < items.size()) {
            bodyFldLang = makeBodyFldLang(items[i], line, start);
            ++i;
        }

        // body-fld-loc
        QByteArray bodyFldLoc;
        if (i < items.size()) {
            if (items[i].type() != QVariant::ByteArray)
                throw UnexpectedHere("body-fld-loc not found", line, start);
            bodyFldLoc = items[i].toByteArray();
            ++i;
        }

        // body-extension
        QVariant bodyExtension;
        if (i < items.size()) {
            if (i == items.size() - 1) {
                bodyExtension = items[i];
                ++i;
            } else {
                QVariantList list;
                for (; i < items.size(); ++i)
                    list << items[i];
                bodyExtension = list;
            }
        }

        return QSharedPointer<AbstractMessage>(
                   new MultiMessage(bodies, mediaSubType, bodyFldParam,
                                    bodyFldDsp, bodyFldLang, bodyFldLoc, bodyExtension));
    } else {
        throw UnexpectedHere("AbstractMessage::fromList: invalid data type of first item", line, start);
    }
}

void dumpListOfAddresses(QTextStream &stream, const QList<MailAddress> &list, const int indent)
{
    QByteArray lf("\n");
    switch (list.size()) {
    case 0:
        stream << "[ ]" << lf;
        break;
    case 1:
        stream << "[ " << list.front() << " ]" << lf;
        break;
    default: {
        QByteArray i(indent + 1, ' ');
        stream << "[" << lf;
        for (QList<MailAddress>::const_iterator it = list.begin(); it != list.end(); ++it)
            stream << i << *it << lf;
        stream << QByteArray(indent, ' ') << "]" << lf;
    }
    }
}

QTextStream &Envelope::dump(QTextStream &stream, const int indent) const
{
    QByteArray i(indent + 1, ' ');
    QByteArray lf("\n");
    stream << QByteArray(indent, ' ') << "Envelope(" << lf <<
           i << "Date: " << date.toString() << lf <<
           i << "Subject: " << subject << lf;
    stream << i << "From: "; dumpListOfAddresses(stream, from, indent + 1);
    stream << i << "Sender: "; dumpListOfAddresses(stream, sender, indent + 1);
    stream << i << "Reply-To: "; dumpListOfAddresses(stream, replyTo, indent + 1);
    stream << i << "To: "; dumpListOfAddresses(stream, to, indent + 1);
    stream << i << "Cc: "; dumpListOfAddresses(stream, cc, indent + 1);
    stream << i << "Bcc: "; dumpListOfAddresses(stream, bcc, indent + 1);
    stream <<
           i << "In-Reply-To: " << inReplyTo << lf <<
           i << "Message-Id: " << messageId << lf;
    return stream << QByteArray(indent, ' ') << ")" << lf;
}

QTextStream &operator<<(QTextStream &stream, const Envelope &e)
{
    return e.dump(stream, 0);
}

QTextStream &operator<<(QTextStream &stream, const AbstractMessage::bodyFldParam_t &p)
{
    stream << "bodyFldParam[ ";
    bool first = true;
    for (AbstractMessage::bodyFldParam_t::const_iterator it = p.begin(); it != p.end(); ++it, first = false)
        stream << (first ? "" : ", ") << it.key() << ": " << it.value();
    return stream << "]";
}

QTextStream &operator<<(QTextStream &stream, const AbstractMessage::bodyFldDsp_t &p)
{
    return stream << "bodyFldDsp( " << p.first << ", " << p.second << ")";
}

QTextStream &operator<<(QTextStream &stream, const QList<QByteArray> &list)
{
    stream << "( ";
    bool first = true;
    for (QList<QByteArray>::const_iterator it = list.begin(); it != list.end(); ++it, first = false)
        stream << (first ? "" : ", ") << *it;
    return stream << " )";
}

bool operator==(const Envelope &a, const Envelope &b)
{
    return a.date == b.date && a.subject == b.subject &&
           a.from == b.from && a.sender == b.sender && a.replyTo == b.replyTo &&
           a.to == b.to && a.cc == b.cc && a.bcc == b.bcc &&
           a.inReplyTo == b.inReplyTo && a.messageId == b.messageId;
}

/** @short Extract interesting part-specific metadata from the BODYSTRUCTURE into the actual part

Examples are stuff like the charset, or the suggested filename.
*/
void AbstractMessage::storeInterestingFields(Mailbox::TreeItemPart *p) const
{
    // Charset
    bodyFldParam_t::const_iterator it = bodyFldParam.find("CHARSET");
    if (it != bodyFldParam.end()) {
        p->setCharset(*it);
    }

    // Support for format=flowed, RFC3676
    it = bodyFldParam.find("FORMAT");
    if (it != bodyFldParam.end()) {
        p->setContentFormat(*it);

        it = bodyFldParam.find("DELSP");
        if (it != bodyFldParam.end()) {
            p->setContentDelSp(*it);
        }
    }

    // Filename and content-disposition
    if (!bodyFldDsp.first.isNull()) {
        p->setBodyDisposition(bodyFldDsp.first);
        QString filename = Imap::extractRfc2231Param(bodyFldDsp.second, "FILENAME");
        if (filename.isEmpty()) {
            filename = Imap::extractRfc2231Param(bodyFldParam, "NAME");
        }
        p->setFileName(filename);
    }
}

void OneMessage::storeInterestingFields(Mailbox::TreeItemPart *p) const
{
    AbstractMessage::storeInterestingFields(p);
    p->setEncoding(bodyFldEnc.toLower());
    p->setOctets(bodyFldOctets);
    p->setBodyFldId(bodyFldId);
}

void MultiMessage::storeInterestingFields(Mailbox::TreeItemPart *p) const
{
    AbstractMessage::storeInterestingFields(p);

    // The multipart/related can specify the root part to show
    if (mediaSubType == QLatin1String("related")) {
        bodyFldParam_t::const_iterator it = bodyFldParam.find("START");
        if (it != bodyFldParam.end()) {
            p->setMultipartRelatedStartPart(*it);
        }
    }
}

Mailbox::TreeItemChildrenList TextMessage::createTreeItems(Mailbox::TreeItem *parent) const
{
    Mailbox::TreeItemChildrenList list;
    Mailbox::TreeItemPart *p = new Mailbox::TreeItemPart(parent, QString("%1/%2").arg(mediaType, mediaSubType));
    storeInterestingFields(p);
    list << p;
    return list;
}

Mailbox::TreeItemChildrenList BasicMessage::createTreeItems(Mailbox::TreeItem *parent) const
{
    Mailbox::TreeItemChildrenList list;
    Mailbox::TreeItemPart *p = new Mailbox::TreeItemPart(parent, QString("%1/%2").arg(mediaType, mediaSubType));
    storeInterestingFields(p);
    list << p;
    return list;
}

Mailbox::TreeItemChildrenList MsgMessage::createTreeItems(Mailbox::TreeItem *parent) const
{
    Mailbox::TreeItemChildrenList list;
    Mailbox::TreeItemPart *part = new Mailbox::TreeItemPartMultipartMessage(parent, envelope);
    part->setChildren(body->createTreeItems(part));     // always returns an empty list -> no need to qDeleteAll()
    storeInterestingFields(part);
    list << part;
    return list;
}

Mailbox::TreeItemChildrenList MultiMessage::createTreeItems(Mailbox::TreeItem *parent) const
{
    Mailbox::TreeItemChildrenList list, list2;
    Mailbox::TreeItemPart *part = new Mailbox::TreeItemPart(parent, QString("multipart/%1").arg(mediaSubType));
    for (QList<QSharedPointer<AbstractMessage> >::const_iterator it = bodies.begin(); it != bodies.end(); ++it) {
        list2 << (*it)->createTreeItems(part);
    }
    part->setChildren(list2);   // always returns an empty list -> no need to qDeleteAll()
    storeInterestingFields(part);
    list << part;
    return list;
}

}
}

QDebug operator<<(QDebug dbg, const Imap::Message::Envelope &envelope)
{
    using namespace Imap::Message;
    return dbg << "Envelope( FROM" << MailAddress::prettyList(envelope.from, MailAddress::FORMAT_READABLE) <<
           "TO" << MailAddress::prettyList(envelope.to, MailAddress::FORMAT_READABLE) <<
           "CC" << MailAddress::prettyList(envelope.cc, MailAddress::FORMAT_READABLE) <<
           "BCC" << MailAddress::prettyList(envelope.bcc, MailAddress::FORMAT_READABLE) <<
           "SUBJECT" << envelope.subject <<
           "DATE" << envelope.date <<
           "MESSAGEID" << envelope.messageId;
}

QDataStream &operator>>(QDataStream &stream, Imap::Message::Envelope &e)
{
    return stream >> e.bcc >> e.cc >> e.date >> e.from >> e.inReplyTo >>
           e.messageId >> e.replyTo >> e.sender >> e.subject >> e.to;
}

QDataStream &operator<<(QDataStream &stream, const Imap::Message::Envelope &e)
{
    return stream << e.bcc << e.cc << e.date << e.from << e.inReplyTo <<
           e.messageId << e.replyTo << e.sender << e.subject << e.to;
}
