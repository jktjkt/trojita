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

#ifndef COMPOSER_RECIPIENTS_H
#define COMPOSER_RECIPIENTS_H

#include "Imap/Parser/MailAddress.h"

class QModelIndex;

namespace Composer {

typedef enum {
    REPLY_PRIVATE, /**< @short Reply to "sender(s)" only */
    REPLY_ALL, /**< @short Reply to all recipients */
    REPLY_ALL_BUT_ME, /**< @short Reply to all recipients excluding any of my own identities */
    REPLY_LIST /**< @short Reply to the mailing list */
} ReplyMode;

/** @short Recipients */
typedef enum {
    ADDRESS_TO,
    ADDRESS_CC,
    ADDRESS_BCC,
    ADDRESS_FROM,
    ADDRESS_SENDER,
    ADDRESS_REPLY_TO
} RecipientKind;

typedef QList<QPair<RecipientKind, Imap::Message::MailAddress> > RecipientList;

class SenderIdentitiesModel;

namespace Util {

bool replyRecipientList(const ReplyMode mode, const SenderIdentitiesModel *senderIdetitiesModel,
                        const RecipientList &originalRecipients,
                        const QList<QUrl> &headerListPost, const bool headerListPostNo,
                        RecipientList &output);

bool replyRecipientList(const ReplyMode mode, const SenderIdentitiesModel *senderIdetitiesModel,
                        const QModelIndex &message, RecipientList &output);

bool chooseSenderIdentity(const SenderIdentitiesModel *senderIdetitiesModel,
        const QList<Imap::Message::MailAddress> &addresses, int &row);

bool chooseSenderIdentityForReply(const SenderIdentitiesModel *senderIdetitiesModel, const QModelIndex &message, int &row);

QList<Imap::Message::MailAddress> extractEmailAddresses(const RecipientList &list);
QList<Imap::Message::MailAddress> extractEmailAddresses(const SenderIdentitiesModel *senderIdetitiesModel);
}

}

#endif // COMPOSER_RECIPIENTS_H
