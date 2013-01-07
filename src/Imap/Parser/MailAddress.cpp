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

#include <typeinfo>

#include <QTextDocument>
#include <QUrl>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QUrlQuery>
#endif
#include <QTextCodec>
#include "MailAddress.h"
#include "../Model/MailboxTree.h"
#include "../Encoders.h"
#include "../Parser/Rfc5322HeaderParser.h"

namespace Imap
{
namespace Message
{

bool MailAddress::fromPrettyString(MailAddress &into, const QString &address)
{
    int offset = 0;

    if (!parseOneAddress(into, address, offset))
        return false;

    if (offset < address.size())
        return false;

    return true;
}

/* A simple regexp to match an address typed into the input field. */
static QRegExp mailishRx(QLatin1String("(?:\\b|\\<)([\\w_.-+]+)\\s*\\@\\s*([\\w_.-]+|(?:\\[[^][\\\\\\\"\\s]+\\]))(?:\\b|\\>)"));

/*
   This is of course far from complete, but at least catches "Real
   Name" <foo@bar>.  It needs to recognize the things people actually
   type, and it should also recognize anything that's a valid
   rfc2822 address.
*/
bool MailAddress::parseOneAddress(Imap::Message::MailAddress &into, const QString &address, int &startOffset)
{
    int offset;
    static QRegExp commaRx(QLatin1String("^\\s*(?:,\\s*)*"));

    offset = mailishRx.indexIn(address, startOffset);
    if (offset < 0) {
        /* Try stripping a leading comma? */
        offset = commaRx.indexIn(address, startOffset, QRegExp::CaretAtOffset);
        if (offset < startOffset)
            return false;
        offset += commaRx.matchedLength();
        startOffset = offset;
        offset = mailishRx.indexIn(address, offset);
        if (offset < 0)
            return false;
    }

    QString before = address.mid(startOffset, offset - startOffset);
    into = MailAddress(before.simplified(), QString(), mailishRx.cap(1), mailishRx.cap(2));

    offset += mailishRx.matchedLength();

    int comma = commaRx.indexIn(address, offset, QRegExp::CaretAtOffset);
    if (comma >= offset)
        offset = comma + commaRx.matchedLength();

    startOffset = offset;
    return true;
}

MailAddress::MailAddress(const QVariantList &input, const QByteArray &line, const int start)
{
    // FIXME: all offsets are wrong here
    if (input.size() != 4)
        throw ParseError("MailAddress: not four items", line, start);

    if (input[0].type() != QVariant::ByteArray)
        throw UnexpectedHere("MailAddress: item#1 not a QByteArray", line, start);
    if (input[1].type() != QVariant::ByteArray)
        throw UnexpectedHere("MailAddress: item#2 not a QByteArray", line, start);
    if (input[2].type() != QVariant::ByteArray)
        throw UnexpectedHere("MailAddress: item#3 not a QByteArray", line, start);
    if (input[3].type() != QVariant::ByteArray)
        throw UnexpectedHere("MailAddress: item#4 not a QByteArray", line, start);

    name = Imap::decodeRFC2047String(input[0].toByteArray());
    adl = Imap::decodeRFC2047String(input[1].toByteArray());
    mailbox = Imap::decodeRFC2047String(input[2].toByteArray());
    host = Imap::decodeRFC2047String(input[3].toByteArray());
}

QString MailAddress::prettyName(FormattingMode mode) const
{
    if (name.isEmpty() && mode == FORMAT_JUST_NAME)
        mode = FORMAT_READABLE;

    if (mode == FORMAT_JUST_NAME) {
        return name;
    } else {
        QString address = mailbox + QLatin1Char('@') + host;
        QString result;
        QString niceName;
        if (name.isEmpty()) {
            result = address;
            niceName = address;
        } else {
            result = name + QLatin1String(" <") + address + QLatin1Char('>');
            niceName = name;
        }
        if (mode == FORMAT_READABLE) {
            return result;
        } else {
            QUrl target;
            target.setScheme(QLatin1String("mailto"));
            target.setPath(QString::fromUtf8("%1@%2").arg(mailbox, host));
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            target.addQueryItem(QLatin1String("X-Trojita-DisplayName"), niceName);
            return QString::fromUtf8("<a href=\"%1\">%2</a>").arg(Qt::escape(target.toString()), Qt::escape(niceName));
#else
            QUrlQuery q(target);
            q.addQueryItem(QLatin1String("X-Trojita-DisplayName"), niceName);
            target.setQuery(q);
            return QString::fromUtf8("<a href=\"%1\">%2</a>").arg(target.toString().toHtmlEscaped(), niceName.toHtmlEscaped());
#endif
        }
    }
}

QString MailAddress::prettyList(const QList<MailAddress> &list, FormattingMode mode)
{
    QStringList buf;
    for (QList<MailAddress>::const_iterator it = list.begin(); it != list.end(); ++it)
        buf << it->prettyName(mode);
    return buf.join(QLatin1String(", "));
}

QString MailAddress::prettyList(const QVariantList &list, FormattingMode mode)
{
    QStringList buf;
    for (QVariantList::const_iterator it = list.begin(); it != list.end(); ++it) {
        Q_ASSERT(it->type() == QVariant::StringList);
        QStringList item = it->toStringList();
        Q_ASSERT(item.size() == 4);
        MailAddress a(item[0], item[1], item[2], item[3]);
        buf << a.prettyName(mode);
    }
    return buf.join(QLatin1String(", "));
}

static QRegExp dotAtomRx(QLatin1String("[A-Za-z0-9!#$&'*+/=?^_`{}|~-]+(?:\\.[A-Za-z0-9!#$&'*+/=?^_`{}|~-]+)*"));

/* This returns the address formatted for use in an SMTP MAIL or RCPT command; specifically, it matches the "Mailbox" production of RFC2821. The surrounding angle-brackets are not included. */
QByteArray MailAddress::asSMTPMailbox() const
{
    QByteArray result;

    /* Check whether the local-part contains any characters
       preventing it from being a dot-atom. */
    if (dotAtomRx.exactMatch(mailbox)) {
        /* Using .toLatin1() here even though we know it only contains
           ASCII, because QString.toAscii() does not necessarily convert
           to ASCII (despite the name). .toLatin1() always converts to
           Latin-1. */
        result = mailbox.toLatin1();
    } else {
        /* The other syntax allowed for local-parts is a double-quoted string.
           Note that RFC2047 tokens aren't allowed there --- local-parts are
           fundamentally bytestrings, apparently, whose interpretation is
           up to the receiving system. If someone types non-ASCII characters
           into the address field we'll generate non-conforming headers, but
           it's the best we can do. */
        result = Imap::quotedString(mailbox.toUtf8());
    }
    
    result.append("@");
    
    QByteArray domainpart;

    if (!(host.startsWith(QLatin1Char('[')) || host.endsWith(QLatin1Char(']')))) {
        /* IDN-encode the hostname part of the address */
        domainpart = QUrl::toAce(host);
        
        /* TODO: QUrl::toAce() is documented to return an empty result if
           the string isn't a valid hostname --- for example, if it's a
           domain literal containing an IP address. In that case, we'll
           need to encode it ourselves (making sure there are square
           brackets, no forbidden characters, appropriate backslashes, and so on). */
    }

    if (domainpart.isEmpty()) {
        /* Either the domainpart looks like a domain-literal, or toAce() failed. */
        
        domainpart = host.toUtf8();
        if (domainpart.startsWith('[')) {
            domainpart.remove(0, 1);
        }
        if (domainpart.endsWith(']')) {
            domainpart.remove(domainpart.size()-1, 1);
        }
        
        result.append(Imap::quotedString(domainpart, Imap::SquareBrackets));
    } else {
        result.append(domainpart);
    }

    return result;
}

QByteArray MailAddress::asMailHeader() const
{
    QByteArray result = Imap::encodeRFC2047Phrase(name);

    if (!result.isEmpty())
        result.append(" ");
    
    result.append("<");
    result.append(asSMTPMailbox());
    result.append(">");

    return result;
}

/** @short The mail address usable for manipulation by user */
QString MailAddress::asPrettyString() const
{
    return name.isEmpty() ?
                asSMTPMailbox() :
                name + QLatin1Char(' ') + QLatin1Char('<') + asSMTPMailbox() + QLatin1Char('>');
}

QTextStream &operator<<(QTextStream &stream, const MailAddress &address)
{
    stream << '"' << address.name << "\" <";
    if (!address.host.isNull())
        stream << address.mailbox << '@' << address.host;
    else
        stream << address.mailbox;
    stream << '>';
    return stream;
}

bool operator==(const MailAddress &a, const MailAddress &b)
{
    return a.name == b.name && a.adl == b.adl && a.mailbox == b.mailbox && a.host == b.host;
}

}
}

QDataStream &operator>>(QDataStream &stream, Imap::Message::MailAddress &a)
{
    return stream >> a.adl >> a.host >> a.mailbox >> a.name;
}

QDataStream &operator<<(QDataStream &stream, const Imap::Message::MailAddress &a)
{
    return stream << a.adl << a.host << a.mailbox << a.name;
}
