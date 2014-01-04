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

#include <algorithm>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include "Recipients.h"
#include "SenderIdentitiesModel.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

namespace {

using namespace Composer;

/** @short Eliminate duplicate identities from the list and preserve just the To, Cc and Bcc fields */
RecipientList deduplicatedAndJustToCcBcc(RecipientList input)
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
            break;
        case Composer::ADDRESS_FROM:
        case Composer::ADDRESS_SENDER:
        case Composer::ADDRESS_REPLY_TO:
            // that's right, ignore these
            break;
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
            result << qMakePair(result.isEmpty() ? Composer::ADDRESS_TO : Composer::ADDRESS_CC, addr);
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

/** @short Mangle the list of recipients according to the stated rules

  The type of each recipient in the input is checked against the mapping. If the mapping has no record for
  that type, the recipient is discarded, otherwise the kind is adjusted to the desired value.

*/
RecipientList mapRecipients(RecipientList input, const QMap<RecipientKind, RecipientKind>& mapping)
{
    RecipientList::iterator recipient = input.begin();
    while (recipient != input.end()) {
        QMap<RecipientKind, RecipientKind>::const_iterator operation = mapping.constFind(recipient->first);
        if (operation == mapping.constEnd()) {
            recipient = input.erase(recipient);
        } else if (*operation != recipient->first) {
            recipient->first = *operation;
            ++recipient;
        } else {
            // don't modify items which don't need modification
            ++recipient;
        }
    }
    return input;
}

/** @short Replying to all */
bool prepareReplyAll(const RecipientList &originalRecipients, RecipientList &output)
{
    QMap<RecipientKind, RecipientKind> mapping;
    mapping[ADDRESS_FROM] = ADDRESS_TO;
    mapping[ADDRESS_REPLY_TO] = ADDRESS_CC;
    mapping[ADDRESS_TO] = ADDRESS_CC;
    mapping[ADDRESS_CC] = ADDRESS_CC;
    mapping[ADDRESS_BCC] = ADDRESS_BCC;
    RecipientList res = deduplicatedAndJustToCcBcc(mapRecipients(originalRecipients, mapping));
    if (res.isEmpty()) {
        return false;
    } else {
        output = res;
        return true;
    }
}

/** @short Replying to the original author only */
bool prepareReplySenderOnly(const RecipientList &originalRecipients, const QList<QUrl> &headerListPost, RecipientList &output)
{
    // Create a blacklist for the Reply-To filtering. This is needed to work with nasty mailing lists (hey, I run quite
    // a few like that) which do the reply-to munging.
    QList<QPair<QString, QString> > blacklist;
    Q_FOREACH(const QUrl &url, headerListPost) {
        if (url.scheme().toLower() != QLatin1String("mailto")) {
            // non-mail links are not relevant in this situation; they don't mean that we have to give up
            continue;
        }

        QStringList list = url.path().split(QLatin1Char('@'));
        if (list.size() != 2) {
            // Malformed mailto: link, maybe it relies on some fancy routing? Either way, play it safe and refuse to work on that
            return false;
        }

        // FIXME: we actually don't catch the routing!like!this#or#like#this (I don't remember which one is used).
        // The routing shall definitely be checked.

        // FIXME: URL decoding? UTF-8 denormalization?
        blacklist << qMakePair(list[0].toLower(), list[1].toLower());
    }

    // Now gather all addresses from the From and Reply-To headers, taking care to skip munged Reply-To from ML software
    RecipientList originalFrom, originalReplyTo;
    Q_FOREACH(const RecipientList::value_type &recipient, originalRecipients) {
        switch (recipient.first) {
        case ADDRESS_FROM:
            originalFrom << qMakePair(ADDRESS_TO, recipient.second);
            break;
        case ADDRESS_REPLY_TO:
            if (blacklist.contains(qMakePair(recipient.second.mailbox.toLower(), recipient.second.host.toLower()))) {
                // This is the safe situation, this item in the Reply-To is set to a recognized mailing list address.
                // We can safely ignore that.
            } else {
                originalReplyTo << qMakePair(ADDRESS_TO, recipient.second);
            }
            break;
        default:
            break;
        }
    }

    if (!originalReplyTo.isEmpty()) {
        // Prefer replying to the (ML-demunged) Reply-To addresses
        output = originalReplyTo;
        return true;
    } else if (!originalFrom.isEmpty()) {
        // If no usable thing is in the Reply-To, fall back to anything in From
        output = originalFrom;
        return true;
    } else {
        // No recognized addresses -> bail out
        return false;
    }
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
        output = deduplicatedAndJustToCcBcc(res);
        return true;
    }

    return false;
}

/** @short Functor for extracting the second value from a pair

Helper for the Composer::Util::extractEmailAddresses.
*/
template<typename T>
class ExtractSecond: public std::unary_function<T, typename T::second_type> {
public:
   typename T::second_type operator()(const T& value) const
   {
       return value.second;
   }
};

}

namespace Composer {
namespace Util {

/** @short Return a list of all addresses which are found in the message's headers */
RecipientList extractListOfRecipients(const QModelIndex &message)
{
    Composer::RecipientList originalRecipients;
    if (!message.isValid())
        return originalRecipients;

    using namespace Imap::Mailbox;
    using namespace Imap::Message;
    Model *model = dynamic_cast<Model *>(const_cast<QAbstractItemModel *>(message.model()));
    TreeItemMessage *messagePtr = dynamic_cast<TreeItemMessage *>(static_cast<TreeItem *>(message.internalPointer()));
    Q_ASSERT(messagePtr);
    Envelope envelope = messagePtr->envelope(model);

    // Prepare the list of recipients
    Q_FOREACH(const MailAddress &addr, envelope.from)
        originalRecipients << qMakePair(Composer::ADDRESS_FROM, addr);
    Q_FOREACH(const MailAddress &addr, envelope.to)
        originalRecipients << qMakePair(Composer::ADDRESS_TO, addr);
    Q_FOREACH(const MailAddress &addr, envelope.cc)
        originalRecipients << qMakePair(Composer::ADDRESS_CC, addr);
    Q_FOREACH(const MailAddress &addr, envelope.bcc)
        originalRecipients << qMakePair(Composer::ADDRESS_BCC, addr);
    Q_FOREACH(const MailAddress &addr, envelope.sender)
        originalRecipients << qMakePair(Composer::ADDRESS_SENDER, addr);
    Q_FOREACH(const MailAddress &addr, envelope.replyTo)
        originalRecipients << qMakePair(Composer::ADDRESS_REPLY_TO, addr);

    return originalRecipients;
}

/** @short Prepare a list of recipients to be used when replying

  @return True if the operation is safe and well-defined, false otherwise (there are situations where
  one cannot be completely sure that the reply will indeed go just to the original author or when replying
  to list is not defined, for example).

*/
bool replyRecipientList(const ReplyMode mode, const SenderIdentitiesModel *senderIdetitiesModel,
                        const RecipientList &originalRecipients,
                        const QList<QUrl> &headerListPost, const bool headerListPostNo,
                        RecipientList &output)
{
    switch (mode) {
    case REPLY_ALL:
        return prepareReplyAll(originalRecipients, output);
    case REPLY_ALL_BUT_ME:
    {
        RecipientList res = output;
        bool ok = prepareReplyAll(originalRecipients, res);
        if (!ok)
            return false;
        Q_FOREACH(const Imap::Message::MailAddress &addr, extractEmailAddresses(senderIdetitiesModel)) {
            RecipientList::iterator it = res.begin();
            while (it != res.end()) {
                if (Imap::Message::MailAddressesEqualByMail()(it->second, addr)) {
                    // this is our own address
                    it = res.erase(it);
                } else {
                    ++it;
                }
            }
        }
        // We might have deleted something, let's repeat the Cc -> To (...) promotion
        res = deduplicatedAndJustToCcBcc(res);
        if (res.size()) {
            output = res;
            return true;
        } else {
            return false;
        }
    }
    case REPLY_PRIVATE:
        return prepareReplySenderOnly(originalRecipients, headerListPost, output);
    case REPLY_LIST:
        return prepareReplyList(headerListPost, headerListPostNo, output);
    }

    Q_ASSERT(false);
    return false;
}

/** @short Convenience wrapper */
bool replyRecipientList(const ReplyMode mode, const SenderIdentitiesModel *senderIdetitiesModel,
                        const QModelIndex &message, RecipientList &output)
{
    if (!message.isValid())
        return false;

    // Prepare the list of recipients
    RecipientList originalRecipients = extractListOfRecipients(message);

    // The List-Post header
    QList<QUrl> headerListPost;
    Q_FOREACH(const QVariant &item, message.data(Imap::Mailbox::RoleMessageHeaderListPost).toList())
        headerListPost << item.toUrl();

    return replyRecipientList(mode, senderIdetitiesModel, originalRecipients, headerListPost,
                              message.data(Imap::Mailbox::RoleMessageHeaderListPostNo).toBool(), output);
}

QList<Imap::Message::MailAddress> extractEmailAddresses(const SenderIdentitiesModel *senderIdetitiesModel)
{
    using namespace Imap::Message;
    // What identities do we have?
    QList<MailAddress> identities;
    for (int i = 0; i < senderIdetitiesModel->rowCount(); ++i) {
        MailAddress addr;
        MailAddress::fromPrettyString(addr,
                senderIdetitiesModel->data(senderIdetitiesModel->index(i, Composer::SenderIdentitiesModel::COLUMN_EMAIL)).toString());
        identities << addr;
    }
    return identities;
}

/** @short Try to find the preferred identity for a reply looking at a list of recipients */
bool chooseSenderIdentity(const SenderIdentitiesModel *senderIdetitiesModel, const QList<Imap::Message::MailAddress> &addresses, int &row)
{
    using namespace Imap::Message;
    QList<MailAddress> identities = extractEmailAddresses(senderIdetitiesModel);

    // I want to stop this madness. I want C++11.

    // First of all, look for a full match of the sender among the addresses
    for (int i = 0; i < identities.size(); ++i) {
        auto it = std::find_if(addresses.constBegin(), addresses.constEnd(),
                               std::bind2nd(Imap::Message::MailAddressesEqualByMail(), identities[i]));
        if (it != addresses.constEnd()) {
            // Found an exact match of one of our identities in the recipients -> return that
            row = i;
            return true;
        }
    }

    // Then look for the matching domain
    for (int i = 0; i < identities.size(); ++i) {
        auto it = std::find_if(addresses.constBegin(), addresses.constEnd(),
                               std::bind2nd(Imap::Message::MailAddressesEqualByDomain(), identities[i]));
        if (it != addresses.constEnd()) {
            // Found a match because the domain matches -> return that
            row = i;
            return true;
        }
    }

    // Check for situations where the identity's domain is the suffix of some address
    for (int i = 0; i < identities.size(); ++i) {
        auto it = std::find_if(addresses.constBegin(), addresses.constEnd(),
                               std::bind2nd(Imap::Message::MailAddressesEqualByDomainSuffix(), identities[i]));
        if (it != addresses.constEnd()) {
            // Found a match because the domain suffix matches -> return that
            row = i;
            return true;
        }
    }

    // No other heuristic is there for now -> give up
    return false;
}

/** @short Try to find the preferred indetity for replying to a message */
bool chooseSenderIdentityForReply(const SenderIdentitiesModel *senderIdetitiesModel,
                                  const QModelIndex &message, int &row)
{
    return chooseSenderIdentity(senderIdetitiesModel, extractEmailAddresses(extractListOfRecipients(message)), row);
}

/** @short Extract the list of e-mail addresses from the list of <type, address> pairs */
QList<Imap::Message::MailAddress> extractEmailAddresses(const RecipientList &list)
{
    QList<Imap::Message::MailAddress> addresses;
    std::transform(list.constBegin(), list.constEnd(), std::back_inserter(addresses),
                   ExtractSecond<RecipientList::value_type>());
    return addresses;
}

}
}
