/* Copyright (C) 2007 - 2011 Jan Kundrát <jkt@flaska.net>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "ObtainSynchronizedMailboxTask.h"
#include <sstream>
#include <QTimer>
#include "ItemRoles.h"
#include "KeepMailboxOpenTask.h"
#include "MailboxTree.h"
#include "Model.h"
#include "UnSelectTask.h"

namespace Imap
{
namespace Mailbox
{

ObtainSynchronizedMailboxTask::ObtainSynchronizedMailboxTask(Model *model, const QModelIndex &mailboxIndex, ImapTask *parentTask,
        KeepMailboxOpenTask *keepTask):
    ImapTask(model), conn(parentTask), mailboxIndex(mailboxIndex), status(STATE_WAIT_FOR_CONN), uidSyncingMode(UID_SYNC_ALL),
    unSelectTask(0), keepTaskChild(keepTask)
{
    if (conn) {
        conn->addDependentTask(this);
    }
    addDependentTask(keepTaskChild);
}

void ObtainSynchronizedMailboxTask::addDependentTask(ImapTask *task)
{
    if (!dependentTasks.isEmpty()) {
        throw CantHappen("Attempted to add another dependent task to an ObtainSynchronizedMailboxTask");
    }
    ImapTask::addDependentTask(task);
}

void ObtainSynchronizedMailboxTask::perform()
{
    // The Parser* is not provided by our parent task, but instead through the keepTaskChild.  The reason is simple, the parent
    // task might not even exist, but there's always an KeepMailboxOpenTask in the game.
    parser = keepTaskChild->parser;
    markAsActiveTask();

    if (_dead || _aborted) {
        // We're at the very start, so let's try to abort in a sane way
        _failed("Asked to abort or die");
        die();
        return;
    }

    if (! mailboxIndex.isValid()) {
        // FIXME: proper error handling
        log("The mailbox went missing, sorry", LOG_MAILBOX_SYNC);
        _completed();
        return;
    }

    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);
    TreeItemMsgList *msgList = dynamic_cast<TreeItemMsgList *>(mailbox->m_children[0]);
    Q_ASSERT(msgList);

    msgList->m_fetchStatus = TreeItem::LOADING;

    QMap<Parser *,ParserState>::iterator it = model->m_parsers.find(parser);
    Q_ASSERT(it != model->m_parsers.end());

    selectCmd = parser->select(mailbox->mailbox());
    mailbox->syncState = SyncState();
    status = STATE_SELECTING;
    log("Synchronizing mailbox", LOG_MAILBOX_SYNC);
    emit model->mailboxSyncingProgress(mailboxIndex, status);
}

bool ObtainSynchronizedMailboxTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (dieIfInvalidMailbox())
        return true;

    if (handleResponseCodeInsideState(resp))
        return true;

    if (resp->tag.isEmpty())
        return false;

    if (_dead) {
        _failed("Asked to die");
        return true;
    }
    // We absolutely have to ignore the abort() request

    if (resp->tag == selectCmd) {

        if (resp->kind == Responses::OK) {
            //qDebug() << "received OK for selectCmd";
            Q_ASSERT(status == STATE_SELECTING);
            finalizeSelect();
        } else {
            _failed("SELECT failed");
            // FIXME: error handling
            model->changeConnectionState(parser, CONN_STATE_AUTHENTICATED);
        }
        return true;
    } else if (resp->tag == uidSyncingCmd) {

        if (resp->kind == Responses::OK) {
            log("UIDs synchronized", LOG_MAILBOX_SYNC);
            Q_ASSERT(status == STATE_SYNCING_UIDS);
            Q_ASSERT(mailboxIndex.isValid());   // FIXME
            TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
            Q_ASSERT(mailbox);
            switch (uidSyncingMode) {
            case UID_SYNC_ONLY_NEW:
                finalizeUidSyncOnlyNew(model, mailbox, model->cache()->mailboxSyncState(mailbox->mailbox()).exists(), uidMap);
                model->changeConnectionState(parser, CONN_STATE_SELECTED);
                break;
            default:
                finalizeUidSyncAll(mailbox);
            }
            syncFlags(mailbox);
        } else {
            _failed("UID syncing failed");
            // FIXME: error handling
        }
        return true;
    } else if (resp->tag == flagsCmd) {

        if (resp->kind == Responses::OK) {
            //qDebug() << "received OK for flagsCmd";
            Q_ASSERT(status == STATE_SYNCING_FLAGS);
            Q_ASSERT(mailboxIndex.isValid());   // FIXME
            TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
            Q_ASSERT(mailbox);
            status = STATE_DONE;
            log("Flags synchronized", LOG_MAILBOX_SYNC);
            notifyInterestingMessages(mailbox);
            model->emitMessageCountChanged(mailbox);
            flagsCmd.clear();

            if (newArrivalsFetch.isEmpty()) {
                _completed();
            } else {
                log("Pending new arrival fetching, not terminating yet", LOG_MAILBOX_SYNC);
            }
        } else {
            status = STATE_DONE;
            _failed("Flags synchronization failed");
            // FIXME: error handling
        }
        emit model->mailboxSyncingProgress(mailboxIndex, status);
        return true;
    } else if (newArrivalsFetch.contains(resp->tag)) {

        if (resp->kind == Responses::OK) {
            newArrivalsFetch.removeOne(resp->tag);

            if (newArrivalsFetch.isEmpty() && status == STATE_DONE && flagsCmd.isEmpty()) {
                _completed();
            }
        } else {
            _failed("UID discovery of new arrivals after initial UID sync has failed");
        }
        return true;

    } else {
        return false;
    }
}

void ObtainSynchronizedMailboxTask::finalizeSelect()
{
    Q_ASSERT(mailboxIndex.isValid());
    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(mailbox->m_children[ 0 ]);
    Q_ASSERT(list);

    model->changeConnectionState(parser, CONN_STATE_SYNCING);
    const SyncState &syncState = mailbox->syncState;
    const SyncState &oldState = model->cache()->mailboxSyncState(mailbox->mailbox());
    list->m_totalMessageCount = syncState.exists();
    // Note: syncState.unSeen() is the NUMBER of the first unseen message, not their count!

    const QList<uint> &seqToUid = model->cache()->uidMapping(mailbox->mailbox());

    if (static_cast<uint>(seqToUid.size()) != oldState.exists() ||
        oldState.exists() != static_cast<uint>(list->m_children.size())) {

        QString buf;
        QDebug dbg(&buf);
        dbg << "Inconsistent cache data, falling back to full sync (" <<
            seqToUid.size() << "in UID map," << oldState.exists() <<
            "EXIST before," << list->m_children.size() << "nodes)";
        log(buf, LOG_MAILBOX_SYNC);
        fullMboxSync(mailbox, list, syncState);
    } else {
        if (syncState.isUsableForSyncing() && oldState.isUsableForSyncing() && syncState.uidValidity() == oldState.uidValidity()) {
            // Perform a nice re-sync

            if (syncState.uidNext() == oldState.uidNext()) {
                // No new messages

                if (syncState.exists() == oldState.exists()) {
                    // No deletions, either, so we resync only flag changes
                    syncNoNewNoDeletions(mailbox, list, syncState, seqToUid);
                } else {
                    // Some messages got deleted, but there have been no additions
                    //_syncOnlyDeletions( mailbox, list, syncState );
                    fullMboxSync(mailbox, list, syncState);
                }

            } else if (syncState.uidNext() > oldState.uidNext()) {
                // Some new messages were delivered since we checked the last time.
                // There's no guarantee they are still present, though.

                if (syncState.uidNext() - oldState.uidNext() == syncState.exists() - oldState.exists()) {
                    // Only some new arrivals, no deletions
                    syncOnlyAdditions(mailbox, list, syncState, oldState);
                } else {
                    // Generic case; we don't know anything about which messages were deleted and which added
                    //_syncGeneric( mailbox, list, syncState );
                    fullMboxSync(mailbox, list, syncState);
                }
            } else {
                // The UIDNEXT has decreased while UIDVALIDITY remains the same. This is forbidden,
                // so either a server's bug, or a completely invalid cache.
                Q_ASSERT(syncState.uidNext() < oldState.uidNext());
                Q_ASSERT(syncState.uidValidity() == oldState.uidValidity());
                log("Yuck, UIDVALIDITY remains same but UIDNEXT decreased", LOG_MAILBOX_SYNC);
                model->cache()->clearAllMessages(mailbox->mailbox());
                fullMboxSync(mailbox, list, syncState);
            }
        } else {
            // Forget everything, do a dumb sync
            model->cache()->clearAllMessages(mailbox->mailbox());
            fullMboxSync(mailbox, list, syncState);
        }
    }
}

void ObtainSynchronizedMailboxTask::fullMboxSync(TreeItemMailbox *mailbox, TreeItemMsgList *list, const SyncState &syncState)
{
    log("Full synchronization", LOG_MAILBOX_SYNC);
    model->cache()->clearUidMapping(mailbox->mailbox());
    model->cache()->setMailboxSyncState(mailbox->mailbox(), SyncState());

    QModelIndex parent = list->toIndex(model);
    if (! list->m_children.isEmpty()) {
        model->beginRemoveRows(parent, 0, list->m_children.size() - 1);
        QList<TreeItem*> oldItems = list->m_children;
        list->m_children.clear();
        model->endRemoveRows();
        qDeleteAll(oldItems);
    }
    if (syncState.exists()) {
        model->beginInsertRows(parent, 0, syncState.exists() - 1);
        for (uint i = 0; i < syncState.exists(); ++i) {
            TreeItemMessage *msg = new TreeItemMessage(list);
            msg->m_offset = i;
            list->m_children << msg;
        }
        model->endInsertRows();

        syncUids(mailbox);
        list->m_numberFetchingStatus = TreeItem::LOADING;
        list->m_unreadMessageCount = 0;
    } else {
        // No messages, we're done here
        list->m_totalMessageCount = 0;
        list->m_unreadMessageCount = 0;
        list->m_numberFetchingStatus = TreeItem::DONE;
        list->m_fetchStatus = TreeItem::DONE;
        model->cache()->setMailboxSyncState(mailbox->mailbox(), syncState);
        model->saveUidMap(list);

        // The remote mailbox is empty -> we're done now
        model->changeConnectionState(parser, CONN_STATE_SELECTED);
        status = STATE_DONE;
        emit model->mailboxSyncingProgress(mailboxIndex, status);
        notifyInterestingMessages(mailbox);
        // Take care here: this call could invalidate our index (see test coverage)
        _completed();
    }
    // Our mailbox might have actually been invalidated by various callbacks activated above
    if (mailboxIndex.isValid()) {
        Q_ASSERT(mailboxIndex.internalPointer() == mailbox);
        model->emitMessageCountChanged(mailbox);
    }
}

void ObtainSynchronizedMailboxTask::syncNoNewNoDeletions(TreeItemMailbox *mailbox, TreeItemMsgList *list, const SyncState &syncState, const QList<uint> &seqToUid)
{
    Q_ASSERT(syncState.exists() == static_cast<uint>(seqToUid.size()));
    log("No arrivals or deletions since the last time", LOG_MAILBOX_SYNC);
    if (syncState.exists()) {
        // Verify that we indeed have all UIDs and not need them anymore
        bool uidsOk = true;
        for (int i = 0; i < list->m_children.size(); ++i) {
            if (! static_cast<TreeItemMessage *>(list->m_children[i])->uid()) {
                uidsOk = false;
                break;
            }
        }
        // FIXME: This assert can fail if the mailbox contained messages with missing UIDs even before we opened it now.
        Q_ASSERT(uidsOk);
    } else {
        list->m_unreadMessageCount = 0;
        list->m_totalMessageCount = 0;
        list->m_numberFetchingStatus = TreeItem::DONE;
    }

    if (list->m_children.isEmpty()) {
        QList<TreeItem *> messages;
        for (uint i = 0; i < syncState.exists(); ++i) {
            TreeItemMessage *msg = new TreeItemMessage(list);
            msg->m_offset = i;
            msg->m_uid = seqToUid[ i ];
            messages << msg;
        }
        list->setChildren(messages);

    } else {
        if (syncState.exists() != static_cast<uint>(list->m_children.size())) {
            throw CantHappen("TreeItemMsgList has wrong number of "
                             "children, even though no change of "
                             "message count occured");
        }
    }

    list->m_fetchStatus = TreeItem::DONE;
    model->cache()->setMailboxSyncState(mailbox->mailbox(), syncState);
    model->saveUidMap(list);

    if (syncState.exists()) {
        syncFlags(mailbox);
    } else {
        status = STATE_DONE;
        emit model->mailboxSyncingProgress(mailboxIndex, status);
        notifyInterestingMessages(mailbox);

        if (newArrivalsFetch.isEmpty()) {
            _completed();
        }
    }
}

void ObtainSynchronizedMailboxTask::syncOnlyDeletions(TreeItemMailbox *mailbox, TreeItemMsgList *list, const SyncState &syncState)
{
    log("Some messages got deleted, but no new arrivals", LOG_MAILBOX_SYNC);
    list->m_numberFetchingStatus = TreeItem::LOADING;
    list->m_unreadMessageCount = 0; // FIXME: this case needs further attention...
    uidMap.clear();
    for (uint i = 0; i < syncState.exists(); ++i)
        uidMap << 0;
    model->cache()->clearUidMapping(mailbox->mailbox());
    syncUids(mailbox);
}

void ObtainSynchronizedMailboxTask::syncOnlyAdditions(TreeItemMailbox *mailbox, TreeItemMsgList *list, const SyncState &syncState, const SyncState &oldState)
{
    log("Syncing new arrivals", LOG_MAILBOX_SYNC);
    Q_UNUSED(syncState);

    // So, we know that messages only got added to the mailbox and that none were removed,
    // neither those that we already know or those that got added while we weren't around.
    // Therefore we ask only for UIDs of new messages

    list->m_numberFetchingStatus = TreeItem::LOADING;
    uidSyncingMode = UID_SYNC_ONLY_NEW;
    syncUids(mailbox, oldState.uidNext());
}

void ObtainSynchronizedMailboxTask::syncGeneric(TreeItemMailbox *mailbox, TreeItemMsgList *list, const SyncState &syncState)
{
    log("generic synchronization from previous state", LOG_MAILBOX_SYNC);
    Q_UNUSED(syncState);

    list->m_numberFetchingStatus = TreeItem::LOADING;
    list->m_unreadMessageCount = 0;
    uidSyncingMode = UID_SYNC_ALL;
    syncUids(mailbox);
}

void ObtainSynchronizedMailboxTask::syncUids(TreeItemMailbox *mailbox, const uint lowestUidToQuery)
{
    status = STATE_SYNCING_UIDS;
    log("Syncing UIDs", LOG_MAILBOX_SYNC);
    QString uidSpecification;
    if (lowestUidToQuery == 0) {
        uidSpecification = QLatin1String("ALL");
    } else {
        uidSpecification = QString::fromAscii("UID %1:*").arg(lowestUidToQuery);
    }
    uidSyncingCmd = parser->uidSearchUid(uidSpecification);
    model->cache()->clearUidMapping(mailbox->mailbox());
    emit model->mailboxSyncingProgress(mailboxIndex, status);
}

void ObtainSynchronizedMailboxTask::syncFlags(TreeItemMailbox *mailbox)
{
    status = STATE_SYNCING_FLAGS;
    log("Syncing flags", LOG_MAILBOX_SYNC);
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(mailbox->m_children[ 0 ]);
    Q_ASSERT(list);

    flagsCmd = parser->fetch(Sequence(1, mailbox->syncState.exists()), QStringList() << QLatin1String("FLAGS"));
    list->m_numberFetchingStatus = TreeItem::LOADING;
    emit model->mailboxSyncingProgress(mailboxIndex, status);
}

bool ObtainSynchronizedMailboxTask::handleResponseCodeInsideState(const Imap::Responses::State *const resp)
{
    if (dieIfInvalidMailbox())
        return true;

    TreeItemMailbox *mailbox = Model::mailboxForSomeItem(mailboxIndex);
    Q_ASSERT(mailbox);
    bool res = false;
    switch (resp->respCode) {
    case Responses::UNSEEN:
    {
        const Responses::RespData<uint> *const num = dynamic_cast<const Responses::RespData<uint>* const>(resp->respCodeData.data());
        if (num) {
            mailbox->syncState.setUnSeenOffset(num->data);
            res = true;
        } else {
            throw CantHappen("State response has invalid UNSEEN respCodeData", *resp);
        }
        break;
    }
    case Responses::PERMANENTFLAGS:
    {
        const Responses::RespData<QStringList> *const num = dynamic_cast<const Responses::RespData<QStringList>* const>(resp->respCodeData.data());
        if (num) {
            mailbox->syncState.setPermanentFlags(num->data);
            res = true;
        } else {
            throw CantHappen("State response has invalid PERMANENTFLAGS respCodeData", *resp);
        }
        break;
    }
    case Responses::UIDNEXT:
    {
        const Responses::RespData<uint> *const num = dynamic_cast<const Responses::RespData<uint>* const>(resp->respCodeData.data());
        if (num) {
            mailbox->syncState.setUidNext(num->data);
            res = true;
        } else {
            throw CantHappen("State response has invalid UIDNEXT respCodeData", *resp);
        }
        break;
    }
    case Responses::UIDVALIDITY:
    {
        const Responses::RespData<uint> *const num = dynamic_cast<const Responses::RespData<uint>* const>(resp->respCodeData.data());
        if (num) {
            mailbox->syncState.setUidValidity(num->data);
            res = true;
        } else {
            throw CantHappen("State response has invalid UIDVALIDITY respCodeData", *resp);
        }
        break;
    }
    case Responses::CLOSED:
    case Responses::HIGHESTMODSEQ:
        // FIXME: handle when supporting the qresync
        res = true;
        break;
    default:
        break;
    }
    return res;
}

bool ObtainSynchronizedMailboxTask::handleNumberResponse(const Imap::Responses::NumberResponse *const resp)
{
    if (dieIfInvalidMailbox())
        return true;

    TreeItemMailbox *mailbox = Model::mailboxForSomeItem(mailboxIndex);
    Q_ASSERT(mailbox);
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(mailbox->m_children[0]);
    Q_ASSERT(list);
    switch (resp->kind) {
    case Imap::Responses::EXISTS:
        switch (status) {
        case STATE_WAIT_FOR_CONN:
            Q_ASSERT(false);
            return false;

        case STATE_SELECTING:
        case STATE_SYNCING_UIDS:
            mailbox->syncState.setExists(resp->number);
            return true;

        case STATE_SYNCING_FLAGS:
        case STATE_DONE:
            if (resp->number == static_cast<uint>(list->m_children.size())) {
                // no changes
                return true;
            }
            mailbox->handleExists(model, *resp);
            Q_ASSERT(list->m_children.size());
            uint highestKnownUid = 0;
            for (int i = list->m_children.size() - 1; ! highestKnownUid && i >= 0; --i) {
                highestKnownUid = static_cast<const TreeItemMessage *>(list->m_children[i])->uid();
            }
            CommandHandle fetchCmd = parser->uidFetch(Sequence::startingAt(
                                                    // Did the UID walk return a usable number?
                                                    highestKnownUid ?
                                                    // Yes, we've got at least one message with a UID known -> ask for higher
                                                    // but don't forget to compensate for an pre-existing UIDNEXT value
                                                    qMax(mailbox->syncState.uidNext(), highestKnownUid + 1)
                                                    :
                                                    // No messages, or no messages with valid UID -> use the UIDNEXT from the syncing state
                                                    // but prevent a possible invalid 0:*
                                                    qMax(mailbox->syncState.uidNext(), 1u)
                                                ), QStringList() << QLatin1String("FLAGS"));
            newArrivalsFetch.append(fetchCmd);
            return true;
        }
        Q_ASSERT(false);
        return false;

    case Imap::Responses::EXPUNGE:
        // must be handled elsewhere
        break;
    case Imap::Responses::RECENT:
        mailbox->syncState.setRecent(resp->number);
        list->m_recentMessageCount = resp->number;
        return true;
        break;
    default:
        throw CantHappen("Got a NumberResponse of invalid kind. This is supposed to be handled in its constructor!", *resp);
    }
    return false;
}

bool ObtainSynchronizedMailboxTask::handleFlags(const Imap::Responses::Flags *const resp)
{
    if (dieIfInvalidMailbox())
        return true;

    TreeItemMailbox *mailbox = Model::mailboxForSomeItem(mailboxIndex);
    Q_ASSERT(mailbox);
    mailbox->syncState.setFlags(resp->flags);
    return true;
}

bool ObtainSynchronizedMailboxTask::handleSearch(const Imap::Responses::Search *const resp)
{
    if (dieIfInvalidMailbox())
        return true;

    TreeItemMailbox *mailbox = Model::mailboxForSomeItem(mailboxIndex);
    Q_ASSERT(mailbox);

    switch (uidSyncingMode) {
    case UID_SYNC_ALL:
        if (static_cast<uint>(resp->items.size()) != mailbox->syncState.exists()) {
            // The (possibly updated) EXISTS does not match what we received for UID SEARCH ALL. Please note that
            // it's the server's responsibility to feed us with valid data; scenarios like sending out-of-order responses
            // would clearly break this contract.
            std::ostringstream ss;
            ss << "Error when synchronizing all messages: server said that there are " << mailbox->syncState.exists() <<
                  "messages, but UID SEARCH ALL response contains " << resp->items.size() << "entries" << std::endl;
            ss.flush();
            throw MailboxException(ss.str().c_str(), *resp);
        }
        Q_ASSERT(mailbox->syncState.isUsableForSyncing());
        break;
    case UID_SYNC_ONLY_NEW:
    {
        // Be sure there really are some new messages
        const SyncState &oldState = model->cache()->mailboxSyncState(mailbox->mailbox());
        const int newArrivals = mailbox->syncState.exists() - oldState.exists();
        Q_ASSERT(newArrivals > 0);

        if (newArrivals != resp->items.size()) {
            std::ostringstream ss;
            ss << "Error when synchronizing new messages: server said that there are " << mailbox->syncState.exists() <<
                  "messages in total (" << newArrivals << " new), but UID SEARCH response contains " << resp->items.size() <<
                  "entries" << std::endl;
            ss.flush();
            throw MailboxException(ss.str().c_str(), *resp);
        }
        break;
    }
    }
    uidMap = resp->items;
    qSort(uidMap);
    if (!uidMap.isEmpty() && mailbox->syncState.uidNext() <= uidMap.last()) {
        mailbox->syncState.setUidNext(uidMap.last() + 1);
    }
    return true;
}

bool ObtainSynchronizedMailboxTask::handleFetch(const Imap::Responses::Fetch *const resp)
{
    if (dieIfInvalidMailbox())
        return true;

    TreeItemMailbox *mailbox = Model::mailboxForSomeItem(mailboxIndex);
    Q_ASSERT(mailbox);
    QList<TreeItemPart *> changedParts;
    TreeItemMessage *changedMessage = 0;
    mailbox->handleFetchResponse(model, *resp, changedParts, changedMessage);
    if (changedMessage) {
        QModelIndex index = changedMessage->toIndex(model);
        emit model->dataChanged(index, index);
        // On the other hand, this one will be emitted at the very end
        // model->emitMessageCountChanged(mailbox);
    }
    if (!changedParts.isEmpty()) {
        qDebug() << "Weird, FETCH when syncing has changed some body parts. We aren't ready for that.";
        log(QString::fromAscii("This response has changed some message parts. That should not have happened, as we're still syncing."));
    }
    return true;
}

/** @short Generic handler for UID syncing

The responsibility of this function is to process the "UID replies" triggered
by previous phases of the syncing activity and use them in order to assign all
messages their UIDs.

This function assumes that we're doing a so-called "full UID sync", ie. we were
forced to ask the server for a full list of all UIDs in a mailbox. This is the
safest, but also the most bandwidth-hungry way to achive our goal.
*/
void ObtainSynchronizedMailboxTask::finalizeUidSyncAll(TreeItemMailbox *mailbox)
{
    log("Processing the UID SEARCH response", LOG_MAILBOX_SYNC);
    // Verify that we indeed received UIDs for all messages
    if (static_cast<uint>(uidMap.size()) != mailbox->syncState.exists()) {
        // FIXME: this needs to be checked for what happens when the number of messages changes between SELECT and UID SEARCH ALL
        throw MailboxException("We received a weird number of UIDs for messages in the mailbox.");
    }
    applyUids(mailbox, 0);
}

/** @short Apply the received UID map to the messages in mailbox

The @arg firstUnknownUidOffset corresponds to the offset of a message whose UID is specified by the first item in the UID map.
*/
void ObtainSynchronizedMailboxTask::applyUids(TreeItemMailbox *mailbox, const uint firstUnknownUidOffset)
{
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(mailbox->m_children[0]);
    Q_ASSERT(list);
    QModelIndex parent = list->toIndex(model);

    int i = firstUnknownUidOffset;
    while (i < uidMap.size()) {
        // For each UID which is really supposed to be there...
        if (i >= list->m_children.size()) {
            // now we're just adding new messages to the end of the list
            model->beginInsertRows(parent, i, i - list->m_children.size());
            for (/*nothing*/; i < list->m_children.size(); ++i) {
                // Add all messages in one go
                TreeItemMessage *msg = new TreeItemMessage(list);
                msg->m_offset = i;
                msg->m_uid = uidMap[i];
                list->m_children << msg;
            }
            model->endInsertRows();
            Q_ASSERT(i == uidMap.size());
            Q_ASSERT(uidMap.size() == list->m_children.size());
        } else if (dynamic_cast<TreeItemMessage *>(list->m_children[i])->m_uid == uidMap[i]) {
            // If the UID of the "current message" matches, we're okay
            dynamic_cast<TreeItemMessage *>(list->m_children[i])->m_offset = i;
            ++i;
        } else if (dynamic_cast<TreeItemMessage *>(list->m_children[i])->m_uid == 0) {
            // If the UID of the "current message" is zero, replace that with this message
            TreeItemMessage *msg = static_cast<TreeItemMessage*>(list->m_children[i]);
            msg->m_uid = uidMap[i];
            msg->m_offset = i;
            QModelIndex idx = model->createIndex(i, 0, msg);
            emit model->dataChanged(idx, idx);
            if (msg->m_fetchStatus == TreeItem::LOADING) {
                // We've got to ask for the message metadata once again; the first attempt happened when the UID was still zero,
                // so this is our chance
                model->askForMsgMetadata(msg);
            }
            ++i;
        } else {
            // We've got an UID mismatch
            int pos = i;
            while (pos < list->m_children.size()) {
                // Remove any messages which have non-zero UID which is at the same time different than the UID we want to add
                // The key idea here is that IMAP guarantees that each and every new message will have greater UID than any
                // other message already in the mailbox. Just for the sake of completeness, should an evil server send us a
                // malformed response, we wouldn't care (or notice at this point), we'd just "needlessly" delete many "innocent"
                // messages due to that one out-of-place arrival -- but we'd still remain correct and not crash.
                TreeItemMessage *otherMessage = dynamic_cast<TreeItemMessage*>(list->m_children[pos]);
                Q_ASSERT(otherMessage);
                if (otherMessage->m_uid != 0 && otherMessage->m_uid != uidMap[i]) {
                    model->cache()->clearMessage(mailbox->mailbox(), otherMessage->uid());
                    ++pos;
                } else {
                    break;
                }
            }
            Q_ASSERT(pos > i);
            model->beginRemoveRows(parent, i, pos - 1);
            QList<TreeItem*> removedItems;
            for (int j = i; j < pos; ++j)
                removedItems << list->m_children.takeAt(i);
            model->endRemoveRows();
            // the m_offset of all subsequent messages will be updated later, at the time *they* are processed
            qDeleteAll(removedItems);
            if (i == list->m_children.size()) {
                // We're asked to add messages to the end of the list. That's something that's already implemented above,
                // so let's reuse that code. That's why we do *not* want to increment the counter here.
            } else {
                Q_ASSERT(i < list->m_children.size());
                Q_ASSERT(dynamic_cast<TreeItemMessage*>(list->m_children[i])->m_uid == 0);
                // But this case is also already implemented above, so we won't touch the counter from here, either,
                // and let the existing code do its job
            }
        }
    }

    if (i != list->m_children.size()) {
        // remove items at the end
        model->beginRemoveRows(parent, i, list->m_children.size() - 1);
        QList<TreeItem*> oldItems;
        for (/* nothing */; i < list->m_children.size(); ++i) {
            TreeItemMessage *message = static_cast<TreeItemMessage *>(list->m_children.takeAt(i));
            model->cache()->clearMessage(mailbox->mailbox(), message->uid());
            oldItems << message;
        }
        model->endRemoveRows();
        qDeleteAll(oldItems);
    }

    uidMap.clear();

    list->m_totalMessageCount = list->m_children.size();
    list->m_fetchStatus = TreeItem::DONE;

    // Store stuff we already have in the cache
    model->cache()->setMailboxSyncState(mailbox->mailbox(), mailbox->syncState);
    model->saveUidMap(list);

    model->emitMessageCountChanged(mailbox);
    model->changeConnectionState(parser, CONN_STATE_SELECTED);
}

/** @short Process delivered UIDs for new messages

This function is similar to _finalizeUidSyncAll, except that it operates on
a smaller set of messages.

It is also inteded to be used from both ObtainSynchronizedMailboxTask and KeepMailboxOpenTask,
so it's a static function with many arguments.
*/
void ObtainSynchronizedMailboxTask::finalizeUidSyncOnlyNew(Model *model, TreeItemMailbox *mailbox, const uint oldExists, QList<uint> &uidMap)
{
    log("Processing the UID response just for new arrivals", LOG_MAILBOX_SYNC);
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(mailbox->m_children[0]);
    Q_ASSERT(list);
    list->m_fetchStatus = TreeItem::DONE;

    // FIXME: verify these "invariants" -- they are rather prone to races during the UID syncing...
    // This invariant is set up in Model::_askForMessagesInMailbox
    Q_ASSERT(oldExists == static_cast<uint>(list->m_children.size()));

    // Be sure there really are some new messages -- verified in handleSearch()
    Q_ASSERT(mailbox->syncState.exists() > oldExists);
    Q_ASSERT(static_cast<uint>(uidMap.size()) == mailbox->syncState.exists() - oldExists);

    QModelIndex parent = list->toIndex(model);
    int offset = list->m_children.size();
    model->beginInsertRows(parent, offset, mailbox->syncState.exists() - 1);
    for (int i = 0; i < uidMap.size(); ++i) {
        TreeItemMessage *msg = new TreeItemMessage(list);
        msg->m_offset = i + offset;
        msg->m_uid = uidMap[ i ];
        list->m_children << msg;
    }
    model->endInsertRows();
    uidMap.clear();

    list->m_totalMessageCount = list->m_children.size();
    list->m_numberFetchingStatus = TreeItem::DONE; // FIXME: they aren't done yet
    model->cache()->setMailboxSyncState(mailbox->mailbox(), mailbox->syncState);
    model->saveUidMap(list);
    uidMap.clear();
    model->emitMessageCountChanged(mailbox);
}

QString ObtainSynchronizedMailboxTask::debugIdentification() const
{
    if (! mailboxIndex.isValid())
        return QString::fromAscii("[invalid mailboxIndex]");

    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);

    QString statusStr;
    switch (status) {
    case STATE_WAIT_FOR_CONN:
        statusStr = "STATE_WAIT_FOR_CONN";
        break;
    case STATE_SELECTING:
        statusStr = "STATE_SELECTING";
        break;
    case STATE_SYNCING_UIDS:
        statusStr = "STATE_SYNCING_UIDS";
        break;
    case STATE_SYNCING_FLAGS:
        statusStr = "STATE_SYNCING_FLAGS";
        break;
    case STATE_DONE:
        statusStr = "STATE_DONE";
        break;
    }
    return QString::fromAscii("%1 %2").arg(statusStr, mailbox->mailbox());
}

void ObtainSynchronizedMailboxTask::notifyInterestingMessages(TreeItemMailbox *mailbox)
{
    Q_ASSERT(mailbox);
    TreeItemMsgList *list = dynamic_cast<Imap::Mailbox::TreeItemMsgList *>(mailbox->m_children[0]);
    Q_ASSERT(list);
    list->recalcVariousMessageCounts(model);
    QModelIndex listIndex = list->toIndex(model);
    Q_ASSERT(listIndex.isValid());
    QModelIndex firstInterestingMessage = model->index(mailbox->syncState.unSeenOffset(), 0, listIndex);
    log("First interesting message at " + QString::number(mailbox->syncState.unSeenOffset()), LOG_MAILBOX_SYNC);
    emit model->mailboxFirstUnseenMessage(mailbox->toIndex(model), firstInterestingMessage);
}

bool ObtainSynchronizedMailboxTask::dieIfInvalidMailbox()
{
    Q_ASSERT(!unSelectTask);

    if (mailboxIndex.isValid())
        return false;

    // OK, so we are in trouble -- our mailbox has disappeared, but the IMAP server will likely keep us busy with its
    // status updates. This is bad, so we have to get out as fast as possible. All hands, evasive maneuvers!

    log("Mailbox disappeared", LOG_MAILBOX_SYNC);

    unSelectTask = model->m_taskFactory->createUnSelectTask(model, this);
    connect(unSelectTask, SIGNAL(completed(ImapTask *)), this, SLOT(slotUnSelectCompleted()));
    unSelectTask->perform();

    return true;
}

void ObtainSynchronizedMailboxTask::slotUnSelectCompleted()
{
    // Now, just finish and signal a failure
    _failed("Escaped from mailbox");
}

QVariant ObtainSynchronizedMailboxTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Synchronizing mailbox")) : QVariant();
}

}
}
