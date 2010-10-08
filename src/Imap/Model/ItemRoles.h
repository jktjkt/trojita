/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef IMAP_MODEL_ITEMROLES_H
#define IMAP_MODEL_ITEMROLES_H

#include <Qt>

namespace Imap {

namespace Mailbox {

/** @short Custom item data roles for IMAP */
enum {
    /** @short A "random" offset */
    RoleBase = Qt::UserRole + 666,

    /** @short Name of the mailbox */
    RoleMailboxName,
    /** @short Total number of messages in a mailbox */
    RoleTotalMessageCount,
    /** @short Number of unread messages in a mailbox */
    RoleUnreadMessageCount,
    /** @short The mailbox in question is the INBOX */
    RoleMailboxIsINBOX,
    /** @short The mailbox can be selected */
    RoleMailboxIsSelectable,
    /** @short The mailbox has child mailboxes */
    RoleMailboxHasChildmailboxes,
    /** @short Informaiton about whether the number of messages in the mailbox has already been loaded */
    RoleMailboxNumbersFetched,

    /** @short Subject of the message */
    RoleMessageSubject,
    /** @short The From addresses */
    RoleMessageFrom,
    /** @short The To addresses */
    RoleMessageTo,
    /** @short The Cc addresses */
    RoleMessageCc,
    /** @short The Bcc: addresses */
    RoleMessageBcc,
    /** @short The Sender: header */
    RoleMessageSender,
    /** @short The Reply-To: header */
    RoleMessageReplyTo,
    /** @short The Message-Id: header */
    RoleMessageMessageId,
    /** @short The In-Reply-To: header */
    RoleMessageInReplyTo,
    /** @short The message timestamp */
    RoleMessageDate,
    /** @short Size of the message */
    RoleMessageSize,
    /** @short Status of the \Seen flag */
    RoleMessageIsMarkedRead,
    /** @short Status of the \Deleted flag */
    RoleMessageIsMarkedDeleted,
    /** @short Was the message forwarded? */
    RoleMessageIsMarkedForwarded,
    /** @short Was the message replied to? */
    RoleMessageIsMarkedReplied,
    /** @short Is the message marked as a recent one? */
    RoleMessageIsMarkedRecent };
}

}

#endif // IMAP_MODEL_ITEMROLES_H
