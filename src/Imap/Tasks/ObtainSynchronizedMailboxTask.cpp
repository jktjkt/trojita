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

#include "ObtainSynchronizedMailboxTask.h"
#include <algorithm>
#include <sstream>
#include <QTimer>
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "KeepMailboxOpenTask.h"
#include "UnSelectTask.h"

namespace Imap
{
namespace Mailbox
{

ObtainSynchronizedMailboxTask::ObtainSynchronizedMailboxTask(Model *model, const QModelIndex &mailboxIndex, ImapTask *parentTask,
        KeepMailboxOpenTask *keepTask):
    ImapTask(model), conn(parentTask), mailboxIndex(mailboxIndex), status(STATE_WAIT_FOR_CONN), uidSyncingMode(UID_SYNC_ALL),
    firstUnknownUidOffset(0), m_usingQresync(false), unSelectTask(0), keepTaskChild(keepTask)
{
    // The Parser* is not provided by our parent task, but instead through the keepTaskChild.  The reason is simple, the parent
    // task might not even exist, but there's always an KeepMailboxOpenTask in the game.
    parser = keepTaskChild->parser;
    Q_ASSERT(parser);
    if (conn) {
        conn->addDependentTask(this);
    }
    CHECK_TASK_TREE
    addDependentTask(keepTaskChild);
    CHECK_TASK_TREE
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
    CHECK_TASK_TREE
    markAsActiveTask();

    if (_dead || _aborted) {
        // We're at the very start, so let's try to abort in a sane way
        _failed("Asked to abort or die");
        die();
        return;
    }

    if (! mailboxIndex.isValid()) {
        // FIXME: proper error handling
        log("The mailbox went missing, sorry", Common::LOG_MAILBOX_SYNC);
        _completed();
        return;
    }

    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);
    TreeItemMsgList *msgList = dynamic_cast<TreeItemMsgList *>(mailbox->m_children[0]);
    Q_ASSERT(msgList);

    msgList->setFetchStatus(TreeItem::LOADING);

    Q_ASSERT(model->m_parsers.contains(parser));

    oldSyncState = model->cache()->mailboxSyncState(mailbox->mailbox());
    if (model->accessParser(parser).capabilities.contains(QLatin1String("QRESYNC")) && oldSyncState.isUsableForCondstore()) {
        m_usingQresync = true;
        QList<uint> oldUidMap = model->cache()->uidMapping(mailbox->mailbox());
        if (oldUidMap.isEmpty()) {
            selectCmd = parser->selectQresync(mailbox->mailbox(), oldSyncState.uidValidity(),
                                              oldSyncState.highestModSeq());
        } else {
            Sequence knownSeq, knownUid;
            int i = oldUidMap.size() / 2;
            while (i < oldUidMap.size()) {
                // Message sequence number is one-based, our indexes are zero-based
                knownSeq.add(i + 1);
                knownUid.add(oldUidMap[i]);
                i += (oldUidMap.size() - i) / 2 + 1;
            }
            // We absolutely want to maintain a complete UID->seq mapping at all times, which is why the known-uids shall remain
            // empty to indicate "anything".
            selectCmd = parser->selectQresync(mailbox->mailbox(), oldSyncState.uidValidity(),
                                              oldSyncState.highestModSeq(), Sequence(), knownSeq, knownUid);
        }
    } else if (model->accessParser(parser).capabilities.contains(QLatin1String("CONDSTORE"))) {
        selectCmd = parser->select(mailbox->mailbox(), QList<QByteArray>() << "CONDSTORE");
    } else {
        selectCmd = parser->select(mailbox->mailbox());
    }
    mailbox->syncState = SyncState();
    status = STATE_SELECTING;
    log("Synchronizing mailbox", Common::LOG_MAILBOX_SYNC);
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
            // FIXME: move the finalizeSearch() here to support working with split SEARCH reposnes -- but beware of
            // arrivals/expunges which happen while the UID SEARCH is in progres...
            log("UIDs synchronized", Common::LOG_MAILBOX_SYNC);
            Q_ASSERT(status == STATE_SYNCING_FLAGS);
            Q_ASSERT(mailboxIndex.isValid());   // FIXME
            TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
            Q_ASSERT(mailbox);
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
            log("Flags synchronized", Common::LOG_MAILBOX_SYNC);
            notifyInterestingMessages(mailbox);
            flagsCmd.clear();

            if (newArrivalsFetch.isEmpty()) {
                mailbox->saveSyncStateAndUids(model);
                model->changeConnectionState(parser, CONN_STATE_SELECTED);
                _completed();
            } else {
                log("Pending new arrival fetching, not terminating yet", Common::LOG_MAILBOX_SYNC);
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
                Q_ASSERT(mailboxIndex.isValid());   // FIXME
                TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
                Q_ASSERT(mailbox);
                mailbox->saveSyncStateAndUids(model);
                model->changeConnectionState(parser, CONN_STATE_SELECTED);
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
    oldSyncState = model->cache()->mailboxSyncState(mailbox->mailbox());
    list->m_totalMessageCount = syncState.exists();
    // Note: syncState.unSeen() is the NUMBER of the first unseen message, not their count!

    uidMap = model->cache()->uidMapping(mailbox->mailbox());

    if (static_cast<uint>(uidMap.size()) != oldSyncState.exists()) {

        QString buf;
        QDebug dbg(&buf);
        dbg << "Inconsistent cache data, falling back to full sync (" << uidMap.size() << "in UID map," << oldSyncState.exists() <<
            "EXIST before)";
        log(buf, Common::LOG_MAILBOX_SYNC);
        oldSyncState.setHighestModSeq(0);
        fullMboxSync(mailbox, list);
    } else {
        if (syncState.isUsableForSyncing() && oldSyncState.isUsableForSyncing() && syncState.uidValidity() == oldSyncState.uidValidity()) {
            // Perform a nice re-sync

            // Check the QRESYNC support and availability
            if (m_usingQresync && oldSyncState.isUsableForCondstore() && syncState.isUsableForCondstore()) {
                // Looks like we can use QRESYNC for fast syncing
                if (oldSyncState.highestModSeq() > syncState.highestModSeq()) {
                    // Looks like a corrupted cache or a server's bug
                    log("Yuck, recycled HIGHESTMODSEQ when trying to use QRESYNC", Common::LOG_MAILBOX_SYNC);
                    mailbox->syncState.setHighestModSeq(0);
                    model->cache()->clearAllMessages(mailbox->mailbox());
                    m_usingQresync = false;
                    fullMboxSync(mailbox, list);
                } else {
                    if (oldSyncState.highestModSeq() == syncState.highestModSeq()) {
                        if (oldSyncState.exists() != syncState.exists()) {
                            log("Sync error: QRESYNC says no changes but EXISTS has changed", Common::LOG_MAILBOX_SYNC);
                            mailbox->syncState.setHighestModSeq(0);
                            model->cache()->clearAllMessages(mailbox->mailbox());
                            m_usingQresync = false;
                            fullMboxSync(mailbox, list);
                        } else if (oldSyncState.uidNext() != syncState.uidNext()) {
                            log("Sync error: QRESYNC says no changes but UIDNEXT has changed", Common::LOG_MAILBOX_SYNC);
                            mailbox->syncState.setHighestModSeq(0);
                            model->cache()->clearAllMessages(mailbox->mailbox());
                            m_usingQresync = false;
                            fullMboxSync(mailbox, list);
                        } else if (syncState.exists() != static_cast<uint>(list->m_children.size())) {
                            log(QString::fromUtf8("Sync error: constant HIGHESTMODSEQ, EXISTS says %1 messages but in fact "
                                                   "there are %2 when finalizing SELECT")
                                .arg(QString::number(mailbox->syncState.exists()), QString::number(list->m_children.size())),
                                Common::LOG_MAILBOX_SYNC);
                            mailbox->syncState.setHighestModSeq(0);
                            model->cache()->clearAllMessages(mailbox->mailbox());
                            m_usingQresync = false;
                            fullMboxSync(mailbox, list);
                        } else {
                            // This should be enough
                            list->setFetchStatus(TreeItem::DONE);
                            notifyInterestingMessages(mailbox);
                            mailbox->saveSyncStateAndUids(model);
                            model->changeConnectionState(parser, CONN_STATE_SELECTED);
                            _completed();
                        }
                        return;
                    }

                    if (static_cast<uint>(list->m_children.size()) != mailbox->syncState.exists()) {
                        log(QString::fromUtf8("Sync error: EXISTS says %1 messages, msgList has %2")
                            .arg(QString::number(mailbox->syncState.exists()), QString::number(list->m_children.size())));
                        mailbox->syncState.setHighestModSeq(0);
                        model->cache()->clearAllMessages(mailbox->mailbox());
                        m_usingQresync = false;
                        fullMboxSync(mailbox, list);
                        return;
                    }


                    if (oldSyncState.uidNext() < syncState.uidNext()) {
                        list->setFetchStatus(TreeItem::DONE);
                        int seqWithLowestUnknownUid = -1;
                        for (int i = 0; i < list->m_children.size(); ++i) {
                            TreeItemMessage *msg = static_cast<TreeItemMessage*>(list->m_children[i]);
                            if (!msg->uid()) {
                                seqWithLowestUnknownUid = i;
                                break;
                            }
                        }
                        if (seqWithLowestUnknownUid >= 0) {
                            // We've got some new arrivals, but unfortunately QRESYNC won't report them just yet :(
                            CommandHandle fetchCmd = parser->uidFetch(Sequence::startingAt(qMax(oldSyncState.uidNext(), 1u)),
                                                                      QStringList() << QLatin1String("FLAGS"));
                            newArrivalsFetch.append(fetchCmd);
                            status = STATE_DONE;
                        } else {
                            // All UIDs are known at this point, including the new arrivals, yay
                            notifyInterestingMessages(mailbox);
                            mailbox->saveSyncStateAndUids(model);
                            model->changeConnectionState(parser, CONN_STATE_SELECTED);
                            _completed();
                        }
                    } else {
                        // This should be enough, the server should've sent the data already
                        list->setFetchStatus(TreeItem::DONE);
                        notifyInterestingMessages(mailbox);
                        mailbox->saveSyncStateAndUids(model);
                        model->changeConnectionState(parser, CONN_STATE_SELECTED);
                        _completed();
                    }
                }
                return;
            }

            if (syncState.exists() == 0) {
                // This is a special case, the mailbox doesn't contain any messages now.
                // Let's just save ourselves some work and reuse the "smart" code in the fullMboxSync() here, it will
                // do the right thing.
                fullMboxSync(mailbox, list);
                return;
            }

            if (syncState.uidNext() == oldSyncState.uidNext()) {
                // No new messages

                if (syncState.exists() == oldSyncState.exists()) {
                    // No deletions, either, so we resync only flag changes
                    syncNoNewNoDeletions(mailbox, list);
                } else {
                    // Some messages got deleted, but there have been no additions
                    syncGeneric(mailbox, list);
                }

            } else if (syncState.uidNext() > oldSyncState.uidNext()) {
                // Some new messages were delivered since we checked the last time.
                // There's no guarantee they are still present, though.

                if (syncState.uidNext() - oldSyncState.uidNext() == syncState.exists() - oldSyncState.exists()) {
                    // Only some new arrivals, no deletions
                    syncOnlyAdditions(mailbox, list);
                } else {
                    // Generic case; we don't know anything about which messages were deleted and which added
                    syncGeneric(mailbox, list);
                }
            } else {
                // The UIDNEXT has decreased while UIDVALIDITY remains the same. This is forbidden,
                // so either a server's bug, or a completely invalid cache.
                Q_ASSERT(syncState.uidNext() < oldSyncState.uidNext());
                Q_ASSERT(syncState.uidValidity() == oldSyncState.uidValidity());
                log("Yuck, UIDVALIDITY remains same but UIDNEXT decreased", Common::LOG_MAILBOX_SYNC);
                model->cache()->clearAllMessages(mailbox->mailbox());
                fullMboxSync(mailbox, list);
            }
        } else {
            // Forget everything, do a dumb sync
            model->cache()->clearAllMessages(mailbox->mailbox());
            fullMboxSync(mailbox, list);
        }
    }
}

void ObtainSynchronizedMailboxTask::fullMboxSync(TreeItemMailbox *mailbox, TreeItemMsgList *list)
{
    log("Full synchronization", Common::LOG_MAILBOX_SYNC);

    QModelIndex parent = list->toIndex(model);
    if (! list->m_children.isEmpty()) {
        model->beginRemoveRows(parent, 0, list->m_children.size() - 1);
        auto oldItems = list->m_children;
        list->m_children.clear();
        model->endRemoveRows();
        qDeleteAll(oldItems);
    }
    if (mailbox->syncState.exists()) {
        list->m_children.reserve(mailbox->syncState.exists());
        model->beginInsertRows(parent, 0, mailbox->syncState.exists() - 1);
        for (uint i = 0; i < mailbox->syncState.exists(); ++i) {
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
        list->setFetchStatus(TreeItem::DONE);

        // The remote mailbox is empty -> we're done now
        model->changeConnectionState(parser, CONN_STATE_SELECTED);
        status = STATE_DONE;
        emit model->mailboxSyncingProgress(mailboxIndex, status);
        notifyInterestingMessages(mailbox);
        mailbox->saveSyncStateAndUids(model);
        model->changeConnectionState(parser, CONN_STATE_SELECTED);
        // Take care here: this call could invalidate our index (see test coverage)
        _completed();
    }
    // Our mailbox might have actually been invalidated by various callbacks activated above
    if (mailboxIndex.isValid()) {
        Q_ASSERT(mailboxIndex.internalPointer() == mailbox);
        model->emitMessageCountChanged(mailbox);
    }
}

void ObtainSynchronizedMailboxTask::syncNoNewNoDeletions(TreeItemMailbox *mailbox, TreeItemMsgList *list)
{
    Q_ASSERT(mailbox->syncState.exists() == static_cast<uint>(uidMap.size()));
    log("No arrivals or deletions since the last time", Common::LOG_MAILBOX_SYNC);
    if (mailbox->syncState.exists()) {
        // Verify that we indeed have all UIDs and not need them anymore
#ifndef QT_NO_DEBUG
        for (int i = 0; i < list->m_children.size(); ++i) {
            // FIXME: This assert can fail if the mailbox contained messages with missing UIDs even before we opened it now.
            Q_ASSERT(static_cast<TreeItemMessage *>(list->m_children[i])->uid());
        }
#endif
    } else {
        list->m_unreadMessageCount = 0;
        list->m_totalMessageCount = 0;
        list->m_numberFetchingStatus = TreeItem::DONE;
    }

    if (list->m_children.isEmpty()) {
        TreeItemChildrenList messages;
        list->m_children.reserve(mailbox->syncState.exists());
        for (uint i = 0; i < mailbox->syncState.exists(); ++i) {
            TreeItemMessage *msg = new TreeItemMessage(list);
            msg->m_offset = i;
            msg->m_uid = uidMap[ i ];
            messages << msg;
        }
        list->setChildren(messages);

    } else {
        if (mailbox->syncState.exists() != static_cast<uint>(list->m_children.size())) {
            throw CantHappen("TreeItemMsgList has wrong number of "
                             "children, even though no change of "
                             "message count occurred");
        }
    }

    list->setFetchStatus(TreeItem::DONE);

    if (mailbox->syncState.exists()) {
        syncFlags(mailbox);
    } else {
        status = STATE_DONE;
        emit model->mailboxSyncingProgress(mailboxIndex, status);
        notifyInterestingMessages(mailbox);

        if (newArrivalsFetch.isEmpty()) {
            mailbox->saveSyncStateAndUids(model);
            model->changeConnectionState(parser, CONN_STATE_SELECTED);
            _completed();
        }
    }
}

void ObtainSynchronizedMailboxTask::syncOnlyAdditions(TreeItemMailbox *mailbox, TreeItemMsgList *list)
{
    log("Syncing new arrivals", Common::LOG_MAILBOX_SYNC);

    // So, we know that messages only got added to the mailbox and that none were removed,
    // neither those that we already know or those that got added while we weren't around.
    // Therefore we ask only for UIDs of new messages

    firstUnknownUidOffset = oldSyncState.exists();
    list->m_numberFetchingStatus = TreeItem::LOADING;
    uidSyncingMode = UID_SYNC_ONLY_NEW;
    syncUids(mailbox, oldSyncState.uidNext());
}

void ObtainSynchronizedMailboxTask::syncGeneric(TreeItemMailbox *mailbox, TreeItemMsgList *list)
{
    log("generic synchronization from previous state", Common::LOG_MAILBOX_SYNC);

    list->m_numberFetchingStatus = TreeItem::LOADING;
    list->m_unreadMessageCount = 0;
    uidSyncingMode = UID_SYNC_ALL;
    syncUids(mailbox);
}

void ObtainSynchronizedMailboxTask::syncUids(TreeItemMailbox *mailbox, const uint lowestUidToQuery)
{
    status = STATE_SYNCING_UIDS;
    log("Syncing UIDs", Common::LOG_MAILBOX_SYNC);
    QByteArray uidSpecification;
    if (lowestUidToQuery == 0) {
        uidSpecification = "ALL";
    } else {
        uidSpecification = QString::fromUtf8("UID %1:*").arg(QString::number(lowestUidToQuery)).toUtf8();
    }
    uidMap.clear();
    if (model->accessParser(parser).capabilities.contains(QLatin1String("ESEARCH"))) {
        uidSyncingCmd = parser->uidESearchUid(uidSpecification);
    } else {
        uidSyncingCmd = parser->uidSearchUid(uidSpecification);
    }
    emit model->mailboxSyncingProgress(mailboxIndex, status);
}

void ObtainSynchronizedMailboxTask::syncFlags(TreeItemMailbox *mailbox)
{
    status = STATE_SYNCING_FLAGS;
    log("Syncing flags", Common::LOG_MAILBOX_SYNC);
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(mailbox->m_children[ 0 ]);
    Q_ASSERT(list);

    // 0 => don't use it; >0 => use that as the old value
    quint64 useModSeq = 0;
    if ((model->accessParser(parser).capabilities.contains(QLatin1String("CONDSTORE")) ||
         model->accessParser(parser).capabilities.contains(QLatin1String("QRESYNC"))) &&
            oldSyncState.highestModSeq() > 0 && mailbox->syncState.isUsableForCondstore() &&
            oldSyncState.uidValidity() == mailbox->syncState.uidValidity()) {
        // The CONDSTORE is available, UIDVALIDITY has not changed and the HIGHESTMODSEQ suggests that
        // it will be useful
        if (oldSyncState.highestModSeq() == mailbox->syncState.highestModSeq()) {
            // Looks like there were no changes in flags -- that's cool, we're done here,
            // but only after some sanity checks
            if (oldSyncState.exists() > mailbox->syncState.exists()) {
                log("Some messages have arrived to the mailbox, but HIGHESTMODSEQ hasn't changed. "
                    "That's a bug in the server implementation.", Common::LOG_MAILBOX_SYNC);
                // will issue the ordinary FETCH command for FLAGS
            } else if (oldSyncState.uidNext() != mailbox->syncState.uidNext()) {
                log("UIDNEXT has changed, yet HIGHESTMODSEQ remained constant; that's server's bug", Common::LOG_MAILBOX_SYNC);
                // and again, don't trust that HIGHESTMODSEQ
            } else {
                // According to HIGHESTMODSEQ, there hasn't been any change. UIDNEXT and EXISTS do not contradict
                // this interpretation, so we can go and call stuff finished.
                if (newArrivalsFetch.isEmpty()) {
                    // No pending activity -> let's call it a day
                    status = STATE_DONE;
                    mailbox->saveSyncStateAndUids(model);
                    model->changeConnectionState(parser, CONN_STATE_SELECTED);
                    _completed();
                    return;
                } else {
                    // ...but there's still some pending activity; let's wait for its termination
                    status = STATE_DONE;
                }
            }
        } else if (oldSyncState.highestModSeq() > mailbox->syncState.highestModSeq()) {
            // Clearly a bug
            log("HIGHESTMODSEQ decreased, that's a bug in the IMAP server", Common::LOG_MAILBOX_SYNC);
            // won't use HIGHESTMODSEQ
        } else {
            // Will use FETCH CHANGEDSINCE
            useModSeq = oldSyncState.highestModSeq();
        }
    }
    if (useModSeq > 0) {
        QMap<QByteArray, quint64> fetchModifier;
        fetchModifier["CHANGEDSINCE"] = oldSyncState.highestModSeq();
        flagsCmd = parser->fetch(Sequence(1, mailbox->syncState.exists()), QStringList() << QLatin1String("FLAGS"), fetchModifier);
    } else {
        flagsCmd = parser->fetch(Sequence(1, mailbox->syncState.exists()), QStringList() << QLatin1String("FLAGS"));
    }
    list->m_numberFetchingStatus = TreeItem::LOADING;
    emit model->mailboxSyncingProgress(mailboxIndex, status);
}

bool ObtainSynchronizedMailboxTask::handleResponseCodeInsideState(const Imap::Responses::State *const resp)
{
    if (dieIfInvalidMailbox())
        return resp->tag.isEmpty();

    TreeItemMailbox *mailbox = Model::mailboxForSomeItem(mailboxIndex);
    Q_ASSERT(mailbox);
    switch (resp->respCode) {
    case Responses::UNSEEN:
    {
        const Responses::RespData<uint> *const num = dynamic_cast<const Responses::RespData<uint>* const>(resp->respCodeData.data());
        if (num) {
            mailbox->syncState.setUnSeenOffset(num->data);
            return resp->tag.isEmpty();
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
            return resp->tag.isEmpty();
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
            return resp->tag.isEmpty();
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
            return resp->tag.isEmpty();
        } else {
            throw CantHappen("State response has invalid UIDVALIDITY respCodeData", *resp);
        }
        break;
    }
    case Responses::NOMODSEQ:
        // NOMODSEQ means that this mailbox doesn't support CONDSTORE or QRESYNC. We have to avoid sending any fancy commands like
        // the FETCH CHANGEDSINCE etc.
        mailbox->syncState.setHighestModSeq(0);
        m_usingQresync = false;
        return resp->tag.isEmpty();
        break;

    case Responses::HIGHESTMODSEQ:
    {
        const Responses::RespData<quint64> *const num = dynamic_cast<const Responses::RespData<quint64>* const>(resp->respCodeData.data());
        Q_ASSERT(num);
        mailbox->syncState.setHighestModSeq(num->data);
        return resp->tag.isEmpty();
        break;
    }
    case Responses::CLOSED:
        // FIXME: handle when supporting the qresync
        return resp->tag.isEmpty();
        break;
    default:
        break;
    }
    return false;
}

void ObtainSynchronizedMailboxTask::updateHighestKnownUid(TreeItemMailbox *mailbox, const TreeItemMsgList *list) const
{
    uint highestKnownUid = 0;
    for (int i = list->m_children.size() - 1; ! highestKnownUid && i >= 0; --i) {
        highestKnownUid = static_cast<const TreeItemMessage *>(list->m_children[i])->uid();
    }
    if (highestKnownUid) {
        // If the UID walk return a usable number, remember that and use it for updating our idea of the UIDNEXT
        mailbox->syncState.setUidNext(qMax(mailbox->syncState.uidNext(), highestKnownUid + 1));
    }
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
            if (m_usingQresync) {
                // Because QRESYNC won't tell us anything about the new UIDs, we have to resort to this kludgy way of working.
                // I really, really wonder why there's no such thing like the ARRIVED to accompany VANISHED. Oh well.
                mailbox->syncState.setExists(resp->number);
                int newArrivals = resp->number - list->m_children.size();
                if (newArrivals > 0) {
                    // We have to add empty messages here
                    QModelIndex parent = list->toIndex(model);
                    int offset = list->m_children.size();
                    list->m_children.reserve(resp->number);
                    model->beginInsertRows(parent, offset, resp->number - 1);
                    for (int i = 0; i < newArrivals; ++i) {
                        TreeItemMessage *msg = new TreeItemMessage(list);
                        msg->m_offset = i + offset;
                        list->m_children << msg;
                        // yes, we really have to add this message with UID 0 :(
                    }
                    model->endInsertRows();
                    list->m_totalMessageCount = resp->number;
                }
            } else {
                // It's perfectly acceptable for the server to start its responses with EXISTS instead of UIDVALIDITY & UIDNEXT, so
                // we really cannot do anything besides remembering this value for later.
                mailbox->syncState.setExists(resp->number);
            }
            return true;

        case STATE_SYNCING_UIDS:
            mailbox->handleExists(model, *resp);
            updateHighestKnownUid(mailbox, list);
            return true;

        case STATE_SYNCING_FLAGS:
        case STATE_DONE:
            if (resp->number == static_cast<uint>(list->m_children.size())) {
                // no changes
                return true;
            }
            mailbox->handleExists(model, *resp);
            Q_ASSERT(list->m_children.size());
            updateHighestKnownUid(mailbox, list);
            CommandHandle fetchCmd = parser->uidFetch(Sequence::startingAt(
                                                    // prevent a possible invalid 0:*
                                                    qMax(mailbox->syncState.uidNext(), 1u)
                                                ), QStringList() << QLatin1String("FLAGS"));
            newArrivalsFetch.append(fetchCmd);
            return true;
        }
        Q_ASSERT(false);
        return false;

    case Imap::Responses::EXPUNGE:

        if (mailbox->syncState.exists() > 0) {
            // Always update the number of expected messages
            mailbox->syncState.setExists(mailbox->syncState.exists() - 1);
        }

        switch (status) {
        case STATE_SYNCING_FLAGS:
            // The UID mapping has been already established, but we don't have enough information for
            // an atomic state transition yet
            mailbox->handleExpunge(model, *resp);
            // The SyncState and the UID map will be saved later, along with the flags, when this task finishes
            return true;

        case STATE_DONE:
            // The UID mapping has been already established, so we just want to handle the EXPUNGE as usual
            mailbox->handleExpunge(model, *resp);
            mailbox->saveSyncStateAndUids(model);
            return true;

        default:
            // This is handled by the code below
            break;
        }

        // We shall track updates to the place where the unknown UIDs resign
        if (resp->number < firstUnknownUidOffset + 1) {
            // The message which we're deleting has UID which is already known, ie. it isn't among those whose UIDs got requested
            // by an incremental UID SEARCH
            Q_ASSERT(firstUnknownUidOffset > 0);
            --firstUnknownUidOffset;

            // The deleted message has previously been present; that means that we shall immediately signal about its expunge
            mailbox->handleExpunge(model, *resp);
        }

        switch(status) {
        case STATE_WAIT_FOR_CONN:
            Q_ASSERT(false);
            return false;

        case STATE_SELECTING:
            // The actual change will be handled by the UID syncing code
            return true;

        case STATE_SYNCING_UIDS:
            // We shouldn't delete stuff at this point, it will be handled by the UID syncing.
            // The response shall be consumed, though.
            return true;

        case STATE_SYNCING_FLAGS:
        case STATE_DONE:
            // Already handled above
            Q_ASSERT(false);
            return false;

        }

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

bool ObtainSynchronizedMailboxTask::handleVanished(const Imap::Responses::Vanished *const resp)
{
    if (dieIfInvalidMailbox())
        return true;

    TreeItemMailbox *mailbox = Model::mailboxForSomeItem(mailboxIndex);
    Q_ASSERT(mailbox);

    switch (status) {
    case STATE_WAIT_FOR_CONN:
        Q_ASSERT(false);
        return false;

    case STATE_SELECTING:
    case STATE_SYNCING_UIDS:
    case STATE_SYNCING_FLAGS:
    case STATE_DONE:
        mailbox->handleVanished(model, *resp);
        return true;
    }

    Q_ASSERT(false);
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

    if (uidSyncingCmd.isEmpty())
        return false;

    Q_ASSERT(Model::mailboxForSomeItem(mailboxIndex));

    uidMap += resp->items;

    finalizeSearch();
    return true;
}

bool ObtainSynchronizedMailboxTask::handleESearch(const Imap::Responses::ESearch *const resp)
{
    if (dieIfInvalidMailbox())
        return true;

    if (resp->tag.isEmpty() || resp->tag != uidSyncingCmd)
        return false;

    if (resp->seqOrUids != Imap::Responses::ESearch::UIDS)
        throw UnexpectedResponseReceived("ESEARCH response with matching tag uses sequence numbers instead of UIDs", *resp);

    // Yes, I just love templates.
    Responses::ESearch::CompareListDataIdentifier<Responses::ESearch::ListData_t> allComparator("ALL");
    Responses::ESearch::ListData_t::const_iterator listIterator =
            std::find_if(resp->listData.constBegin(), resp->listData.constEnd(), allComparator);

    if (listIterator != resp->listData.constEnd()) {
        uidMap = listIterator->second;
        ++listIterator;
        if (std::find_if(listIterator, resp->listData.constEnd(), allComparator) != resp->listData.constEnd())
            throw UnexpectedResponseReceived("ESEARCH contains the ALL key too many times", *resp);
    } else {
        // If the ALL key is not present, the server is telling us that there are no messages matching the query
        uidMap.clear();
    }

    finalizeSearch();
    return true;
}

bool ObtainSynchronizedMailboxTask::handleEnabled(const Responses::Enabled * const resp)
{
    if (dieIfInvalidMailbox())
        return false;

    // This function is needed to work around a bug in Kolab's version of Cyrus which sometimes sends out untagged ENABLED
    // during the SELECT processing. RFC 5161 is pretty clear in saying that ENABLED shall be sent only in response to
    // the ENABLE command; the log submitted at https://bugs.kde.org/show_bug.cgi?id=329204#c5 shows that Trojita receives
    // an extra * ENABLED CONDSTORE QRESYNC even after we have issued the x ENABLE QRESYNC previously and the server already
    // replied with * ENABLED QRESYNC to that.

    return resp->extensions.contains("CONDSTORE") || resp->extensions.contains("QRESYNC");
}

/** @short Process the result of UID SEARCH or UID ESEARCH commands */
void ObtainSynchronizedMailboxTask::finalizeSearch()
{
    TreeItemMailbox *mailbox = Model::mailboxForSomeItem(mailboxIndex);
    Q_ASSERT(mailbox);
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList*>(mailbox->m_children[0]);
    Q_ASSERT(list);

    switch (uidSyncingMode) {
    case UID_SYNC_ALL:
        if (static_cast<uint>(uidMap.size()) != mailbox->syncState.exists()) {
            // The (possibly updated) EXISTS does not match what we received for UID (E)SEARCH ALL. Please note that
            // it's the server's responsibility to feed us with valid data; scenarios like sending out-of-order responses
            // would clearly break this contract.
            std::ostringstream ss;
            ss << "Error when synchronizing all messages: server said that there are " << mailbox->syncState.exists() <<
                  " messages, but UID (E)SEARCH ALL response contains " << uidMap.size() << " entries" << std::endl;
            ss.flush();
            throw MailboxException(ss.str().c_str());
        }
        break;
    case UID_SYNC_ONLY_NEW:
    {
        // Be sure there really are some new messages
        const int newArrivals = mailbox->syncState.exists() - firstUnknownUidOffset;
        Q_ASSERT(newArrivals >= 0);

        if (newArrivals != uidMap.size()) {
            std::ostringstream ss;
            ss << "Error when synchronizing new messages: server said that there are " << mailbox->syncState.exists() <<
                  " messages in total (" << newArrivals << " new), but UID (E)SEARCH response contains " << uidMap.size() <<
                  " entries" << std::endl;
            ss.flush();
            throw MailboxException(ss.str().c_str());
        }
        break;
    }
    }

    qSort(uidMap);
    if (!uidMap.isEmpty() && uidMap.front() == 0) {
        throw MailboxException("UID (E)SEARCH response contains invalid UID zero");
    }
    applyUids(mailbox);
    uidMap.clear();
    updateHighestKnownUid(mailbox, list);
    status = STATE_SYNCING_FLAGS;
}

bool ObtainSynchronizedMailboxTask::handleFetch(const Imap::Responses::Fetch *const resp)
{
    if (dieIfInvalidMailbox())
        return true;

    TreeItemMailbox *mailbox = Model::mailboxForSomeItem(mailboxIndex);
    Q_ASSERT(mailbox);
    QList<TreeItemPart *> changedParts;
    TreeItemMessage *changedMessage = 0;
    mailbox->handleFetchResponse(model, *resp, changedParts, changedMessage, m_usingQresync);
    if (changedMessage) {
        QModelIndex index = changedMessage->toIndex(model);
        emit model->dataChanged(index, index);
        if (mailbox->syncState.uidNext() <= changedMessage->uid()) {
            mailbox->syncState.setUidNext(changedMessage->uid() + 1);
        }
        // On the other hand, this one will be emitted at the very end
        // model->emitMessageCountChanged(mailbox);
    }
    if (!changedParts.isEmpty() && !m_usingQresync) {
        // On the other hand, with QRESYNC our code is ready to receive extra data that changes body parts...
        qDebug() << "Weird, FETCH when syncing has changed some body parts. We aren't ready for that.";
        log(QLatin1String("This response has changed some message parts. That should not have happened, as we're still syncing."));
    }
    return true;
}

/** @short Apply the received UID map to the messages in mailbox

The @arg firstUnknownUidOffset corresponds to the offset of a message whose UID is specified by the first item in the UID map.
*/
void ObtainSynchronizedMailboxTask::applyUids(TreeItemMailbox *mailbox)
{
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(mailbox->m_children[0]);
    Q_ASSERT(list);
    QModelIndex parent = list->toIndex(model);
    list->m_children.reserve(mailbox->syncState.exists());

    int i = firstUnknownUidOffset;
    while (i < uidMap.size() + static_cast<int>(firstUnknownUidOffset)) {
        // Index inside the uidMap in which the UID of a message at offset i in the list->m_children can be found
        int uidOffset = i - firstUnknownUidOffset;
        Q_ASSERT(uidOffset >= 0);
        Q_ASSERT(uidOffset < uidMap.size());

        // For each UID which is really supposed to be there...

        Q_ASSERT(i <= list->m_children.size());
        if (i == list->m_children.size()) {
            // now we're just adding new messages to the end of the list
            const int futureTotalMessages = mailbox->syncState.exists();
            model->beginInsertRows(parent, i, futureTotalMessages - 1);
            for (/*nothing*/; i < futureTotalMessages; ++i) {
                // Add all messages in one go
                TreeItemMessage *msg = new TreeItemMessage(list);
                msg->m_offset = i;
                // We're iterating with i, so we got to update the uidOffset
                uidOffset = i - firstUnknownUidOffset;
                Q_ASSERT(uidOffset >= 0);
                Q_ASSERT(uidOffset < uidMap.size());
                msg->m_uid = uidMap[uidOffset];
                list->m_children << msg;
            }
            model->endInsertRows();
            Q_ASSERT(i == list->m_children.size());
            Q_ASSERT(i == futureTotalMessages);
        } else if (static_cast<TreeItemMessage *>(list->m_children[i])->m_uid == uidMap[uidOffset]) {
            // If the UID of the "current message" matches, we're okay
            static_cast<TreeItemMessage *>(list->m_children[i])->m_offset = i;
            ++i;
        } else if (static_cast<TreeItemMessage *>(list->m_children[i])->m_uid == 0) {
            // If the UID of the "current message" is zero, replace that with this message
            TreeItemMessage *msg = static_cast<TreeItemMessage*>(list->m_children[i]);
            msg->m_uid = uidMap[uidOffset];
            msg->m_offset = i;
            QModelIndex idx = model->createIndex(i, 0, msg);
            emit model->dataChanged(idx, idx);
            if (msg->accessFetchStatus() == TreeItem::LOADING) {
                // We've got to ask for the message metadata once again; the first attempt happened when the UID was still zero,
                // so this is our chance
                model->askForMsgMetadata(msg, Model::PRELOAD_PER_POLICY);
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
                TreeItemMessage *otherMessage = static_cast<TreeItemMessage*>(list->m_children[pos]);
                if (otherMessage->m_uid != 0 && otherMessage->m_uid != uidMap[uidOffset]) {
                    model->cache()->clearMessage(mailbox->mailbox(), otherMessage->uid());
                    ++pos;
                } else {
                    break;
                }
            }
            Q_ASSERT(pos > i);
            model->beginRemoveRows(parent, i, pos - 1);
            TreeItemChildrenList removedItems = list->m_children.mid(i, pos - i);
            list->m_children.erase(list->m_children.begin() + i, list->m_children.begin() + pos);
            model->endRemoveRows();
            // the m_offset of all subsequent messages will be updated later, at the time *they* are processed
            qDeleteAll(removedItems);
            if (i == list->m_children.size()) {
                // We're asked to add messages to the end of the list. That's something that's already implemented above,
                // so let's reuse that code. That's why we do *not* want to increment the counter here.
            } else {
                Q_ASSERT(i < list->m_children.size());
                // But this case is also already implemented above, so we won't touch the counter from here, either,
                // and let the existing code do its job
            }
        }
    }

    if (i != list->m_children.size()) {
        // remove items at the end
        model->beginRemoveRows(parent, i, list->m_children.size() - 1);
        TreeItemChildrenList removedItems = list->m_children.mid(i);
        list->m_children.erase(list->m_children.begin() + i, list->m_children.end());
        model->endRemoveRows();
        qDeleteAll(removedItems);
    }

    uidMap.clear();

    list->m_totalMessageCount = list->m_children.size();
    list->setFetchStatus(TreeItem::DONE);

    model->emitMessageCountChanged(mailbox);
    model->changeConnectionState(parser, CONN_STATE_SELECTED);
}

QString ObtainSynchronizedMailboxTask::debugIdentification() const
{
    if (! mailboxIndex.isValid())
        return QLatin1String("[invalid mailboxIndex]");

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
    return QString::fromUtf8("%1 %2").arg(statusStr, mailbox->mailbox());
}

void ObtainSynchronizedMailboxTask::notifyInterestingMessages(TreeItemMailbox *mailbox)
{
    Q_ASSERT(mailbox);
    TreeItemMsgList *list = dynamic_cast<Imap::Mailbox::TreeItemMsgList *>(mailbox->m_children[0]);
    Q_ASSERT(list);
    list->recalcVariousMessageCounts(model);
    QModelIndex listIndex = list->toIndex(model);
    Q_ASSERT(listIndex.isValid());
    QModelIndex firstInterestingMessage = model->index(
                // remember, the offset has one-based indexing
                mailbox->syncState.unSeenOffset() ? mailbox->syncState.unSeenOffset() - 1 : 0, 0, listIndex);
    if (!firstInterestingMessage.data(RoleMessageIsMarkedRecent).toBool() &&
            firstInterestingMessage.data(RoleMessageIsMarkedRead).toBool()) {
        // Clearly the reported value is utter nonsense. Let's just scroll to the end instead
        int offset = model->rowCount(listIndex) - 1;
        log(QString::fromUtf8("\"First interesting message\" doesn't look terribly interesting (%1), scrolling to the end at %2 instead")
            .arg(firstInterestingMessage.data(RoleMessageFlags).toStringList().join(QLatin1String(", ")),
                 QString::number(offset)), Common::LOG_MAILBOX_SYNC);
        firstInterestingMessage = model->index(offset, 0, listIndex);
    } else {
        log(QString::fromUtf8("First interesting message at %1 (%2)")
            .arg(QString::number(mailbox->syncState.unSeenOffset()),
                 firstInterestingMessage.data(RoleMessageFlags).toStringList().join(QLatin1String(", "))
                 ), Common::LOG_MAILBOX_SYNC);
    }
    emit model->mailboxFirstUnseenMessage(mailbox->toIndex(model), firstInterestingMessage);
}

bool ObtainSynchronizedMailboxTask::dieIfInvalidMailbox()
{
    if (mailboxIndex.isValid())
        return false;

    // OK, so we are in trouble -- our mailbox has disappeared, but the IMAP server will likely keep us busy with its
    // status updates. This is bad, so we have to get out as fast as possible. All hands, evasive maneuvers!

    log("Mailbox disappeared", Common::LOG_MAILBOX_SYNC);

    if (!unSelectTask) {
        unSelectTask = model->m_taskFactory->createUnSelectTask(model, this);
        connect(unSelectTask, SIGNAL(completed(Imap::Mailbox::ImapTask*)), this, SLOT(slotUnSelectCompleted()));
        unSelectTask->perform();
    }

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
