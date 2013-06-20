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

#include <algorithm>
#include <QTextStream>
#include "Common/FindWithUnknown.h"
#include "Imap/Encoders.h"
#include "Imap/Parser/Rfc5322HeaderParser.h"
#include "Imap/Tasks/KeepMailboxOpenTask.h"
#include "DelayedPopulation.h"
#include "ItemRoles.h"
#include "MailboxTree.h"
#include "Model.h"
#include <QtDebug>

namespace
{

void decodeMessagePartTransportEncoding(const QByteArray &rawData, const QByteArray &encoding, QByteArray *outputData)
{
    Q_ASSERT(outputData);
    if (encoding == "quoted-printable") {
        *outputData = Imap::quotedPrintableDecode(rawData);
    } else if (encoding == "base64") {
        *outputData = QByteArray::fromBase64(rawData);
    } else if (encoding.isEmpty() || encoding == "7bit" || encoding == "8bit" || encoding == "binary") {
        *outputData = rawData;
    } else {
        qDebug() << "Warning: unknown encoding" << encoding;
        *outputData = rawData;
    }
}

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

TreeItem::TreeItem(TreeItem *parent): m_parent(parent), m_fetchStatus(NONE)
{
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
    return m_parent ? m_parent->m_children.indexOf(const_cast<TreeItem *>(this)) : 0;
}

QList<TreeItem *> TreeItem::setChildren(const QList<TreeItem *> items)
{
    QList<TreeItem *> res = m_children;
    m_children = items;
    m_fetchStatus = DONE;
    return res;
}

bool TreeItem::isUnavailable(Model *const model) const
{
    return m_fetchStatus == UNAVAILABLE && model->networkPolicy() == Model::NETWORK_OFFLINE;
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

    if (hasNoChildMaliboxesAlreadyKnown()) {
        m_fetchStatus = DONE;
        return;
    }

    if (! loading()) {
        m_fetchStatus = LOADING;
        // It's possible that we've got invoked in response to something relatively harmless like rowCount(),
        // that's why we have to delay the call to askForChildrenOfMailbox() until we re-enter the event
        // loop.
        new DelayedAskForChildrenOfMailbox(model, toIndex(model),
                                           forceReload ?
                                               DelayedAskForChildrenOfMailbox::CACHE_FORCE_RELOAD :
                                               DelayedAskForChildrenOfMailbox::CACHE_NORMAL);
    }
}

void TreeItemMailbox::rescanForChildMailboxes(Model *const model)
{
    // FIXME: fix duplicate requests (ie. don't allow more when some are on their way)
    // FIXME: gotta be fixed in the Model, or spontaneous replies from server can break us
    m_fetchStatus = NONE;
    fetchWithCacheControl(model, true);
}

unsigned int TreeItemMailbox::rowCount(Model *const model)
{
    fetch(model);
    return m_children.size();
}

QVariant TreeItemMailbox::data(Model *const model, int role)
{
    if (! m_parent)
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
    case RoleShortMailboxName:
        return separator().isEmpty() ? mailbox() : mailbox().split(separator(), QString::SkipEmptyParts).last();
    case RoleMailboxName:
        return mailbox();
    case RoleMailboxSeparator:
        return separator();
    case RoleMailboxHasChildmailboxes:
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

bool TreeItemMailbox::hasNoChildMaliboxesAlreadyKnown()
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
    } else if (hasNoChildMaliboxesAlreadyKnown()) {
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

QList<TreeItem *> TreeItemMailbox::setChildren(const QList<TreeItem *> items)
{
    // This function has to be special because we want to preserve m_children[0]

    TreeItemMsgList *msgList = dynamic_cast<TreeItemMsgList *>(m_children[0]);
    Q_ASSERT(msgList);
    m_children.removeFirst();

    QList<TreeItem *> list = TreeItem::setChildren(items);  // this also adjusts m_loading and m_fetched

    m_children.prepend(msgList);

    // FIXME: anything else required for \Noselect?
    if (! isSelectable())
        msgList->m_fetchStatus = DONE;

    return list;
}

void TreeItemMailbox::handleFetchResponse(Model *const model,
        const Responses::Fetch &response,
        QList<TreeItemPart *> &changedParts,
        TreeItemMessage *&changedMessage, bool canSaveSyncStateDirectly, bool usingQresync)
{
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(m_children[0]);
    Q_ASSERT(list);
    if (! list->fetched() && !usingQresync) {
        QByteArray buf;
        QTextStream ss(&buf);
        ss << response;
        ss.flush();
        qDebug() << "Ignoring FETCH response to a mailbox that isn't synced yet:" << buf;
        return;
    }

    int number = response.number - 1;
    if (number < 0 || number >= list->m_children.size())
        throw UnknownMessageIndex(QString::fromUtf8("Got FETCH that is out of bounds -- got %1 messages").arg(
                                      QString::number(list->m_children.size())).toUtf8().constData(), response);

    TreeItemMessage *message = dynamic_cast<TreeItemMessage *>(list->child(number, model));
    Q_ASSERT(message);   // FIXME: this should be relaxed for allowing null pointers instead of "unfetched" TreeItemMessage

    // At first, have a look at the response and check the UID of the message
    Responses::Fetch::dataType::const_iterator uidRecord = response.data.find("UID");
    if (uidRecord != response.data.constEnd()) {
        uint receivedUid = dynamic_cast<const Responses::RespData<uint>&>(*(uidRecord.value())).data;
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
                message->m_fetchStatus = NONE;
                message->fetch(model);
            }
            if (canSaveSyncStateDirectly && syncState.uidNext() <= receivedUid) {
                // Try to guess the UIDNEXT. We have to take an educated guess here, and I believe that this approach
                // at least is not wrong. The server won't tell us the UIDNEXT (well, it could, but it doesn't have to),
                // the only way of asking for it is via STATUS which is not allowed to reference the current mailbox and
                // even if it was, it wouldn't be atomic. So, what could the UIDNEXT possibly be? It can't be smaller
                // than the UID_of_highest_message, and it can't be the same, either, so it really has to be higher.
                // Let's just increment it by one, this is our lower bound.
                // Not guessing the UIDNEXT correctly would result at decreased performance at the next sync, and we
                // can't really do better -> let's just set it now, along with the UID mapping.
                syncState.setUidNext(receivedUid + 1);
                model->cache()->setMailboxSyncState(mailbox(), syncState);
                model->saveUidMap(list);
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
            uidsInMailbox << dynamic_cast<TreeItemMessage *>(node)->uid();
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
            Q_ASSERT(dynamic_cast<const Responses::RespData<uint>&>(*(it.value())).data == message->uid());
        } else if (it.key() == "ENVELOPE") {
            message->m_envelope = dynamic_cast<const Responses::RespData<Message::Envelope>&>(*(it.value())).data;
            message->m_fetchStatus = DONE;
            gotEnvelope = true;
            changedMessage = message;
        } else if (it.key() == "BODYSTRUCTURE") {
            if (message->fetched()) {
                // The message structure is already known, so we are free to ignore it
            } else {
                // We had no idea about the structure of the message
                QList<TreeItem *> newChildren = dynamic_cast<const Message::AbstractMessage &>(*(it.value())).createTreeItems(message);
                if (! message->m_children.isEmpty()) {
                    QModelIndex messageIdx = message->toIndex(model);
                    model->beginRemoveRows(messageIdx, 0, message->m_children.size() - 1);
                    QList<TreeItem *> oldChildren = message->setChildren(newChildren);
                    model->endRemoveRows();
                    qDeleteAll(oldChildren);
                } else {
                    QList<TreeItem *> oldChildren = message->setChildren(newChildren);
                    Q_ASSERT(oldChildren.size() == 0);
                }
                savedBodyStructure = true;
            }
        } else if (it.key() == "x-trojita-bodystructure") {
            // do nothing
        } else if (it.key() == "RFC822.SIZE") {
            message->m_size = dynamic_cast<const Responses::RespData<uint>&>(*(it.value())).data;
            gotSize = true;
        } else if (it.key().startsWith("BODY[HEADER.FIELDS (")) {
            // Process any headers found in any such response bit
            const QByteArray &rawHeaders = dynamic_cast<const Responses::RespData<QByteArray>&>(*(it.value())).data;
            message->processAdditionalHeaders(model, rawHeaders);
            changedMessage = message;
        } else if (it.key().startsWith("BODY[") || it.key().startsWith("BINARY[")) {
            if (it.key()[ it.key().size() - 1 ] != ']')
                throw UnknownMessageIndex("Can't parse such BODY[]/BINARY[]", response);
            TreeItemPart *part = partIdToPtr(model, message, it.key());
            if (! part)
                throw UnknownMessageIndex("Got BODY[]/BINARY[] fetch that did not resolve to any known part", response);
            const QByteArray &data = dynamic_cast<const Responses::RespData<QByteArray>&>(*(it.value())).data;
            if (it.key().startsWith("BODY[")) {
                // got to decode the part data by hand
                decodeMessagePartTransportEncoding(data, part->encoding(), part->dataPtr());
            } else {
                // A BINARY FETCH item is already decoded for us, yay
                part->m_data = data;
            }
            part->m_fetchStatus = DONE;
            if (message->uid())
                model->cache()->setMsgPart(mailbox(), message->uid(), part->partId(), part->m_data);
            changedParts.append(part);
        } else if (it.key() == "FLAGS") {
            // Only emit signals when the flags have actually changed
            QStringList newFlags = model->normalizeFlags(dynamic_cast<const Responses::RespData<QStringList>&>(*(it.value())).data);
            bool forceChange = (message->m_flags != newFlags);
            message->setFlags(list, newFlags, forceChange);
            if (forceChange) {
                updatedFlags = true;
                changedMessage = message;
            }
        } else if (it.key() == "INTERNALDATE") {
            message->m_internalDate = dynamic_cast<const Responses::RespData<QDateTime>&>(*(it.value())).data;
            gotInternalDate = true;
        } else if (it.key() == "MODSEQ") {
            quint64 num = dynamic_cast<const Responses::RespData<quint64>&>(*(it.value())).data;
            if (num > syncState.highestModSeq()) {
                syncState.setHighestModSeq(num);
                // FIXME: when shall we save this one to the persistent cache?
            }
        } else {
            qDebug() << "TreeItemMailbox::handleFetchResponse: unknown FETCH identifier" << it.key();
        }
    }
    if (message->uid()) {
        if (gotEnvelope && gotSize && savedBodyStructure && gotInternalDate) {
            Imap::Mailbox::AbstractCache::MessageDataBundle dataForCache;
            dataForCache.envelope = message->m_envelope;
            dataForCache.serializedBodyStructure = dynamic_cast<const Responses::RespData<QByteArray>&>(*(response.data[ "x-trojita-bodystructure" ])).data;
            dataForCache.size = message->m_size;
            dataForCache.uid = message->uid();
            dataForCache.internalDate = message->m_internalDate;
            dataForCache.hdrReferences = message->m_hdrReferences;
            dataForCache.hdrListPost = message->m_hdrListPost;
            dataForCache.hdrListPostNo = message->m_hdrListPostNo;
            model->cache()->setMessageMetadata(mailbox(), message->uid(), dataForCache);
        }
        if (updatedFlags) {
            model->cache()->setMsgFlags(mailbox(), message->uid(), message->m_flags);
        }
    }
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
    TreeItemMessage *message = static_cast<TreeItemMessage *>(list->m_children.takeAt(offset));
    model->cache()->clearMessage(static_cast<TreeItemMailbox *>(list->parent())->mailbox(), message->uid());
    for (int i = offset; i < list->m_children.size(); ++i) {
        --static_cast<TreeItemMessage *>(list->m_children[i])->m_offset;
    }
    model->endRemoveRows();
    delete message;

    --list->m_totalMessageCount;
    list->recalcVariousMessageCounts(const_cast<Model *>(model));
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

    QList<TreeItem *>::iterator it = list->m_children.end();
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
        for (QList<TreeItem*>::iterator furtherMessage = it; furtherMessage != list->m_children.end(); ++furtherMessage) {
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
    Q_ASSERT(item->parent()->fetched());   // TreeItemMsgList
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
    if (! parent->parent())
        m_fetchStatus = DONE;
}

void TreeItemMsgList::fetch(Model *const model)
{
    if (fetched() || isUnavailable(model))
        return;

    if (! loading()) {
        m_fetchStatus = LOADING;
        // We can't ask right now, has to wait till the end of the event loop
        new DelayedAskForMessagesInMailbox(model, toIndex(model));
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

    if (role != Qt::DisplayRole)
        return QVariant();

    if (! m_parent)
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



TreeItemMessage::TreeItemMessage(TreeItem *parent):
    TreeItem(parent), m_size(0), m_uid(0), m_hdrListPostNo(false), m_flagsHandled(false), m_offset(-1), m_wasUnread(false),
    m_partHeader(0), m_partText(0)
{
}

TreeItemMessage::~TreeItemMessage()
{
    delete m_partHeader;
    delete m_partText;
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
        m_fetchStatus = LOADING;
    }
}

unsigned int TreeItemMessage::rowCount(Model *const model)
{
    fetch(model);
    return m_children.size();
}

unsigned int TreeItemMessage::columnCount()
{
    return 3;
}

TreeItem *TreeItemMessage::specialColumnPtr(int row, int column) const
{
    // This is a nasty one -- we have an irregular shape...

    // No extra columns on other rows
    if (row != 0)
        return 0;

    if (!m_partText) {
        m_partText = new TreeItemModifiedPart(const_cast<TreeItemMessage *>(this), OFFSET_TEXT);
    }
    if (!m_partHeader) {
        m_partHeader = new TreeItemModifiedPart(const_cast<TreeItemMessage *>(this), OFFSET_HEADER);
    }

    switch (column) {
    case OFFSET_TEXT:
        return m_partText;
    case OFFSET_HEADER:
        return m_partHeader;
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
    if (! m_parent)
        return QVariant();

    // Special item roles which should not trigger fetching of message metadata
    switch (role) {
    case RoleMessageUid:
        return m_uid ? QVariant(m_uid) : QVariant();
    case RoleIsFetched:
        return fetched();
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
            return QString::fromUtf8("UID %1: %2").arg(QString::number(uid()), m_envelope.subject);
        }
    case Qt::ToolTipRole:
        if (fetched()) {
            QString buf;
            QTextStream stream(&buf);
            stream << m_envelope;
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
        return m_internalDate;
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
        return m_size;
    case RoleMessageHeaderReferences:
        return QVariant::fromValue(m_hdrReferences);
    case RoleMessageHeaderListPost:
    {
        QVariantList res;
        Q_FOREACH(const QUrl &url, m_hdrListPost)
            res << url;
        return res;
    }
    case RoleMessageHeaderListPostNo:
        return m_hdrListPostNo;
    case RoleMessageEnvelope:
        return QVariant::fromValue<Message::Envelope>(envelope(model));
    default:
        return QVariant();
    }
}

bool TreeItemMessage::isMarkedAsDeleted() const
{
    return m_flags.contains(QLatin1String("\\Deleted"));
}

bool TreeItemMessage::isMarkedAsRead() const
{
    return m_flags.contains(QLatin1String("\\Seen"));
}

bool TreeItemMessage::isMarkedAsReplied() const
{
    return m_flags.contains(QLatin1String("\\Answered"));
}

bool TreeItemMessage::isMarkedAsForwarded() const
{
    return m_flags.contains(QLatin1String("$Forwarded"));
}

bool TreeItemMessage::isMarkedAsRecent() const
{
    return m_flags.contains(QLatin1String("\\Recent"));
}

uint TreeItemMessage::uid() const
{
    return m_uid;
}

Message::Envelope TreeItemMessage::envelope(Model *const model)
{
    fetch(model);
    return m_envelope;
}

QDateTime TreeItemMessage::internalDate(Model *const model)
{
    fetch(model);
    return m_internalDate;
}

uint TreeItemMessage::size(Model *const model)
{
    fetch(model);
    return m_size;
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

    m_hdrReferences = parser.references;
    if (!parser.listPost.isEmpty()) {
        m_hdrListPost.clear();
        Q_FOREACH(const QByteArray &item, parser.listPost)
            m_hdrListPost << QUrl(item);
    }
    // That's right, this can only be set, not ever reset from this context.
    // This is because we absolutely want to support incremental header arrival.
    if (parser.listPostNo)
        m_hdrListPostNo = true;
}


TreeItemPart::TreeItemPart(TreeItem *parent, const QString &mimeType): TreeItem(parent), m_mimeType(mimeType.toLower()), m_octets(0)
{
    if (isTopLevelMultiPart()) {
        // Note that top-level multipart messages are special, their immediate contents
        // can't be fetched. That's why we have to update the status here.
        m_fetchStatus = DONE;
    }
    m_partHeader = new TreeItemModifiedPart(this, OFFSET_HEADER);
    m_partMime = new TreeItemModifiedPart(this, OFFSET_MIME);
    m_partText = new TreeItemModifiedPart(this, OFFSET_TEXT);
}

TreeItemPart::TreeItemPart(TreeItem *parent):
    TreeItem(parent), m_mimeType(QLatin1String("text/plain")), m_octets(0), m_partHeader(0), m_partText(0), m_partMime(0)
{
}

TreeItemPart::~TreeItemPart()
{
    delete m_partHeader;
    delete m_partMime;
    delete m_partText;
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

QList<TreeItem *> TreeItemPart::setChildren(const QList<TreeItem *> items)
{
    FetchingState fetchStatus = m_fetchStatus;
    QList<TreeItem *> res = TreeItem::setChildren(items);
    m_fetchStatus = fetchStatus;
    return res;
}

void TreeItemPart::fetch(Model *const model)
{
    if (fetched() || loading() || isUnavailable(model))
        return;

    m_fetchStatus = LOADING;
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
    if (! m_parent)
        return QVariant();

    // these data are available immediately
    switch (role) {
    case RoleIsFetched:
        return fetched();
    case RolePartMimeType:
        return m_mimeType;
    case RolePartCharset:
        return m_charset;
    case RolePartContentFormat:
        return m_contentFormat;
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
    return  m_mimeType.startsWith("multipart/") && (msg || (part && part->m_mimeType.startsWith("message/")));
}

QString TreeItemPart::partId() const
{
    if (isTopLevelMultiPart()) {
        TreeItemPart *part = dynamic_cast<TreeItemPart *>(parent());
        if (part)
            return part->partId();
        else
            return QString();
    } else if (dynamic_cast<TreeItemMessage *>(parent())) {
        return QString::number(row() + 1);
    } else {
        QString parentId = dynamic_cast<TreeItemPart *>(parent())->partId();
        if (parentId.isNull())
            return QString::number(row() + 1);
        else
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
    return 4; // This one includes the OFFSET_MIME
}

TreeItem *TreeItemPart::specialColumnPtr(int row, int column) const
{
    // No extra columns on other rows
    if (row != 0)
        return 0;

    switch (column) {
    case OFFSET_HEADER:
        return m_partHeader;
    case OFFSET_TEXT:
        return m_partText;
    case OFFSET_MIME:
        return m_partMime;
    default:
        return 0;
    }
}

void TreeItemPart::silentlyReleaseMemoryRecursive()
{
    Q_FOREACH(TreeItem *item, m_children) {
        TreeItemPart *part = dynamic_cast<TreeItemPart *>(item);
        Q_ASSERT(part);
        part->silentlyReleaseMemoryRecursive();
    }
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
    if (m_partMime) {
        m_partMime->silentlyReleaseMemoryRecursive();
        delete m_partMime;
        m_partMime = 0;
    }
    m_data.clear();
    m_fetchStatus = NONE;
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
    QString parentId;

    if (TreeItemPart *part = dynamic_cast<TreeItemPart *>(parent()))
        parentId = part->partId() + QLatin1Char('.');

    return parentId + modifierToString();
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
    default:
        Q_ASSERT(false);
        return QString();
    }
}

QString TreeItemModifiedPart::pathToPart() const
{
    TreeItemPart *parentPart = dynamic_cast<TreeItemPart *>(parent());
    TreeItemMessage *parentMessage = dynamic_cast<TreeItemMessage *>(parent());
    Q_ASSERT(parentPart || parentMessage);
    if (parentPart) {
        return QString::fromUtf8("%1/%2").arg(parentPart->pathToPart(), modifierToString());
    } else {
        Q_ASSERT(parentMessage);
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
    TreeItemPart(parent, QLatin1String("message/rfc822")), m_envelope(envelope)
{
}

TreeItemPartMultipartMessage::~TreeItemPartMultipartMessage()
{
}

/** @short Overriden from TreeItemPart::data with added support for RoleMessageEnvelope */
QVariant TreeItemPartMultipartMessage::data(Model * const model, int role)
{
    if (role == RoleMessageEnvelope) {
        fetch(model);
        return QVariant::fromValue<Message::Envelope>(m_envelope);
    } else {
        return TreeItemPart::data(model, role);
    }
}

}
}
