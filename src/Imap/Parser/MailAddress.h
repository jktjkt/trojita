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

#ifndef IMAP_PARSER_MAILADDRESS_H
#define IMAP_PARSER_MAILADDRESS_H

#include <QString>
#include <QVariantList>

class QTextStream;

/** @short Namespace for IMAP interaction */
namespace Imap
{


/** @short Classes for handling e-mail messages */
namespace Message
{

/** @short Storage container for one address from an envelope */
class MailAddress {
public:

    /** @short Mode to format the address to string */
    typedef enum {
        FORMAT_JUST_NAME, /**< @short Just the human-readable name */
        FORMAT_READABLE, /**< @short Real Name <foo@example.org> */
        FORMAT_CLICKABLE /**< @short HTML clickable form of FORMAT_READABLE */
    } FormattingMode;

    /** @short Phrase from RFC2822 mailbox */
    QString name;

    /** @short Route information */
    QString adl;

    /** @short RFC2822 Group Name or Local Part */
    QString mailbox;

    /** @short RFC2822 Domain Name */
    QString host;

    MailAddress(const QString &_name, const QString &_adl,
                const QString &_mailbox, const QString &_host):
        name(_name), adl(_adl), mailbox(_mailbox), host(_host) {}
    MailAddress(const QVariantList &input, const QByteArray &line, const int start);
    MailAddress() {}
    QString prettyName(FormattingMode mode) const;

    QByteArray asSMTPMailbox() const;
    QByteArray asMailHeader() const;
    QString asPrettyString() const;
    QUrl asUrl() const;

    bool hasUsefulDisplayName() const;

    static QString prettyList(const QList<MailAddress> &list, FormattingMode mode);
    static QString prettyList(const QVariantList &list, FormattingMode mode);

    static bool fromPrettyString(MailAddress &into, const QString &address);
    static bool parseOneAddress(MailAddress &into, const QString &address, int &startOffset);
    static bool fromUrl(MailAddress &into, const QUrl &url, const QString &expectedScheme);
};

QTextStream &operator<<(QTextStream &stream, const MailAddress &address);

bool operator==(const MailAddress &a, const MailAddress &b);
inline bool operator!=(const MailAddress &a, const MailAddress &b) { return !(a == b); }


/** Are the actual e-mail addresses (without any fancy details) equal?

These ugly functors are needed as long as we need support for pre-C++11 compilers.
*/
class MailAddressesEqualByMail: public std::binary_function<MailAddress, MailAddress, bool>
{
public:
    result_type operator()(const MailAddress &a, const MailAddress &b) const;
};

/** Are the domains in the e-mail addresses equal?

These ugly functors are needed as long as we need support for pre-C++11 compilers.
*/
class MailAddressesEqualByDomain: public std::binary_function<MailAddress, MailAddress, bool>
{
public:
    result_type operator()(const MailAddress &a, const MailAddress &b) const;
};

/** @short Is the second domain a prefix of the first one?

Insert the usual complaint about lack of C++11 support here.
*/
class MailAddressesEqualByDomainSuffix: public std::binary_function<MailAddress, MailAddress, bool>
{
public:
    result_type operator()(const MailAddress &a, const MailAddress &b) const;
};

}

}

QDataStream &operator>>(QDataStream &stream, Imap::Message::MailAddress &a);
QDataStream &operator<<(QDataStream &stream, const Imap::Message::MailAddress &a);


#endif // IMAP_PARSER_MAILADDRESS_H
