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
#include <QTextStream>
#include "Common/FindWithUnknown.h"
#include "Common/InvokeMethod.h"
#include "Common/MetaTypes.h"
#include "Imap/Encoders.h"
#include "Imap/Parser/Rfc5322HeaderParser.h"
#include "Imap/Tasks/KeepMailboxOpenTask.h"
#include "ItemRoles.h"
#include "MailboxTree.h"
#include "Model.h"
#include "SpecialFlagNames.h"
#include <QtDebug>

namespace
{

QVariantList addresListToQVariant(const QList<Imap::Message::MailAddress> &addressList)
{
    QVariantList res;
    foreach(const Imap::Message::MailAddress& address, addressList) {
        res.append(QVariant(QStringList() << address.name << address.adl << address.mailbox << address.host));
    }
    return res;
}

}



namespace Imap
{
namespace Mailbox
{

TreeItem::TreeItem(TreeItem *parent): m_parent(parent)
{
    // These just have to be present in the context of TreeItem, otherwise they couldn't access the protected members
#ifndef __clang__
#   if (__GNUC__ == 4 && __GNUC_MINOR__ < 6)
#       define alignof __alignof__
#   endif
#endif
    static_assert(static_cast<intptr_t>(alignof(TreeItem)) > TreeItem::TagMask,
                  "class TreeItem must be aligned at at least four bytes due to the FetchingState optimization");
    static_assert(DONE <= TagMask, "Invalid masking for pointer tag access");
}

TreeItem::~TreeItem()
{
    qDeleteAll(m_children);
}

unsigned int TreeItem::childrenCount(Model *const model)
{
    fetch(model);
    return m_children.size();
}

TreeItem *TreeItem::child(int offset, Model *const model)
{
    fetch(model);
    if (offset >= 0 && offset < m_children.size())
        return m_children[ offset ];
    else
        return 0;
}

int TreeItem::row() const
{
    return parent() ? parent()->m_children.indexOf(const_cast<TreeItem *>(this)) : 0;
}

TreeItemChildrenList TreeItem::setChildren(const TreeItemChildrenList &items)
{
    auto res = m_children;
    m_children = items;
    setFetchStatus(DONE);
    return res;
}

bool TreeItem::isUnavailable(Model *const model) const
{
    return accessFetchStatus() == UNAVAILABLE && model->networkPolicy() == NETWORK_OFFLINE;
}

unsigned int TreeItem::columnCount()
{
    return 1;
}

TreeItem *TreeItem::specialColumnPtr(int row, int column) const
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    return 0;
}

QModelIndex TreeItem::toIndex(Model *const model) const
{
    Q_ASSERT(model);
    // void* != const void*, but I believe that it's safe in this context
    return model->createIndex(row(), 0, const_cast<TreeItem *>(this));
}


TreeItemMailbox::TreeItemMailbox(TreeItem *parent): TreeItem(parent), maintainingTask(0)
{
    m_children.prepend(new TreeItemMsgList(this));
}

TreeItemMailbox::TreeItemMailbox(TreeItem *parent, Responses::List response):
    TreeItem(parent), m_metadata(response.mailbox, response.separator, QStringList()), maintainingTask(0)
{
    for (QStringList::const_iterator it = response.flags.constBegin(); it != response.flags.constEnd(); ++it)
        m_metadata.flags.append(it->toUpper());
    m_children.prepend(new TreeItemMsgList(this));
}

TreeItemMailbox::~TreeItemMailbox()
{
    if (maintainingTask) {
        maintainingTask->dieIfInvalidMailbox();
    }
}

TreeItemMailbox *TreeItemMailbox::fromMetadata(TreeItem *parent, const MailboxMetadata &metadata)
{
    TreeItemMailbox *res = new TreeItemMailbox(parent);
    res->m_metadata = metadata;
    return res;
}

void TreeItemMailbox::fetch(Model *const model)
{
    fetchWithCacheControl(model, false);
}

void TreeItemMailbox::fetchWithCacheControl(Model *const model, bool forceReload)
{
    if (fetched() || isUnavailable(model))
        return;

    if (hasNoChildMailboxesAlreadyKnown()) {
        setFetchStatus(DONE);
        return;
    }

    if (! loading()) {
        setFetchStatus(LOADING);
        QModelIndex mailboxIndex = toIndex(model);
        if (mailboxIndex.isValid()) {
            CALL_LATER(model, askForChildrenOfMailbox, Q_ARG(QModelIndex, mailboxIndex),
                       Q_ARG(Imap::Mailbox::CacheLoadingMode, forceReload ? LOAD_FORCE_RELOAD : LOAD_CACHED_IS_OK));
        } else {
            CALL_LATER(model, askForTopLevelChildren, Q_ARG(Imap::Mailbox::CacheLoadingMode, forceReload ? LOAD_FORCE_RELOAD : LOAD_CACHED_IS_OK));
        }
    }
}

void TreeItemMailbox::rescanForChildMailboxes(Model *const model)
{
    // FIXME: fix duplicate requests (ie. don't allow more when some are on their way)
    // FIXME: gotta be fixed in the Model, or spontaneous replies from server can break us
    setFetchStatus(NONE);
    fetchWithCacheControl(model, true);
}

unsigned int TreeItemMailbox::rowCount(Model *const model)
{
    fetch(model);
    return m_children.size();
}

QVariant TreeItemMailbox::data(Model *const model, int role)
{
    if (!parent())
        return QVariant();

    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(m_children[0]);
    Q_ASSERT(list);

    switch (role) {
    case Qt::DisplayRole:
    {
        // this one is used only for a dumb view attached to the Model
        QString res = separator().isEmpty() ? mailbox() : mailbox().split(separator(), QString::SkipEmptyParts).last();
        return loading() ? res + " [loading]" : res;
    }
    case RoleIsFetched:
        return fetched();
    case RoleIsUnavailable:
        return isUnavailable(model);
    case RoleShortMailboxName:
        return separator().isEmpty() ? mailbox() : mailbox().split(separator(), QString::SkipEmptyParts).last();
    case RoleMailboxName:
        return mailbox();
    case RoleMailboxSeparator:
        return separator();
    case RoleMailboxHasChildMailboxes:
        return hasChildMailboxes(model);
    case RoleMailboxIsINBOX:
        return mailbox().toUpper() == QLatin1String("INBOX");
    case RoleMailboxIsSelectable:
        return isSelectable();
    case RoleMailboxNumbersFetched:
        return list->numbersFetched();
    case RoleTotalMessageCount:
    {
        if (! isSelectable())
            return QVariant();
        // At first, register that request for count
        int res = list->totalMessageCount(model);
        // ...and now that it's been sent, display a number if it's available
        return list->numbersFetched() ? QVariant(res) : QVariant();
    }
    case RoleUnreadMessageCount:
    {
        if (! isSelectable())
            return QVariant();
        // This one is similar to the case of RoleTotalMessageCount
        int res = list->unreadMessageCount(model);
        return list->numbersFetched() ? QVariant(res): QVariant();
    }
    case RoleRecentMessageCount:
    {
        if (! isSelectable())
            return QVariant();
        // see above
        int res = list->recentMessageCount(model);
        return list->numbersFetched() ? QVariant(res): QVariant();
    }
    case RoleMailboxItemsAreLoading:
        return list->loading() || (isSelectable() && ! list->numbersFetched());
    case RoleMailboxUidValidity:
    {
        list->fetch(model);
        return list->fetched() ? QVariant(syncState.uidValidity()) : QVariant();
    }
    case RoleMailboxIsSubscribed:
        return QVariant::fromValue<bool>(m_metadata.flags.contains(QLatin1String("\\SUBSCRIBED")));
    default:
        return QVariant();
    }
}

bool TreeItemMailbox::hasChildren(Model *const model)
{
    Q_UNUSED(model);
    return true; // we have that "messages" thing built in
}

QLatin1String TreeItemMailbox::flagNoInferiors("\\NOINFERIORS");
QLatin1String TreeItemMailbox::flagHasNoChildren("\\HASNOCHILDREN");
QLatin1String TreeItemMailbox::flagHasChildren("\\HASCHILDREN");

bool TreeItemMailbox::hasNoChildMailboxesAlreadyKnown()
{
    if (m_metadata.flags.contains(flagNoInferiors) ||
        (m_metadata.flags.contains(flagHasNoChildren) &&
         ! m_metadata.flags.contains(flagHasChildren)))
        return true;
    else
        return false;
}

bool TreeItemMailbox::hasChildMailboxes(Model *const model)
{
    if (fetched() || isUnavailable(model)) {
        return m_children.size() > 1;
    } else if (hasNoChildMailboxesAlreadyKnown()) {
        return false;
    } else if (m_metadata.flags.contains(flagHasChildren) && ! m_metadata.flags.contains(flagHasNoChildren)) {
        return true;
    } else {
        fetch(model);
        return m_children.size() > 1;
    }
}

TreeItem *TreeItemMailbox::child(const int offset, Model *const model)
{
    // accessing TreeItemMsgList doesn't need fetch()
    if (offset == 0)
        return m_children[ 0 ];

    return TreeItem::child(offset, model);
}

TreeItemChildrenList TreeItemMailbox::setChildren(const TreeItemChildrenList &items)
{
    // This function has to be special because we want to preserve m_children[0]

    TreeItemMsgList *msgList = dynamic_cast<TreeItemMsgList *>(m_children[0]);
    Q_ASSERT(msgList);
    m_children.erase(m_children.begin());

    auto list = TreeItem::setChildren(items);  // this also adjusts m_loading and m_fetched

    m_children.prepend(msgList);

    // FIXME: anything else required for \Noselect?
    if (! isSelectable())
        msgList->setFetchStatus(DONE);

    return list;
}

void TreeItemMailbox::handleFetchResponse(Model *const model,
        const Responses::Fetch &response,
        QList<TreeItemPart *> &changedParts,
        TreeItemMessage *&changedMessage, bool usingQresync)
{
    TreeItemMsgList *list = static_cast<TreeItemMsgList *>(m_children[0]);

    Responses::Fetch::dataType::const_iterator uidRecord = response.data.find("UID");

    // Previously, we would ignore any FETCH responses until we are fully synced. This is rather hard do to "properly",
    // though.
    // What we want to achieve is to never store data into a "wrong" message. Theoretically, we are prone to just this
    // in case the server sends us unsolicited data before we are fully synced. When this happens for flags, it's a pretty
    // harmless operation as we're going to re-fetch the flags for the concerned part of mailbox anyway (even with CONDSTORE,
    // and this is never an issue with QRESYNC).
    // It's worse when the data refer to some immutable piece of information like the bodystructure or body parts.
    // If that happens, then we have to actively prevent the data from being stored because we cannot know whether we would
    // be putting it into a correct bucket^Hmessage.
    bool ignoreImmutableData = !list->fetched() && uidRecord == response.data.constEnd();

    int number = response.number - 1;
    if (number < 0 || number >= list->m_children.size())
        throw UnknownMessageIndex(QString::fromUtf8("Got FETCH that is out of bounds -- got %1 messages").arg(
                                      QString::number(list->m_children.size())).toUtf8().constData(), response);

    TreeItemMessage *message = static_cast<TreeItemMessage *>(list->child(number, model));

    // At first, have a look at the response and check the UID of the message
    if (uidRecord != response.data.constEnd()) {
        uint receivedUid = static_cast<const Responses::RespData<uint>&>(*(uidRecord.value())).data;
        if (message->uid() == receivedUid) {
            // That's what we expect -> do nothing
        } else if (message->uid() == 0) {
            // This is the first time we see the UID, so let's take a note
            message->m_uid = receivedUid;
            changedMessage = message;
            if (message->loading()) {
                // The Model tried to ask for data for this message. That couldn't succeeded because the UID
                // wasn't known at that point, so let's ask now
                //
                // FIXME: tweak this to keep a high watermark of "highest UID we requested an ENVELOPE for",
                // issue bulk fetches in the same manner as we do the UID FETCH (FLAGS) when discovering UIDs,
                // and at this place in code, only ask for the metadata when the UID is higher than the watermark.
                // Optionally, simply ask for the ENVELOPE etc along with the FLAGS upon new message arrivals, maybe
                // with some limit on the number of pending fetches. And make that dapandent on online/expensive modes.
                message->setFetchStatus(NONE);
                message->fetch(model);
            }
            if (syncState.uidNext() <= receivedUid) {
                // Try to guess the UIDNEXT. We have to take an educated guess here, and I believe that this approach
                // at least is not wrong. The server won't tell us the UIDNEXT (well, it could, but it doesn't have to),
                // the only way of asking for it is via STATUS which is not allowed to reference the current mailbox and
                // even if it was, it wouldn't be atomic. So, what could the UIDNEXT possibly be? It can't be smaller
                // than the UID_of_highest_message, and it can't be the same, either, so it really has to be higher.
                // Let's just increment it by one, this is our lower bound.
                // Not guessing the UIDNEXT correctly would result at decreased performance at the next sync, and we
                // can't really do better -> let's just set it now, along with the UID mapping.
                syncState.setUidNext(receivedUid + 1);
                list->setFetchStatus(LOADING);
            }
        } else {
            throw MailboxException(QString::fromUtf8("FETCH response: UID consistency error for message #%1 -- expected UID %2, got UID %3").arg(
                                       QString::number(response.number), QString::number(message->uid()), QString::number(receivedUid)
                                       ).toUtf8().constData(), response);
        }
    } else if (! message->uid()) {
        qDebug() << "FETCH: received a FETCH response for message #" << response.number << "whose UID is not yet known. This sucks.";
        QList<uint> uidsInMailbox;
        Q_FOREACH(TreeItem *node, list->m_children) {
            uidsInMailbox << static_cast<TreeItemMessage *>(node)->uid();
        }
        qDebug() << "UIDs in the mailbox now: " << uidsInMailbox;
    }

    bool savedBodyStructure = false;
    bool gotEnvelope = false;
    bool gotSize = false;
    bool gotInternalDate = false;
    bool updatedFlags = false;

    for (Responses::Fetch::dataType::const_iterator it = response.data.begin(); it != response.data.end(); ++ it) {
        if (it.key() == "UID") {
            // established above
            Q_ASSERT(static_cast<const Responses::RespData<uint>&>(*(it.value())).data == message->uid());
        } else if (it.key() == "FLAGS") {
            // Only emit signals when the flags have actually changed
            QStringList newFlags = model->normalizeFlags(static_cast<const Responses::RespData<QStringList>&>(*(it.value())).data);
            bool forceChange = (message->m_flags != newFlags);
            message->setFlags(list, newFlags, forceChange);
            if (forceChange) {
                updatedFlags = true;
                changedMessage = message;
            }
        } else if (it.key() == "MODSEQ") {
            quint64 num = static_cast<const Responses::RespData<quint64>&>(*(it.value())).data;
            if (num > syncState.highestModSeq()) {
                syncState.setHighestModSeq(num);
                if (list->accessFetchStatus() == DONE) {
                    // This means that everything is known already, so we are by definition OK to save stuff to disk.
                    // We can also skip rebuilding the UID map and save just the HIGHESTMODSEQ, i.e. the SyncState.
                    model->cache()->setMailboxSyncState(mailbox(), syncState);
                } else {
                    // it's already marked as dirty -> nothing to do here
                }
            }
        } else if (ignoreImmutableData) {
            QByteArray buf;
            QTextStream ss(&buf);
            ss << response;
            ss.flush();
            qDebug() << "Ignoring FETCH response to a mailbox that isn't synced yet:" << buf;
            continue;
        } else if (it.key() == "ENVELOPE") {
            message->data()->m_envelope = static_cast<const Responses::RespData<Message::Envelope>&>(*(it.value())).data;
            message->setFetchStatus(DONE);
            gotEnvelope = true;
            changedMessage = message;
        } else if (it.key() == "BODYSTRUCTURE") {
            if (message->fetched()) {
                // The message structure is already known, so we are free to ignore it
            } else {
                // We had no idea about the structure of the message
                auto newChildren = static_cast<const Message::AbstractMessage &>(*(it.value())).createTreeItems(message);
                if (!message->m_children.isEmpty()) {
                    QModelIndex messageIdx = message->toIndex(model);
                    model->beginRemoveRows(messageIdx, 0, message->m_children.size() - 1);
                    auto oldChildren = message->setChildren(newChildren);
                    model->endRemoveRows();
                    qDeleteAll(oldChildren);
                } else {
                    auto oldChildren = message->setChildren(newChildren);
                    Q_ASSERT(oldChildren.size() == 0);
                }
                savedBodyStructure = true;
            }
        } else if (it.key() == "x-trojita-bodystructure") {
            // do nothing
        } else if (it.key() == "RFC822.SIZE") {
            message->data()->m_size = static_cast<const Responses::RespData<uint>&>(*(it.value())).data;
            gotSize = true;
        } else if (it.key().startsWith("BODY[HEADER.FIELDS (")) {
            // Process any headers found in any such response bit
            const QByteArray &rawHeaders = static_cast<const Responses::RespData<QByteArray>&>(*(it.value())).data;
            message->processAdditionalHeaders(model, rawHeaders);
            changedMessage = message;
        } else if (it.key().startsWith("BODY[") || it.key().startsWith("BINARY[")) {
            if (it.key()[ it.key().size() - 1 ] != ']')
                throw UnknownMessageIndex("Can't parse such BODY[]/BINARY[]", response);
            TreeItemPart *part = partIdToPtr(model, message, it.key());
            if (! part)
                throw UnknownMessageIndex("Got BODY[]/BINARY[] fetch that did not resolve to any known part", response);
            const QByteArray &data = static_cast<const Responses::RespData<QByteArray>&>(*(it.value())).data;
            if (it.key().startsWith("BODY[")) {

                // Check whether we are supposed to be loading the raw, undecoded part as well.
                // The check has to be done via a direct pointer access to m_partRaw to make sure that it does not
                // get instantiated when not actually needed.
                if (part->m_partRaw && part->m_partRaw->loading()) {
                    part->m_partRaw->m_data = data;
                    part->m_partRaw->setFetchStatus(DONE);
                    changedParts.append(part->m_partRaw);
                    if (message->uid()) {
                        model->cache()->forgetMessagePart(mailbox(), message->uid(), part->partId());
                        model->cache()->setMsgPart(mailbox(), message->uid(), part->partId() + QLatin1String(".X-RAW"), data);
                    }
                }

                // Do not overwrite the part data if we were not asked to fetch it.
                // One possibility is that it's already there because it was fetched before. The second option is that
                // we were in fact asked to only fetch the raw data and the user is not itnerested in the processed data at all.
                if (part->loading()) {
                    // got to decode the part data by hand
                    Imap::decodeContentTransferEncoding(data, part->encoding(), part->dataPtr());
                    part->setFetchStatus(DONE);
                    changedParts.append(part);
                    if (message->uid()
                            && model->cache()->messagePart(mailbox(), message->uid(), part->partId() + QLatin1String(".X-RAW")).isNull()) {
                        // Do not store the data into cache if the raw data are already there
                        model->cache()->setMsgPart(mailbox(), message->uid(), part->partId(), part->m_data);
                    }
                }

            } else {
                // A BINARY FETCH item is already decoded for us, yay
                part->m_data = data;
                part->setFetchStatus(DONE);
                changedParts.append(part);
                if (message->uid()) {
                    model->cache()->setMsgPart(mailbox(), message->uid(), part->partId(), part->m_data);
                }
            }
        } else if (it.key() == "INTERNALDATE") {
            message->data()->m_internalDate = static_cast<const Responses::RespData<QDateTime>&>(*(it.value())).data;
            gotInternalDate = true;
        } else {
            qDebug() << "TreeItemMailbox::handleFetchResponse: unknown FETCH identifier" << it.key();
        }
    }
    if (message->uid()) {
        if (gotEnvelope && gotSize && savedBodyStructure && gotInternalDate) {
            Imap::Mailbox::AbstractCache::MessageDataBundle dataForCache;
            dataForCache.envelope = message->data()->m_envelope;
            dataForCache.serializedBodyStructure = static_cast<const Responses::RespData<QByteArray>&>(*(response.data[ "x-trojita-bodystructure" ])).data;
            dataForCache.size = message->data()->m_size;
            dataForCache.uid = message->uid();
            dataForCache.internalDate = message->data()->m_internalDate;
            dataForCache.hdrReferences = message->data()->m_hdrReferences;
            dataForCache.hdrListPost = message->data()->m_hdrListPost;
            dataForCache.hdrListPostNo = message->data()->m_hdrListPostNo;
            model->cache()->setMessageMetadata(mailbox(), message->uid(), dataForCache);
        }
        if (updatedFlags) {
            model->cache()->setMsgFlags(mailbox(), message->uid(), message->m_flags);
        }
    }
}

/** @short Save the sync state and the UID mapping into the cache

Please note that FLAGS are still being updated "asynchronously", i.e. immediately when an update arrives. The motivation
behind this is that both SyncState and UID mapping just absolutely have to be kept in sync due to the way they are used where
our syncing code simply expects both to match each other. There cannot ever be any 0 UIDs in the saved UID mapping, and the
number in EXISTS and the amount of cached UIDs is not supposed to differ or all bets are off.

The flags, on the other hand, are not critical -- if a message gets saved with the correct flags "too early", i.e. before
the corresponding SyncState and/or UIDs are saved, the wors case which could possibly happen are data which do not match the
old state any longer. But the old state is not important anyway because it's already gone on the server.
*/
void TreeItemMailbox::saveSyncStateAndUids(Model * model)
{
    model->cache()->setMailboxSyncState(mailbox(), syncState);
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList*>(m_children[0]);
    model->saveUidMap(list);
    list->setFetchStatus(DONE);
}

/** @short Process the EXPUNGE response when the UIDs are already synced */
void TreeItemMailbox::handleExpunge(Model *const model, const Responses::NumberResponse &resp)
{
    Q_ASSERT(resp.kind == Responses::EXPUNGE);
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(m_children[ 0 ]);
    Q_ASSERT(list);
    if (resp.number > static_cast<uint>(list->m_children.size()) || resp.number == 0) {
        throw UnknownMessageIndex("EXPUNGE references message number which is out-of-bounds");
    }
    uint offset = resp.number - 1;

    model->beginRemoveRows(list->toIndex(model), offset, offset);
    auto it = list->m_children.begin() + offset;
    TreeItemMessage *message = static_cast<TreeItemMessage *>(*it);
    list->m_children.erase(it);
    model->cache()->clearMessage(static_cast<TreeItemMailbox *>(list->parent())->mailbox(), message->uid());
    for (int i = offset; i < list->m_children.size(); ++i) {
        --static_cast<TreeItemMessage *>(list->m_children[i])->m_offset;
    }
    model->endRemoveRows();
    delete message;

    --list->m_totalMessageCount;
    list->recalcVariousMessageCounts(const_cast<Model *>(model));

    if (list->accessFetchStatus() == DONE) {
        // Previously, we were synced, so we got to save this update
        saveSyncStateAndUids(model);
    }
}

void TreeItemMailbox::handleVanished(Model *const model, const Responses::Vanished &resp)
{
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(m_children[ 0 ]);
    Q_ASSERT(list);
    QModelIndex listIndex = list->toIndex(model);

    QList<uint> uids = resp.uids;
    qSort(uids);
    // Remove duplicates -- even that garbage can be present in a perfectly valid VANISHED :(
    uids.erase(std::unique(uids.begin(), uids.end()), uids.end());

    auto it = list->m_children.end();
    while (!uids.isEmpty()) {
        // We have to process each UID separately because the UIDs in the mailbox are not necessarily present
        // in a continuous range; zeros might be present
        uint uid = uids.takeLast();

        if (uid == 0) {
            qDebug() << "VANISHED informs about removal of UID zero...";
            model->logTrace(listIndex.parent(), Common::LOG_MAILBOX_SYNC, QLatin1String("TreeItemMailbox::handleVanished"),
                            "VANISHED contains UID zero for increased fun");
            break;
        }

        if (list->m_children.isEmpty()) {
            // Well, it'd be cool to throw an exception here but VANISHED is free to contain references to UIDs which are not here
            // at all...
            qDebug() << "VANISHED attempted to remove too many messages";
            model->logTrace(listIndex.parent(), Common::LOG_MAILBOX_SYNC, QLatin1String("TreeItemMailbox::handleVanished"),
                            "VANISHED attempted to remove too many messages");
            break;
        }

        // Find a highest message with UID zero such as no message with non-zero UID higher than the current UID exists
        // at a position after the target message
        it = model->findMessageOrNextOneByUid(list, uid);

        if (it == list->m_children.end()) {
            // this is a legitimate situation, the UID of the last message in the mailbox which is getting expunged right now
            // could very well be not know at this point
            --it;
        }
        // there's a special case above guarding against an empty list
        Q_ASSERT(it >= list->m_children.begin());

        TreeItemMessage *msgCandidate = static_cast<TreeItemMessage*>(*it);
        if (msgCandidate->uid() == uid) {
            // will be deleted
        } else if (resp.earlier == Responses::Vanished::EARLIER) {
            // We don't have any such UID in our UID mapping, so we can safely ignore this one
            continue;
        } else if (msgCandidate->uid() == 0) {
            // will be deleted
        } else {
            if (it != list->m_children.begin()) {
                --it;
                msgCandidate = static_cast<TreeItemMessage*>(*it);
                if (msgCandidate->uid() == 0) {
                    // will be deleted
                } else {
                    // VANISHED is free to refer to a non-existing UID...
                    QString str;
                    QTextStream ss(&str);
                    ss << "VANISHED refers to UID " << uid << " which wasn't found in the mailbox (found adjacent UIDs " <<
                          msgCandidate->uid() << " and " << static_cast<TreeItemMessage*>(*(it + 1))->uid() << " with " <<
                          static_cast<TreeItemMessage*>(*(list->m_children.end() - 1))->uid() << " at the end)";
                    ss.flush();
                    qDebug() << str.toUtf8().constData();
                    model->logTrace(listIndex.parent(), Common::LOG_MAILBOX_SYNC, QLatin1String("TreeItemMailbox::handleVanished"), str);
                    continue;
                }
            } else {
                // Again, VANISHED can refer to non-existing UIDs
                QString str;
                QTextStream ss(&str);
                ss << "VANISHED refers to UID " << uid << " which is too low (lowest UID is " <<
                      static_cast<TreeItemMessage*>(list->m_children.front())->uid() << ")";
                ss.flush();
                qDebug() << str.toUtf8().constData();
                model->logTrace(listIndex.parent(), Common::LOG_MAILBOX_SYNC, QLatin1String("TreeItemMailbox::handleVanished"), str);
                continue;
            }
        }

        int row = msgCandidate->row();
        Q_ASSERT(row == it - list->m_children.begin());
        model->beginRemoveRows(listIndex, row, row);
        it = list->m_children.erase(it);
        for (auto furtherMessage = it; furtherMessage != list->m_children.end(); ++furtherMessage) {
            --static_cast<TreeItemMessage *>(*furtherMessage)->m_offset;
        }
        model->endRemoveRows();

        if (syncState.uidNext() <= uid) {
            // We're informed about a message being deleted; this means that that UID must have been in the mailbox for some
            // (possibly tiny) time and we can therefore use it to get an idea about the UIDNEXT
            syncState.setUidNext(uid + 1);
        }
        model->cache()->clearMessage(mailbox(), uid);
        delete msgCandidate;
    }

    if (resp.earlier == Responses::Vanished::EARLIER && static_cast<uint>(list->m_children.size()) < syncState.exists()) {
        // Okay, there were some new arrivals which we failed to take into account because we had processed EXISTS
        // before VANISHED (EARLIER). That means that we have to add some of that messages back right now.
        int newArrivals = syncState.exists() - list->m_children.size();
        Q_ASSERT(newArrivals > 0);
        QModelIndex parent = list->toIndex(model);
        int offset = list->m_children.size();
        model->beginInsertRows(parent, offset, syncState.exists() - 1);
        for (int i = 0; i < newArrivals; ++i) {
            TreeItemMessage *msg = new TreeItemMessage(list);
            msg->m_offset = i + offset;
            list->m_children << msg;
            // yes, we really have to add this message with UID 0 :(
        }
        model->endInsertRows();
    }

    list->m_totalMessageCount = list->m_children.size();
    syncState.setExists(list->m_totalMessageCount);
    list->recalcVariousMessageCounts(const_cast<Model *>(model));

    if (list->accessFetchStatus() == DONE) {
        // Previously, we were synced, so we got to save this update
        saveSyncStateAndUids(model);
    }
}

/** @short Process the EXISTS response

This function assumes that the mailbox is already synced.
*/
void TreeItemMailbox::handleExists(Model *const model, const Responses::NumberResponse &resp)
{
    Q_ASSERT(resp.kind == Responses::EXISTS);
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(m_children[0]);
    Q_ASSERT(list);
    // This is a bit tricky -- unfortunately, we can't assume anything about the UID of new arrivals. On the other hand,
    // these messages can be referenced by (even unrequested) FETCH responses and deleted by EXPUNGE, so we really want
    // to add them to the tree.
    int newArrivals = resp.number - list->m_children.size();
    if (newArrivals < 0) {
        throw UnexpectedResponseReceived("EXISTS response attempted to decrease number of messages", resp);
    }
    syncState.setExists(resp.number);
    if (newArrivals == 0) {
        // remains unchanged...
        return;
    }

    QModelIndex parent = list->toIndex(model);
    int offset = list->m_children.size();
    model->beginInsertRows(parent, offset, resp.number - 1);
    for (int i = 0; i < newArrivals; ++i) {
        TreeItemMessage *msg = new TreeItemMessage(list);
        msg->m_offset = i + offset;
        list->m_children << msg;
        // yes, we really have to add this message with UID 0 :(
    }
    model->endInsertRows();
    list->m_totalMessageCount = resp.number;
    list->setFetchStatus(LOADING);
    model->emitMessageCountChanged(this);
}

TreeItemPart *TreeItemMailbox::partIdToPtr(Model *const model, TreeItemMessage *message, const QString &msgId)
{
    QString partIdentification;
    if (msgId.startsWith(QLatin1String("BODY["))) {
        partIdentification = msgId.mid(5, msgId.size() - 6);
    } else if (msgId.startsWith(QLatin1String("BODY.PEEK["))) {
        partIdentification = msgId.mid(10, msgId.size() - 11);
    } else if (msgId.startsWith(QLatin1String("BINARY.PEEK["))) {
        partIdentification = msgId.mid(12, msgId.size() - 13);
    } else if (msgId.startsWith(QLatin1String("BINARY["))) {
        partIdentification = msgId.mid(7, msgId.size() - 8);
    } else {
        throw UnknownMessageIndex(QString::fromUtf8("Fetch identifier doesn't start with reasonable prefix: %1").arg(msgId).toUtf8().constData());
    }

    TreeItem *item = message;
    Q_ASSERT(item);
    QStringList separated = partIdentification.split('.');
    for (QStringList::const_iterator it = separated.constBegin(); it != separated.constEnd(); ++it) {
        bool ok;
        uint number = it->toUInt(&ok);
        if (!ok) {
            // It isn't a number, so let's check for that special modifiers
            if (it + 1 != separated.constEnd()) {
                // If it isn't at the very end, it's an error
                throw UnknownMessageIndex(
                    QString::fromUtf8("Part offset contains non-numeric identifiers in the middle: %1")
                    .arg(msgId).toUtf8().constData());
            }
            // Recognize the valid modifiers
            if (*it == QLatin1String("HEADER"))
                item = item->specialColumnPtr(0, OFFSET_HEADER);
            else if (*it == QLatin1String("TEXT"))
                item = item->specialColumnPtr(0, OFFSET_TEXT);
            else if (*it == QLatin1String("MIME"))
                item = item->specialColumnPtr(0, OFFSET_MIME);
            else
                throw UnknownMessageIndex(QString::fromUtf8(
                                              "Can't translate received offset of the message part to a number: %1")
                                          .arg(msgId).toUtf8().constData());
            break;
        }

        // Normal path: descending down and finding the correct part
        TreeItemPart *part = dynamic_cast<TreeItemPart *>(item->child(0, model));
        if (part && part->isTopLevelMultiPart())
            item = part;
        item = item->child(number - 1, model);
        if (! item) {
            throw UnknownMessageIndex(QString::fromUtf8(
                                          "Offset of the message part not found: message %1 (UID %2), current number %3, full identification %4")
                                      .arg(QString::number(message->row()), QString::number(message->uid()),
                                           QString::number(number), msgId).toUtf8().constData());
        }
    }
    TreeItemPart *part = dynamic_cast<TreeItemPart *>(item);
    return part;
}

bool TreeItemMailbox::isSelectable() const
{
    return !m_metadata.flags.contains(QLatin1String("\\NOSELECT")) && !m_metadata.flags.contains(QLatin1String("\\NONEXISTENT"));
}



TreeItemMsgList::TreeItemMsgList(TreeItem *parent):
    TreeItem(parent), m_numberFetchingStatus(NONE), m_totalMessageCount(-1),
    m_unreadMessageCount(-1), m_recentMessageCount(-1)
{
    if (!parent->parent())
        setFetchStatus(DONE);
}

void TreeItemMsgList::fetch(Model *const model)
{
    if (fetched() || isUnavailable(model))
        return;

    if (!loading()) {
        setFetchStatus(LOADING);
        // We can't ask right now, has to wait till the end of the event loop
        CALL_LATER(model, askForMessagesInMailbox, Q_ARG(QModelIndex, toIndex(model)));
    }
}

void TreeItemMsgList::fetchNumbers(Model *const model)
{
    if (m_numberFetchingStatus == NONE) {
        m_numberFetchingStatus = LOADING;
        model->askForNumberOfMessages(this);
    }
}

unsigned int TreeItemMsgList::rowCount(Model *const model)
{
    return childrenCount(model);
}

QVariant TreeItemMsgList::data(Model *const model, int role)
{
    if (role == RoleIsFetched)
        return fetched();
    if (role == RoleIsUnavailable)
        return isUnavailable(model);

    if (role != Qt::DisplayRole)
        return QVariant();

    if (!parent())
        return QVariant();

    if (loading())
        return "[loading messages...]";

    if (isUnavailable(model))
        return "[offline]";

    if (fetched())
        return hasChildren(model) ? QString("[%1 messages]").arg(childrenCount(model)) : "[no messages]";

    return "[messages?]";
}

bool TreeItemMsgList::hasChildren(Model *const model)
{
    Q_UNUSED(model);
    return true; // we can easily wait here
}

int TreeItemMsgList::totalMessageCount(Model *const model)
{
    // Yes, the numbers can be accommodated by a full mailbox sync, but that's not really what we shall do from this context.
    // Because we want to allow the old-school polling for message numbers, we have to look just at the numberFetched() state.
    if (!numbersFetched())
        fetchNumbers(model);
    return m_totalMessageCount;
}

int TreeItemMsgList::unreadMessageCount(Model *const model)
{
    // See totalMessageCount()
    if (!numbersFetched())
        fetchNumbers(model);
    return m_unreadMessageCount;
}

int TreeItemMsgList::recentMessageCount(Model *const model)
{
    // See totalMessageCount()
    if (!numbersFetched())
        fetchNumbers(model);
    return m_recentMessageCount;
}

void TreeItemMsgList::recalcVariousMessageCounts(Model *model)
{
    m_unreadMessageCount = 0;
    m_recentMessageCount = 0;
    for (int i = 0; i < m_children.size(); ++i) {
        TreeItemMessage *message = static_cast<TreeItemMessage *>(m_children[i]);
        if (!message->m_flagsHandled)
            message->m_wasUnread = ! message->isMarkedAsRead();
        message->m_flagsHandled = true;
        if (! message->isMarkedAsRead())
            ++m_unreadMessageCount;
        if (message->isMarkedAsRecent())
            ++m_recentMessageCount;
    }
    m_totalMessageCount = m_children.size();
    m_numberFetchingStatus = DONE;
    model->emitMessageCountChanged(static_cast<TreeItemMailbox *>(parent()));
}

void TreeItemMsgList::resetWasUnreadState()
{
    for (int i = 0; i < m_children.size(); ++i) {
        TreeItemMessage *message = static_cast<TreeItemMessage *>(m_children[i]);
        message->m_wasUnread = ! message->isMarkedAsRead();
    }
}

bool TreeItemMsgList::numbersFetched() const
{
    return m_numberFetchingStatus == DONE;
}



MessageDataPayload::MessageDataPayload():
    m_size(0), m_hdrListPostNo(false), m_partHeader(0), m_partText(0)
{
}

MessageDataPayload::~MessageDataPayload()
{
    delete m_partHeader;
    delete m_partText;
}

TreeItemMessage::TreeItemMessage(TreeItem *parent):
    TreeItem(parent), m_offset(-1), m_uid(0), m_data(0), m_flagsHandled(false), m_wasUnread(false)
{
}

TreeItemMessage::~TreeItemMessage()
{
    delete m_data;
}

void TreeItemMessage::fetch(Model *const model)
{
    if (fetched() || loading() || isUnavailable(model))
        return;

    if (m_uid) {
        // Message UID is already known, which means that we can request data for this message
        model->askForMsgMetadata(this, Model::PRELOAD_PER_POLICY);
    } else {
        // The UID is not known yet, so we can't initiate a UID FETCH at this point. However, we mark
        // this message as "loading", which has the side effect that it will get re-fetched as soon as
        // the UID arrives -- see TreeItemMailbox::handleFetchResponse(), the section which deals with
        // setting previously unknown UIDs, and the similar code in ObtainSynchronizedMailboxTask.
        //
        // Even though this breaks the message preload done in Model::_askForMsgmetadata, chances are that
        // the UIDs will arrive rather soon for all of the pending messages, and the request for metadata
        // will therefore get queued roughly at the same time.  This gives the KeepMailboxOpenTask a chance
        // to group similar requests together.  To reiterate:
        // - Messages are attempted to get *preloaded* (ie. requesting metadata even for messages that are not
        //   yet shown) as usual; this could fail because the UIDs might not be known yet.
        // - The actual FETCH could be batched by the KeepMailboxOpenTask anyway
        // - Hence, this should be still pretty fast and efficient
        setFetchStatus(LOADING);
    }
}

unsigned int TreeItemMessage::rowCount(Model *const model)
{
    fetch(model);
    return m_children.size();
}

unsigned int TreeItemMessage::columnCount()
{
    static_assert(OFFSET_HEADER < OFFSET_TEXT && OFFSET_MIME == OFFSET_TEXT + 1,
                  "We need column 0 for regular children and columns 1 and 2 for OFFSET_HEADER and OFFSET_TEXT.");
    // Oh, and std::max is not constexpr in C++11.
    return OFFSET_MIME;
}

TreeItem *TreeItemMessage::specialColumnPtr(int row, int column) const
{
    // This is a nasty one -- we have an irregular shape...

    // No extra columns on other rows
    if (row != 0)
        return 0;

    switch (column) {
    case OFFSET_TEXT:
        if (!data()->m_partText) {
            data()->m_partText = new TreeItemModifiedPart(const_cast<TreeItemMessage *>(this), OFFSET_TEXT);
        }
        return data()->m_partText;
    case OFFSET_HEADER:
        if (!data()->m_partHeader) {
            data()->m_partHeader = new TreeItemModifiedPart(const_cast<TreeItemMessage *>(this), OFFSET_HEADER);
        }
        return data()->m_partHeader;
    default:
        return 0;
    }
}

int TreeItemMessage::row() const
{
    Q_ASSERT(m_offset != -1);
    return m_offset;
}

QVariant TreeItemMessage::data(Model *const model, int role)
{
    if (!parent())
        return QVariant();

    // Special item roles which should not trigger fetching of message metadata
    switch (role) {
    case RoleMessageUid:
        return m_uid ? QVariant(m_uid) : QVariant();
    case RoleIsFetched:
        return fetched();
    case RoleIsUnavailable:
        return isUnavailable(model);
    case RoleMessageFlags:
        // The flags are already sorted by Model::normalizeFlags()
        return m_flags;
    case RoleMessageIsMarkedDeleted:
        return isMarkedAsDeleted();
    case RoleMessageIsMarkedRead:
        return isMarkedAsRead();
    case RoleMessageIsMarkedForwarded:
        return isMarkedAsForwarded();
    case RoleMessageIsMarkedReplied:
        return isMarkedAsReplied();
    case RoleMessageIsMarkedRecent:
        return isMarkedAsRecent();
    case RoleMessageFuzzyDate:
    {
        // When the QML ListView is configured with its section.* properties, it will call the corresponding data() section *very*
        // often.  The data are however only "needed" when the real items are visible, and when they are visible, the data() will
        // get called anyway and the correct stuff will ultimately arrive.  This is why we don't call fetch() from here.
        //
        // FIXME: double-check the above once again! Maybe it was just a side effect of too fast updates of the currentIndex?
        if (!fetched()) {
            return QVariant();
        }

        QDateTime timestamp = envelope(model).date;
        if (!timestamp.isValid())
            return QString();

        if (timestamp.date() == QDate::currentDate())
            return Model::tr("Today");

        int beforeDays = timestamp.date().daysTo(QDate::currentDate());
        if (beforeDays >= 0 && beforeDays < 7)
            return Model::tr("Last Week");

        return QDate(timestamp.date().year(), timestamp.date().month(), 1).toString(Model::tr("MMMM yyyy"));
    }
    case RoleMessageWasUnread:
        return m_wasUnread;
    case RoleThreadRootWithUnreadMessages:
        // This one doesn't really make much sense here, but we do want to catch it to prevent a fetch request from this context
        qDebug() << "Warning: asked for RoleThreadRootWithUnreadMessages on TreeItemMessage. This does not make sense.";
        return QVariant();
    case RoleMailboxName:
    case RoleMailboxUidValidity:
        return parent()->parent()->data(model, role);
    }

    // Any other roles will result in fetching the data
    fetch(model);

    switch (role) {
    case Qt::DisplayRole:
        if (loading()) {
            return QString::fromUtf8("[loading UID %1...]").arg(QString::number(uid()));
        } else if (isUnavailable(model)) {
            return QString::fromUtf8("[offline UID %1]").arg(QString::number(uid()));
        } else {
            return QString::fromUtf8("UID %1: %2").arg(QString::number(uid()), data()->m_envelope.subject);
        }
    case Qt::ToolTipRole:
        if (fetched()) {
            QString buf;
            QTextStream stream(&buf);
            stream << data()->m_envelope;
            return buf;
        } else {
            return QVariant();
        }
    default:
        // fall through
        ;
    }

    if (! fetched())
        return QVariant();

    switch (role) {
    case RoleMessageDate:
        return envelope(model).date;
    case RoleMessageInternalDate:
        return data()->m_internalDate;
    case RoleMessageFrom:
        return addresListToQVariant(envelope(model).from);
    case RoleMessageTo:
        return addresListToQVariant(envelope(model).to);
    case RoleMessageCc:
        return addresListToQVariant(envelope(model).cc);
    case RoleMessageBcc:
        return addresListToQVariant(envelope(model).bcc);
    case RoleMessageSender:
        return addresListToQVariant(envelope(model).sender);
    case RoleMessageReplyTo:
        return addresListToQVariant(envelope(model).replyTo);
    case RoleMessageInReplyTo:
        return QVariant::fromValue(envelope(model).inReplyTo);
    case RoleMessageMessageId:
        return envelope(model).messageId;
    case RoleMessageSubject:
        return envelope(model).subject;
    case RoleMessageSize:
        return data()->m_size;
    case RoleMessageHeaderReferences:
        return QVariant::fromValue(data()->m_hdrReferences);
    case RoleMessageHeaderListPost:
    {
        QVariantList res;
        Q_FOREACH(const QUrl &url, data()->m_hdrListPost)
            res << url;
        return res;
    }
    case RoleMessageHeaderListPostNo:
        return data()->m_hdrListPostNo;
    case RoleMessageEnvelope:
        return QVariant::fromValue<Message::Envelope>(envelope(model));
    default:
        return QVariant();
    }
}

bool TreeItemMessage::isMarkedAsDeleted() const
{
    return m_flags.contains(FlagNames::deleted);
}

bool TreeItemMessage::isMarkedAsRead() const
{
    return m_flags.contains(FlagNames::seen);
}

bool TreeItemMessage::isMarkedAsReplied() const
{
    return m_flags.contains(FlagNames::answered);
}

bool TreeItemMessage::isMarkedAsForwarded() const
{
    return m_flags.contains(FlagNames::forwarded);
}

bool TreeItemMessage::isMarkedAsRecent() const
{
    return m_flags.contains(FlagNames::recent);
}

uint TreeItemMessage::uid() const
{
    return m_uid;
}

Message::Envelope TreeItemMessage::envelope(Model *const model)
{
    fetch(model);
    return data()->m_envelope;
}

QDateTime TreeItemMessage::internalDate(Model *const model)
{
    fetch(model);
    return data()->m_internalDate;
}

uint TreeItemMessage::size(Model *const model)
{
    fetch(model);
    return data()->m_size;
}

void TreeItemMessage::setFlags(TreeItemMsgList *list, const QStringList &flags, bool forceChange)
{
    // wasSeen is used to determine if the message was marked as read before this operation
    bool wasSeen = isMarkedAsRead();
    m_flags = flags;
    if (list->m_numberFetchingStatus == DONE && forceChange) {
        bool isSeen = isMarkedAsRead();
        if (m_flagsHandled) {
            if (wasSeen && !isSeen) {
                ++list->m_unreadMessageCount;
                // leave the message as "was unread" so it persists in the view when read messages are hidden
                m_wasUnread = true;
            } else if (!wasSeen && isSeen) {
                --list->m_unreadMessageCount;
            }
        } else {
            // it's a new message
            m_flagsHandled = true;
            if (!isSeen) {
                ++list->m_unreadMessageCount;
                // mark the message as "was unread" so it shows up in the view when read messages are hidden
                m_wasUnread = true;
            }
        }
    }
}

/** @short Process the data found in the headers passed along and file in auxiliary metadata

This function accepts a snippet containing some RFC5322 headers of a message, no matter what headers are actually
present in the passed text.  The headers are parsed and those recognized are used as a source of data to file
the "auxiliary metadata" of this TreeItemMessage (basically anything not available in ENVELOPE, UID, FLAGS,
INTERNALDATE etc).
*/
void TreeItemMessage::processAdditionalHeaders(Model *model, const QByteArray &rawHeaders)
{
    Imap::LowLevelParser::Rfc5322HeaderParser parser;
    bool ok = parser.parse(rawHeaders);
    if (!ok) {
        model->logTrace(0, Common::LOG_OTHER, QLatin1String("Rfc5322HeaderParser"),
                        QLatin1String("Unspecified error during RFC5322 header parsing"));
    }

    data()->m_hdrReferences = parser.references;
    if (!parser.listPost.isEmpty()) {
        data()->m_hdrListPost.clear();
        Q_FOREACH(const QByteArray &item, parser.listPost)
            data()->m_hdrListPost << QUrl(item);
    }
    // That's right, this can only be set, not ever reset from this context.
    // This is because we absolutely want to support incremental header arrival.
    if (parser.listPostNo)
        data()->m_hdrListPostNo = true;
}


TreeItemPart::TreeItemPart(TreeItem *parent, const QString &mimeType):
    TreeItem(parent), m_mimeType(mimeType.toLower()), m_octets(0), m_partMime(0), m_partRaw(0)
{
    if (isTopLevelMultiPart()) {
        // Note that top-level multipart messages are special, their immediate contents
        // can't be fetched. That's why we have to update the status here.
        setFetchStatus(DONE);
    }
}

TreeItemPart::TreeItemPart(TreeItem *parent):
    TreeItem(parent), m_mimeType(QLatin1String("text/plain")), m_octets(0), m_partMime(0), m_partRaw(0)
{
}

TreeItemPart::~TreeItemPart()
{
    delete m_partMime;
    delete m_partRaw;
}

unsigned int TreeItemPart::childrenCount(Model *const model)
{
    Q_UNUSED(model);
    return m_children.size();
}

TreeItem *TreeItemPart::child(const int offset, Model *const model)
{
    Q_UNUSED(model);
    if (offset >= 0 && offset < m_children.size())
        return m_children[ offset ];
    else
        return 0;
}

TreeItemChildrenList TreeItemPart::setChildren(const TreeItemChildrenList &items)
{
    FetchingState fetchStatus = accessFetchStatus();
    auto res = TreeItem::setChildren(items);
    setFetchStatus(fetchStatus);
    return res;
}

void TreeItemPart::fetch(Model *const model)
{
    if (fetched() || loading() || isUnavailable(model))
        return;

    setFetchStatus(LOADING);
    model->askForMsgPart(this);
}

void TreeItemPart::fetchFromCache(Model *const model)
{
    if (fetched() || loading() || isUnavailable(model))
        return;

    model->askForMsgPart(this, true);
}

unsigned int TreeItemPart::rowCount(Model *const model)
{
    // no call to fetch() required
    Q_UNUSED(model);
    return m_children.size();
}

QVariant TreeItemPart::data(Model *const model, int role)
{
    if (!parent())
        return QVariant();

    // these data are available immediately
    switch (role) {
    case RoleIsFetched:
        return fetched();
    case RoleIsUnavailable:
        return isUnavailable(model);
    case RolePartMimeType:
        return m_mimeType;
    case RolePartCharset:
        return m_charset;
    case RolePartContentFormat:
        return m_contentFormat;
    case RolePartContentDelSp:
        return m_delSp;
    case RolePartEncoding:
        return m_encoding;
    case RolePartBodyFldId:
        return m_bodyFldId;
    case RolePartBodyDisposition:
        return m_bodyDisposition;
    case RolePartFileName:
        return m_fileName;
    case RolePartOctets:
        return m_octets;
    case RolePartId:
        return partId();
    case RolePartPathToPart:
        return pathToPart();
    case RolePartMultipartRelatedMainCid:
        if (!multipartRelatedStartPart().isEmpty())
            return multipartRelatedStartPart();
        else
            return QVariant();
    case RoleMailboxName:
    case RoleMailboxUidValidity:
        return message()->parent()->parent()->data(model, role);
    case RoleMessageUid:
        return message()->uid();
    case RolePartIsTopLevelMultipart:
        return isTopLevelMultiPart();
    }


    fetch(model);

    if (loading()) {
        if (role == Qt::DisplayRole) {
            return isTopLevelMultiPart() ?
                   Model::tr("[loading %1...]").arg(m_mimeType) :
                   Model::tr("[loading %1: %2...]").arg(partId()).arg(m_mimeType);
        } else {
            return QVariant();
        }
    }

    switch (role) {
    case Qt::DisplayRole:
        return isTopLevelMultiPart() ?
               QString("%1").arg(m_mimeType) :
               QString("%1: %2").arg(partId()).arg(m_mimeType);
    case Qt::ToolTipRole:
        return m_data.size() > 10000 ? Model::tr("%1 bytes of data").arg(m_data.size()) : m_data;
    case RolePartData:
        return m_data;
    default:
        return QVariant();
    }
}

bool TreeItemPart::hasChildren(Model *const model)
{
    // no need to fetch() here
    Q_UNUSED(model);
    return ! m_children.isEmpty();
}

/** @short Returns true if we're a multipart, top-level item in the body of a message */
bool TreeItemPart::isTopLevelMultiPart() const
{
    TreeItemMessage *msg = dynamic_cast<TreeItemMessage *>(parent());
    TreeItemPart *part = dynamic_cast<TreeItemPart *>(parent());
    return m_mimeType.startsWith("multipart/") && (msg || (part && part->m_mimeType.startsWith("message/")));
}

QString TreeItemPart::partId() const
{
    if (isTopLevelMultiPart()) {
        return QString();
    } else if (dynamic_cast<TreeItemMessage *>(parent())) {
        return QString::number(row() + 1);
    } else {
        QString parentId;
        TreeItemPart *parentPart = dynamic_cast<TreeItemPart *>(parent());
        Q_ASSERT(parentPart);
        if (parentPart->isTopLevelMultiPart()) {
            if (TreeItemPart *parentOfParent = dynamic_cast<TreeItemPart *>(parentPart->parent())) {
                Q_ASSERT(!parentOfParent->isTopLevelMultiPart());
                // grand parent: message/rfc822 with a part-id, parent: top-level multipart
                parentId = parentOfParent->partId();
            } else {
                // grand parent: TreeItemMessage, parent: some multipart, me: some part
                return QString::number(row() + 1);
            }
        } else {
            parentId = parentPart->partId();
        }
        Q_ASSERT(!parentId.isEmpty());
        return parentId + QChar('.') + QString::number(row() + 1);
    }
}

QString TreeItemPart::partIdForFetch(const PartFetchingMode mode) const
{
    return QString::fromUtf8(mode == FETCH_PART_BINARY ? "BINARY.PEEK[%1]" : "BODY.PEEK[%1]").arg(partId());
}

QString TreeItemPart::pathToPart() const
{
    TreeItemPart *part = dynamic_cast<TreeItemPart *>(parent());
    TreeItemMessage *msg = dynamic_cast<TreeItemMessage *>(parent());
    if (part)
        return part->pathToPart() + QLatin1Char('/') + QString::number(row());
    else if (msg)
        return QLatin1Char('/') + QString::number(row());
    else {
        Q_ASSERT(false);
        return QString();
    }
}

TreeItemMessage *TreeItemPart::message() const
{
    const TreeItemPart *part = this;
    while (part) {
        TreeItemMessage *message = dynamic_cast<TreeItemMessage *>(part->parent());
        if (message)
            return message;
        part = dynamic_cast<TreeItemPart *>(part->parent());
    }
    return 0;
}

QByteArray *TreeItemPart::dataPtr()
{
    return &m_data;
}

unsigned int TreeItemPart::columnCount()
{
    if (isTopLevelMultiPart()) {
        // Because a top-level multipart doesn't have its own part number, one cannot really fetch from it
        return 1;
    }

    // This one includes the OFFSET_MIME and OFFSET_RAW_CONTENTS, unlike the TreeItemMessage
    static_assert(OFFSET_MIME < OFFSET_RAW_CONTENTS, "The OFFSET_RAW_CONTENTS shall be the biggest one for tree invariants to work");
    return OFFSET_RAW_CONTENTS + 1;
}

TreeItem *TreeItemPart::specialColumnPtr(int row, int column) const
{
    if (row == 0 && !isTopLevelMultiPart()) {
        switch (column) {
        case OFFSET_MIME:
            if (!m_partMime) {
                m_partMime = new TreeItemModifiedPart(const_cast<TreeItemPart*>(this), OFFSET_MIME);
            }
            return m_partMime;
        case OFFSET_RAW_CONTENTS:
            if (!m_partRaw) {
                m_partRaw = new TreeItemModifiedPart(const_cast<TreeItemPart*>(this), OFFSET_RAW_CONTENTS);
            }
            return m_partRaw;
        }
    }
    return 0;
}

void TreeItemPart::silentlyReleaseMemoryRecursive()
{
    Q_FOREACH(TreeItem *item, m_children) {
        TreeItemPart *part = dynamic_cast<TreeItemPart *>(item);
        Q_ASSERT(part);
        part->silentlyReleaseMemoryRecursive();
    }
    if (m_partMime) {
        m_partMime->silentlyReleaseMemoryRecursive();
        delete m_partMime;
        m_partMime = 0;
    }
    if (m_partRaw) {
        m_partRaw->silentlyReleaseMemoryRecursive();
        delete m_partRaw;
        m_partRaw = 0;
    }
    m_data.clear();
    setFetchStatus(NONE);
    qDeleteAll(m_children);
    m_children.clear();
}



TreeItemModifiedPart::TreeItemModifiedPart(TreeItem *parent, const PartModifier kind):
    TreeItemPart(parent), m_modifier(kind)
{
}

int TreeItemModifiedPart::row() const
{
    // we're always at the very top
    return 0;
}

TreeItem *TreeItemModifiedPart::specialColumnPtr(int row, int column) const
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    // no special children below the current special one
    return 0;
}

bool TreeItemModifiedPart::isTopLevelMultiPart() const
{
    // we're special enough not to ever act like a "top-level multipart"
    return false;
}

unsigned int TreeItemModifiedPart::columnCount()
{
    // no child items, either
    return 0;
}

QString TreeItemModifiedPart::partId() const
{
    if (m_modifier == OFFSET_RAW_CONTENTS) {
        // This item is not directly fetcheable, so it does *not* make sense to ask for it.
        // We cannot really assert at this point, though, because this function is published via the MVC interface.
        return QLatin1String("application-bug-dont-fetch-this");
    } else if (TreeItemPart *part = dynamic_cast<TreeItemPart *>(parent())) {
        // The TreeItemPart is supposed to prevent creation of any special subparts if it's a top-level multipart
        Q_ASSERT(!part->isTopLevelMultiPart());
        return part->partId() + QLatin1Char('.') + modifierToString();
    } else {
        // Our parent is a message/rfc822, and it's definitely not nested -> no need for parent id here
        // Cannot assert() on a dynamic_cast<TreeItemMessage*> at this point because the part is already nullptr at this time
        Q_ASSERT(dynamic_cast<TreeItemMessage*>(parent()));
        return modifierToString();
    }
}

TreeItem::PartModifier TreeItemModifiedPart::kind() const
{
    return m_modifier;
}

QString TreeItemModifiedPart::modifierToString() const
{
    switch (m_modifier) {
    case OFFSET_HEADER:
        return QLatin1String("HEADER");
    case OFFSET_TEXT:
        return QLatin1String("TEXT");
    case OFFSET_MIME:
        return QLatin1String("MIME");
    case OFFSET_RAW_CONTENTS:
        Q_ASSERT(!"Cannot get the fetch modifier for an OFFSET_RAW_CONTENTS item");
        // fall through
    default:
        Q_ASSERT(false);
        return QString();
    }
}

QString TreeItemModifiedPart::pathToPart() const
{
    if (TreeItemPart *parentPart = dynamic_cast<TreeItemPart *>(parent())) {
        return QString::fromUtf8("%1/%2").arg(parentPart->pathToPart(), modifierToString());
    } else {
        Q_ASSERT(dynamic_cast<TreeItemMessage *>(parent()));
        return QString::fromUtf8("/%1").arg(modifierToString());
    }
}

QModelIndex TreeItemModifiedPart::toIndex(Model *const model) const
{
    Q_ASSERT(model);
    // see TreeItem::toIndex() for the const_cast explanation
    return model->createIndex(row(), static_cast<int>(kind()), const_cast<TreeItemModifiedPart *>(this));
}

QString TreeItemModifiedPart::partIdForFetch(const PartFetchingMode mode) const
{
    Q_UNUSED(mode);
    // Don't try to use BINARY for special message parts, it's forbidden. One can only use that for the "regular" MIME parts
    return TreeItemPart::partIdForFetch(FETCH_PART_IMAP);
}

TreeItemPartMultipartMessage::TreeItemPartMultipartMessage(TreeItem *parent, const Message::Envelope &envelope):
    TreeItemPart(parent, QLatin1String("message/rfc822")), m_envelope(envelope), m_partHeader(0), m_partText(0)
{
}

TreeItemPartMultipartMessage::~TreeItemPartMultipartMessage()
{
}

/** @short Overridden from TreeItemPart::data with added support for RoleMessageEnvelope */
QVariant TreeItemPartMultipartMessage::data(Model * const model, int role)
{
    if (role == RoleMessageEnvelope) {
        fetch(model);
        return QVariant::fromValue<Message::Envelope>(m_envelope);
    } else {
        return TreeItemPart::data(model, role);
    }
}

TreeItem *TreeItemPartMultipartMessage::specialColumnPtr(int row, int column) const
{
    if (row != 0)
        return 0;
    switch (column) {
    case OFFSET_HEADER:
        if (!m_partHeader) {
            m_partHeader = new TreeItemModifiedPart(const_cast<TreeItemPartMultipartMessage*>(this), OFFSET_HEADER);
        }
        return m_partHeader;
    case OFFSET_TEXT:
        if (!m_partText) {
            m_partText = new TreeItemModifiedPart(const_cast<TreeItemPartMultipartMessage*>(this), OFFSET_TEXT);
        }
        return m_partText;
    default:
        return TreeItemPart::specialColumnPtr(row, column);
    }
}

void TreeItemPartMultipartMessage::silentlyReleaseMemoryRecursive()
{
    TreeItemPart::silentlyReleaseMemoryRecursive();
    if (m_partHeader) {
        m_partHeader->silentlyReleaseMemoryRecursive();
        delete m_partHeader;
        m_partHeader = 0;
    }
    if (m_partText) {
        m_partText->silentlyReleaseMemoryRecursive();
        delete m_partText;
        m_partText = 0;
    }
}

}
}
