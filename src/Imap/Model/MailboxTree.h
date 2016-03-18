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

#ifndef IMAP_MAILBOXTREE_H
#define IMAP_MAILBOXTREE_H

#include <memory>
#include <QList>
#include <QModelIndex>
#include <QPointer>
#include <QString>
#include "../Parser/Response.h"
#include "../Parser/Message.h"
#include "MailboxMetadata.h"

namespace Imap
{

namespace Mailbox
{

class Model;
class MailboxModel;
class KeepMailboxOpenTask;
class ListChildMailboxesTask;

class TreeItem
{
    friend class Model; // for m_loading and m_fetched
    TreeItem(const TreeItem &); // don't implement
    void operator=(const TreeItem &);  // don't implement
    friend class DeleteMailboxTask; // for direct access to m_children
    friend class ObtainSynchronizedMailboxTask;
    friend class KeepMailboxOpenTask; // for direct access to m_children
    friend class ListChildMailboxesTask; // setStatus() in case of failure
    friend class MsgListModel; // for direct access to m_children
    friend class ThreadingMsgListModel; // for direct access to m_children
    friend class UpdateFlagsOfAllMessagesTask; // for direct access to m_children

protected:
    /** @short Availability of an item */
    enum FetchingState {
        NONE, /**< @short No attempt to download an item has been made yet */
        UNAVAILABLE, /**< @short Item isn't cached and remote requests are disabled */
        LOADING, /**< @short Download of an item is already scheduled */
        DONE /**< @short Item is available right now */
    };

public:
    typedef enum {
        /** @short Full body of an e-mail stored on the IMAP server

          This one really makes sense on a TreeItemMessage and TreeItemPart, and
          are used
        */
        /** @short The HEADER fetch modifier for the current item */
        OFFSET_HEADER=1,
        /** @short The TEXT fetch modifier for the current item */
        OFFSET_TEXT=2,
        /** @short The MIME fetch modifier for individual message parts

          In constrast to OFFSET_HEADER and OFFSET_TEXT, this one applies
          only to TreeItemPart, simply because using the MIME modifier on
          a top-level message is not allowed as per RFC 3501.
        */
        OFFSET_MIME=3,
        /** @short Obtain the raw data without any kind of Content-Transfer-Encoding decoding */
        OFFSET_RAW_CONTENTS = 4
    } PartModifier;

protected:
    static const intptr_t TagMask = 0x3;
    static const intptr_t PointerMask = ~TagMask;
    union {
        TreeItem *m_parent;
        intptr_t m_parentAsBits;
    };
    TreeItemChildrenList m_children;

    FetchingState accessFetchStatus() const
    {
        return static_cast<FetchingState>(m_parentAsBits & TagMask);
    }
    void setFetchStatus(const FetchingState fetchStatus)
    {
        m_parentAsBits = reinterpret_cast<intptr_t>(parent()) | fetchStatus;
    }
public:
    explicit TreeItem(TreeItem *parent);
    TreeItem *parent() const
    {
        return reinterpret_cast<TreeItem *>(m_parentAsBits & PointerMask);
    }
    virtual int row() const;

    virtual ~TreeItem();
    virtual unsigned int childrenCount(Model *const model);
    virtual TreeItem *child(const int offset, Model *const model);
    virtual TreeItemChildrenList setChildren(const TreeItemChildrenList &items);
    virtual void fetch(Model *const model) = 0;
    virtual unsigned int rowCount(Model *const model) = 0;
    virtual unsigned int columnCount();
    virtual QVariant data(Model *const model, int role) = 0;
    virtual bool hasChildren(Model *const model) = 0;
    virtual bool fetched() const { return accessFetchStatus() == DONE; }
    virtual bool loading() const { return accessFetchStatus() == LOADING; }
    virtual bool isUnavailable() const;
    virtual TreeItem *specialColumnPtr(int row, int column) const;
    virtual QModelIndex toIndex(Model *const model) const;
};

class TreeItemPart;
class TreeItemMessage;

class TreeItemMailbox: public TreeItem
{
    void operator=(const TreeItem &);  // don't implement
    MailboxMetadata m_metadata;
    friend class Model; // needs access to maintianingTask
    friend class MailboxModel;
    friend class DeleteMailboxTask; // for direct access to maintainingTask
    friend class KeepMailboxOpenTask; // needs access to maintainingTask
    friend class SubscribeUnsubscribeTask; // needs access to m_metadata.flags
    static QLatin1String flagNoInferiors;
    static QLatin1String flagHasNoChildren;
    static QLatin1String flagHasChildren;
public:
    explicit TreeItemMailbox(TreeItem *parent);
    TreeItemMailbox(TreeItem *parent, Responses::List);
    ~TreeItemMailbox();

    static TreeItemMailbox *fromMetadata(TreeItem *parent, const MailboxMetadata &metadata);

    virtual TreeItemChildrenList setChildren(const TreeItemChildrenList &items);
    virtual void fetch(Model *const model);
    virtual void fetchWithCacheControl(Model *const model, bool forceReload);
    virtual unsigned int rowCount(Model *const model);
    virtual QVariant data(Model *const model, int role);
    virtual bool hasChildren(Model *const model);
    virtual TreeItem *child(const int offset, Model *const model);

    SyncState syncState;

    /** @short Returns true if this mailbox has child mailboxes

    This function might access the network if the answer can't be decided, for example on basis of mailbox flags.
    */
    bool hasChildMailboxes(Model *const model);
    /** @short Return true if the mailbox is already known to not have any child mailboxes

    No network activity will be caused. If the answer is not known for sure, we return false (meaning "don't know").
    */
    bool hasNoChildMailboxesAlreadyKnown();

    QString mailbox() const { return m_metadata.mailbox; }
    QString separator() const { return m_metadata.separator; }
    const MailboxMetadata &mailboxMetadata() const { return m_metadata; }
    /** @short Update internal tree with the results of a FETCH response

      If \a changedPart is not null, it will be updated to point to the message
      part whose content got fetched.
    */
    void handleFetchResponse(Model *const model,
                             const Responses::Fetch &response,
                             QList<TreeItemPart *> &changedParts,
                             TreeItemMessage *&changedMessage,
                             bool usingQresync);
    void rescanForChildMailboxes(Model *const model);
    void handleExpunge(Model *const model, const Responses::NumberResponse &resp);
    void handleExists(Model *const model, const Responses::NumberResponse &resp);
    void handleVanished(Model *const model, const Responses::Vanished &resp);
    bool isSelectable() const;

    void saveSyncStateAndUids(Model *model);

private:
    TreeItemPart *partIdToPtr(Model *model, TreeItemMessage *message, const QByteArray &msgId);

    /** @short ImapTask which is currently responsible for well-being of this mailbox */
    QPointer<KeepMailboxOpenTask> maintainingTask;
};

class TreeItemMsgList: public TreeItem
{
    void operator=(const TreeItem &);  // don't implement
    friend class TreeItemMailbox;
    friend class TreeItemMessage; // for maintaining the m_unreadMessageCount
    friend class Model;
    friend class ObtainSynchronizedMailboxTask;
    friend class KeepMailboxOpenTask;
    FetchingState m_numberFetchingStatus;
    int m_totalMessageCount;
    int m_unreadMessageCount;
    int m_recentMessageCount;
public:
    explicit TreeItemMsgList(TreeItem *parent);

    virtual void fetch(Model *const model);
    virtual unsigned int rowCount(Model *const model);
    virtual QVariant data(Model *const model, int role);
    virtual bool hasChildren(Model *const model);

    int totalMessageCount(Model *const model);
    int unreadMessageCount(Model *const model);
    int recentMessageCount(Model *const model);
    void fetchNumbers(Model *const model);
    void recalcVariousMessageCounts(Model *model);
    void recalcVariousMessageCountsOnExpunge(Model *model, TreeItemMessage *expungedMessage);
    void resetWasUnreadState();
    bool numbersFetched() const;
};

class MessageDataPayload
{
public:
    MessageDataPayload();

    const Message::Envelope &envelope() const;
    void setEnvelope(const Message::Envelope &envelope);
    const QDateTime &internalDate() const;
    void setInternalDate(const QDateTime &internalDate);
    quint64 size() const;
    void setSize(const quint64 size);
    const QList<QByteArray> &hdrReferences() const;
    void setHdrReferences(const QList<QByteArray> &hdrReferences);
    const QList<QUrl> &hdrListPost() const;
    void setHdrListPost(const QList<QUrl> &hdrListPost);
    bool hdrListPostNo() const;
    void setHdrListPostNo(const bool hdrListPostNo);
    const QByteArray &rememberedBodyStructure() const;
    void setRememberedBodyStructure(const QByteArray &blob);

    TreeItemPart *partHeader() const;
    void setPartHeader(std::unique_ptr<TreeItemPart> part);
    TreeItemPart *partText() const;
    void setPartText(std::unique_ptr<TreeItemPart> part);

    bool isComplete() const;

    bool gotEnvelope() const;
    bool gotInternalDate() const;
    bool gotSize() const;
    bool gotHdrReferences() const;
    bool gotHdrListPost() const;
    bool gotRemeberedBodyStructure() const;

private:
    Message::Envelope m_envelope;
    QDateTime m_internalDate;
    quint64 m_size;
    QList<QByteArray> m_hdrReferences;
    QList<QUrl> m_hdrListPost;
    QByteArray m_rememberedBodyStructure;
    bool m_hdrListPostNo;
    std::unique_ptr<TreeItemPart> m_partHeader;
    std::unique_ptr<TreeItemPart> m_partText;

    bool m_gotEnvelope : 1;
    bool m_gotInternalDate : 1;
    bool m_gotSize : 1;
    bool m_gotBodystructure : 1;
    bool m_gotHdrReferences : 1;
    bool m_gotHdrListPost : 1;
};

class TreeItemMessage: public TreeItem
{
    void operator=(const TreeItem &);  // don't implement
    friend class TreeItemMailbox;
    friend class TreeItemMsgList;
    friend class Model;
    friend class ObtainSynchronizedMailboxTask; // needs access to m_offset
    friend class KeepMailboxOpenTask; // needs access to m_offset
    friend class UpdateFlagsTask; // needs access to m_flags
    friend class UpdateFlagsOfAllMessagesTask; // needs access to m_flags
    int m_offset;
    uint m_uid;
    mutable MessageDataPayload *m_data;
    QStringList m_flags;
    bool m_flagsHandled;
    bool m_wasUnread;
    /** @short Set FLAGS and maintain the unread message counter */
    void setFlags(TreeItemMsgList *list, const QStringList &flags);
    void processAdditionalHeaders(Model *model, const QByteArray &rawHeaders);
    static bool hasNestedAttachments(Model *const model, TreeItemPart *part);

    MessageDataPayload *data() const
    {
        return m_data ? m_data : (m_data = new MessageDataPayload());
    }

public:
    explicit TreeItemMessage(TreeItem *parent);
    ~TreeItemMessage();

    virtual int row() const;
    virtual void fetch(Model *const model);
    virtual unsigned int rowCount(Model *const model);
    virtual unsigned int columnCount();
    virtual QVariant data(Model *const model, int role);
    virtual bool hasChildren(Model *const model) { Q_UNUSED(model); return true; }
    virtual TreeItemChildrenList setChildren(const TreeItemChildrenList &items);
    Message::Envelope envelope(Model *const model);
    QDateTime internalDate(Model *const model);
    quint64 size(Model *const model);
    bool isMarkedAsDeleted() const;
    bool isMarkedAsRead() const;
    bool isMarkedAsReplied() const;
    bool isMarkedAsForwarded() const;
    bool isMarkedAsRecent() const;
    bool isMarkedAsFlagged() const;
    bool isMarkedAsJunk() const;
    bool isMarkedAsNotJunk() const;
    void checkFlagsReadRecent(bool &isRead, bool &isRecent) const;
    uint uid() const;
    virtual TreeItem *specialColumnPtr(int row, int column) const;
    bool hasAttachments(Model *const model);

    static QVariantList addresListToQVariant(const QList<Imap::Message::MailAddress> &addressList);
};

class TreeItemPart: public TreeItem
{
    void operator=(const TreeItem &);  // don't implement
    friend class TreeItemMailbox; // needs access to m_data
    friend class Model; // dtto
    QByteArray m_mimeType;
    QByteArray m_charset;
    QByteArray m_contentFormat;
    QByteArray m_delSp;
    QByteArray m_encoding;
    QByteArray m_data;
    QByteArray m_bodyFldId;
    QByteArray m_bodyDisposition;
    QString m_fileName;
    quint64 m_octets;
    QByteArray m_multipartRelatedStartPart;
    Imap::Message::AbstractMessage::bodyFldParam_t m_bodyFldParam;
    mutable TreeItemPart *m_partMime;
    mutable TreeItemPart *m_partRaw;
public:
    TreeItemPart(TreeItem *parent, const QByteArray &mimeType);
    ~TreeItemPart();

    virtual unsigned int childrenCount(Model *const model);
    virtual TreeItem *child(const int offset, Model *const model);
    virtual TreeItemChildrenList setChildren(const TreeItemChildrenList &items);

    virtual void fetchFromCache(Model *const model);
    virtual void fetch(Model *const model);
    virtual unsigned int rowCount(Model *const model);
    virtual unsigned int columnCount();
    virtual QVariant data(Model *const model, int role);
    virtual bool hasChildren(Model *const model);

    virtual QByteArray partId() const;

    /** @short Shall we use RFC3516 BINARY for fetching message parts or not */
    typedef enum {
        /** @short Use the baseline IMAP feature, the BODY[...], from RFC 3501 */
        FETCH_PART_IMAP,
        /** @short Fetch via the RFC3516's BINARY extension */
        FETCH_PART_BINARY
    } PartFetchingMode;

    virtual QByteArray partIdForFetch(const PartFetchingMode fetchingMode) const;
    virtual QByteArray pathToPart() const;
    TreeItemMessage *message() const;

    /** @short Provide access to the internal buffer holding data

        It is safe to access the obtained pointer as long as this object is not
        deleted. This function violates the classic concept of object
        encapsulation, but is really useful for the implementation of
        Imap::Network::MsgPartNetworkReply.
     */
    QByteArray *dataPtr();
    QByteArray mimeType() const { return m_mimeType; }
    QByteArray charset() const { return m_charset; }
    void setCharset(const QByteArray &ch) { m_charset = ch; }
    void setContentFormat(const QByteArray &format) { m_contentFormat = format; }
    void setContentDelSp(const QByteArray &delSp) { m_delSp = delSp; }
    void setEncoding(const QByteArray &encoding) { m_encoding = encoding; }
    QByteArray encoding() const { return m_encoding; }
    void setBodyFldId(const QByteArray &id) { m_bodyFldId = id; }
    QByteArray bodyFldId() const { return m_bodyFldId; }
    void setBodyDisposition(const QByteArray &disposition) { m_bodyDisposition = disposition; }
    QByteArray bodyDisposition() const { return m_bodyDisposition; }
    void setFileName(const QString &name) { m_fileName = name; }
    QString fileName() const { return m_fileName; }
    void setOctets(const quint64 size) { m_octets = size; }
    /** @short Return the downloadable size of the message part */
    quint64 octets() const { return m_octets; }
    QByteArray multipartRelatedStartPart() const { return m_multipartRelatedStartPart; }
    void setMultipartRelatedStartPart(const QByteArray &start) { m_multipartRelatedStartPart = start; }
    void setBodyFldParam(const Imap::Message::AbstractMessage::bodyFldParam_t &bodyFldParam) { m_bodyFldParam = bodyFldParam; }
    Imap::Message::AbstractMessage::bodyFldParam_t bodyFldParam() const { return m_bodyFldParam; }
    virtual TreeItem *specialColumnPtr(int row, int column) const;
    virtual bool isTopLevelMultiPart() const;

    virtual void silentlyReleaseMemoryRecursive();
protected:
    TreeItemPart(TreeItem *parent);
};

/** @short A message part with a modifier

This item hanldes fetching of message parts with an attached modifier (like TEXT, HEADER or MIME).
*/
class TreeItemModifiedPart: public TreeItemPart
{
    PartModifier m_modifier;
public:
    TreeItemModifiedPart(TreeItem *parent, const PartModifier kind);
    virtual int row() const;
    virtual unsigned int columnCount();
    virtual QByteArray partId() const;
    virtual QByteArray pathToPart() const;
    virtual TreeItem *specialColumnPtr(int row, int column) const;
    PartModifier kind() const;
    virtual QModelIndex toIndex(Model *const model) const;
    virtual QByteArray partIdForFetch(const PartFetchingMode fetchingMode) const;
protected:
    virtual bool isTopLevelMultiPart() const;
private:
    QByteArray modifierToByteArray() const;
};

/** @short Specialization of TreeItemPart for parts holding a multipart/message */
class TreeItemPartMultipartMessage: public TreeItemPart
{
    Message::Envelope m_envelope;
    mutable TreeItemPart *m_partHeader;
    mutable TreeItemPart *m_partText;
public:
    TreeItemPartMultipartMessage(TreeItem *parent, const Message::Envelope &envelope);
    virtual ~TreeItemPartMultipartMessage();
    virtual QVariant data(Model * const model, int role);
    virtual TreeItem *specialColumnPtr(int row, int column) const;
    virtual void silentlyReleaseMemoryRecursive();
};

}

}

Q_DECLARE_METATYPE(QByteArray*)

#endif // IMAP_MAILBOXTREE_H
