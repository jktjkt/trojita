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

#ifndef IMAP_MODEL_ITEMROLES_H
#define IMAP_MODEL_ITEMROLES_H

namespace Imap {
namespace Mailbox {

/** @short Custom item data roles for IMAP */
enum {
    /** @short A "random" offset */
    RoleBase = Qt::UserRole + 666,

    /** @short Is the item already fetched? */
    RoleIsFetched,
    /** @short The item is not available -- perhaps we're offline and it isn't cached */
    RoleIsUnavailable,

    /** @short Name of the mailbox */
    RoleMailboxName,
    /** @short Short name of the mailbox */
    RoleShortMailboxName,
    /** @short Separator for mailboxes at the current level */
    RoleMailboxSeparator,
    /** @short Total number of messages in a mailbox */
    RoleTotalMessageCount,
    /** @short Number of unread messages in a mailbox */
    RoleUnreadMessageCount,
    /** @short Number of recent messages in a mailbox */
    RoleRecentMessageCount,
    /** @short The mailbox in question is the INBOX */
    RoleMailboxIsINBOX,
    /** @short The mailbox can be selected */
    RoleMailboxIsSelectable,
    /** @short The mailbox has child mailboxes */
    RoleMailboxHasChildMailboxes,
    /** @short Information about whether the number of messages in the mailbox has already been loaded */
    RoleMailboxNumbersFetched,
    /** @short Is anything still loading for this mailbox? */
    RoleMailboxItemsAreLoading,
    /** @short Current UIDVALIDITY of a mailbox */
    RoleMailboxUidValidity,
    /** @short Is the mailbox subscribed?

    If the server doesn't support RFC5258, this can return wrong answer.
    */
    RoleMailboxIsSubscribed,

    /** @short UID of the message */
    RoleMessageUid,
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
    /** @short The message timestamp as determined from IMAP's ENVELOPE, ie. from the RFC2822 headers */
    RoleMessageDate,
    /** @short Timestamp of when the message was delivered to the mailbox (ie. IMAP's INTERNALDATE) */
    RoleMessageInternalDate,
    /** @short Size of the message */
    RoleMessageSize,
    /** @short Status of the \\Seen flag */
    RoleMessageIsMarkedRead,
    /** @short Was unread when mailbox opened, or has been marked unread. This flag is transient and is recalculated
     *         when switching mailboxes. */
    RoleMessageWasUnread,
    /** @short Status of the \\Deleted flag */
    RoleMessageIsMarkedDeleted,
    /** @short Was the message forwarded? */
    RoleMessageIsMarkedForwarded,
    /** @short Was the message replied to? */
    RoleMessageIsMarkedReplied,
    /** @short Is the message marked as a recent one? */
    RoleMessageIsMarkedRecent,
    /** @short IMAP flags of a message */
    RoleMessageFlags,
    /** @short Is the current item a root of thread with unread messages */
    RoleThreadRootWithUnreadMessages,
    /** @short Fuzzy date of a particular message; useful for rough navigation */
    RoleMessageFuzzyDate,
    /** @short List of message IDs from the message's References header */
    RoleMessageHeaderReferences,
    /** @short The List-Post header from RFC 2369 */
    RoleMessageHeaderListPost,
    /** @short Is the List-Post set to a special value of "NO"? */
    RoleMessageHeaderListPostNo,
    /** @short A full message envelope */
    RoleMessageEnvelope,

    /** @short Contents of a message part */
    RolePartData,
    /** @short MIME type of a message part */
    RolePartMimeType,
    /** @short Charset of a message part */
    RolePartCharset,
    /** @short The format= parameter of the message part's Content-Type */
    RolePartContentFormat,
    /** @short The delsp= parameter of the message part's Content-Type */
    RolePartContentDelSp,
    /** @short Encoding of a message part */
    RolePartEncoding,
    /** @short The body-fld-id field from BODYSTRUCTURE */
    RolePartBodyFldId,
    /** @short The Content-Disposition of a message part */
    RolePartBodyDisposition,
    /** @short The file name for a message part */
    RolePartFileName,
    /** @short The size of this part, as determined from BODYSTRUCTURE */
    RolePartOctets,
    /** @short Access to the partId() function */
    RolePartId,
    /** @short Access to the partToPath() function */
    RolePartPathToPart,
    /** @short CID of the main part of a multipart/related message */
    RolePartMultipartRelatedMainCid,
    /** @short Is this a top-level multipart, i.e. a multipart/... and a child of a message/rfc822? See isTopLevelMultipart. */
    RolePartIsTopLevelMultipart,

    /** @short True if the item in the tasks list is actually a ParserState

    This role is *not* used or implemented by the IMAP models, but only by the TaskPresentationModel.
    */
    RoleTaskIsParserState,
    /** @short True if the task shall be visible in the user-facing list of current activities

    This role is *not* used or implemented by the IMAP models, but only by the TaskPresentationModel and VisibleTasksModel.
    */
    RoleTaskIsVisible,
    /** @short A short explanaiton of the task -- what is it doing? */
    RoleTaskCompactName,

    /** @short Content-Disposition (inline or attachment) of an attachment within MessageComposer

    The enum value is converted to int.
    */
    RoleAttachmentContentDispositionMode,

    /** @short The very last role */
    RoleInvalidLastOne
};

}
}

#endif // IMAP_MODEL_ITEMROLES_H
