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

#include <Qt>

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
    /** @short Is the message markes as flagged? */
    RoleMessageIsMarkedFlagged,
    /** @short Is the message markes as junk? */
    RoleMessageIsMarkedJunk,
    /** @short Is the message markes as notjunk? */
    RoleMessageIsMarkedNotJunk,
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
    /** @short Is this a mail with at least one attachment?

    The returned value might be a bit fuzzy.
    */
    RoleMessageHasAttachments,

    /** @short Contents of a message part */
    RolePartData,
    /** @short Unicode text, i.e. RolePartData already decoded */
    RolePartUnicodeText,
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
    /** @short Return the body-fld-param from BODUSTRUCTURE, which usually contains some optional MIME parameters about this part */
    RolePartBodyFldParam,

    /** @short Fetch a part from the cache if it's available, but do not request it from the server */
    RolePartForceFetchFromCache,
    /** @short Pointer to the internal buffer */
    RolePartBufferPtr,

    /** @short QModelIndex of the message a part is associated to */
    RolePartMessageIndex,


    /** @short Is the format of this particular multipart/signed supported for signature verification?

    A multipart/signed could use some unrecognized or unsupported algorithm, in which case we won't even try
    to verify the signature. If this is role returns true, it means that there will be just one child item
    and that that child's validity will be checked by the crypto operation. This role does not imply anything
    about the validity of the actual signature, though.
    */
    RolePartSignatureVerifySupported,
    /** @short Is the format of this particular multipart/encrypted supported and recognized?

    See RolePartSignatureVerifySupported, this is an equivalent.
    */
    RolePartDecryptionSupported,
    /** @short Is there any point in waiting longer?

    If true, this means that the crypto code is either waiting for data from the network, or that there is
    a crypto operation in progress.
    */
    RolePartCryptoNotFinishedYet,
    /** @short Was there a failure in some cryptography operation which affected the ability to show the message?

    "Failure" means that something went wrong. Maybe some system component failed, or the message arrived too damaged to
    be decrypted. This state has nothing to do with, say, a message whose signature failed to verify.
    */
    RolePartCryptoDecryptionFailed,
    /** @short Short message about the status/result of a crypto operation

    This is suitable for an immediate presentation to the user. The text should be short enough to not distract
    the user too much, but also descriptive enough to make sense on its own, without having to consult the longer,
    more detailed status message.
    */
    RolePartCryptoTLDR,
    /** @short Longer information about the status/result of a crypto operation

    This can be shown to the user when they ask for more details. It could possibly be a very long text, including some cryptic
    output from gpg's stderr, for example.
    */
    RolePartCryptoDetailedMessage,
    /** @short Icon name for showing the result of a crypto operation */
    RolePartCryptoStatusIconName,
    /** @short Is this a valid signature subject to all checks, whatever they are? */
    RolePartSignatureValidTrusted,
    /** @short Is this a technically valid signature without taking the trust level and other policies into account? */
    RolePartSignatureValidDisregardingTrust,
    /** @short Who made the signature */
    RolePartSignatureSignerName,
    /** @short When was the signature made */
    RolePartSignatureSignDate,


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
