/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

#include <QSet>
#include <QStringList>
#include <QUrl>
#include "Recipients.h"

namespace {

using namespace Composer;

/** @short Eliminate duplicate identities from the list */
RecipientList deduplicated(RecipientList input)
{
    QList<Imap::Message::MailAddress> to, cc, bcc;

    Q_FOREACH(const RecipientList::value_type &recipient, input) {
        switch (recipient.first) {
        case Composer::ADDRESS_TO:
            to << recipient.second;
            break;
        case Composer::ADDRESS_CC:
            cc << recipient.second;
            break;
        case Composer::ADDRESS_BCC:
            bcc << recipient.second;
        }
    }

    // Keep processing the To, Cc and Bcc fields, making sure that no duplicates (where comparing just the addresses) are present
    // in any of them and also making sure that an address is present in at most one of the (To, Cc, Bcc) groups.
    RecipientList result;
    QSet<QPair<QString, QString> > alreadySeen;

    Q_FOREACH(const Imap::Message::MailAddress &addr, to) {
        QPair<QString, QString> item = qMakePair(addr.mailbox, addr.host);
        if (!alreadySeen.contains(item)) {
            result << qMakePair(Composer::ADDRESS_TO, addr);
            alreadySeen.insert(item);
        }
    }

    Q_FOREACH(const Imap::Message::MailAddress &addr, cc) {
        QPair<QString, QString> item = qMakePair(addr.mailbox, addr.host);
        if (!alreadySeen.contains(item)) {
            result << qMakePair(Composer::ADDRESS_CC, addr);
            alreadySeen.insert(item);
        }
    }

    Q_FOREACH(const Imap::Message::MailAddress &addr, bcc) {
        QPair<QString, QString> item = qMakePair(addr.mailbox, addr.host);
        if (!alreadySeen.contains(item)) {
            result << qMakePair(Composer::ADDRESS_BCC, addr);
            alreadySeen.insert(item);
        }
    }

    return result;
}

/** @short Helper: replying to all */
bool prepareReplyAll(const RecipientList &originalRecipients, RecipientList &output)
{
    output = deduplicated(originalRecipients);
    return true;
}

/** @short Helper: replying to sender only */
bool prepareReplySenderOnly(const RecipientList &originalRecipients, RecipientList &output)
{
    // FIXME: implement me
    return false;
}

/** @short Helper: replying to the list */
bool prepareReplyList(const QList<QUrl> &headerListPost, const bool headerListPostNo, RecipientList &output)
{
    if (headerListPostNo)
        return false;

    RecipientList res;
    Q_FOREACH(const QUrl &url, headerListPost) {
        if (url.scheme().toLower() != QLatin1String("mailto"))
            continue;

        QStringList mail = url.path().split(QLatin1Char('@'));
        if (mail.size() != 2)
            continue;

        res << qMakePair(Composer::ADDRESS_TO, Imap::Message::MailAddress(QString(), QString(), mail[0], mail[1]));
    }

    if (!res.isEmpty()) {
        output = deduplicated(res);
        return true;
    }

    return false;
}

}

namespace Composer {
namespace Util {

/** @short Prepare a list of recipients to be used when replying

  @return True if the operation is safe and well-defined, false otherwise (there are situations where
  one cannot be completely sure that the reply will indeed go just to the original author or when replying
  to list is not defined, for example).

*/
bool replyRecipientList(const ReplyMode mode, const RecipientList &originalRecipients,
                        const QList<QUrl> &headerListPost, const bool headerListPostNo,
                        RecipientList &output)
{
    switch (mode) {
    case REPLY_ALL:
        return prepareReplyAll(originalRecipients, output);
    case REPLY_SENDER_ONLY:
        return prepareReplySenderOnly(originalRecipients, output);
    case REPLY_LIST:
        return prepareReplyList(headerListPost, headerListPostNo, output);
    }

    Q_ASSERT(false);
    return false;
}


}
}
