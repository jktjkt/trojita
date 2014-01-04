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

#include "ThreadingMsgListModel.h"
#include <algorithm>
#include <QBuffer>
#include <QDebug>
#include "Imap/Tasks/SortTask.h"
#include "Imap/Tasks/ThreadTask.h"
#include "ItemRoles.h"
#include "MailboxTree.h"
#include "MsgListModel.h"
#include "QAIM_reset.h"

#if 0
namespace
{
using Imap::Mailbox::ThreadNodeInfo;
QByteArray dumpThreadNodeInfo(const QHash<uint,ThreadNodeInfo> &mapping, const uint nodeId, const uint offset)
{
    QByteArray res;
    QByteArray prefix(offset, ' ');
    QTextStream ss(&res);
    Q_ASSERT(mapping.contains(nodeId));
    const ThreadNodeInfo &node = mapping[nodeId];
    ss << prefix << "ThreadNodeInfo intId " << node.internalId << " UID " << node.uid << " ptr " << node.ptr <<
          " parentIntId " << node.parent << "\n";
    Q_FOREACH(const uint childId, node.children) {
        ss << dumpThreadNodeInfo(mapping, childId, offset + 1);
    }
    return res;
}
}
#endif

namespace Imap
{
namespace Mailbox
{

ThreadingMsgListModel::ThreadingMsgListModel(QObject *parent):
    QAbstractProxyModel(parent), threadingHelperLastId(0), modelResetInProgress(false), threadingInFlight(false),
    m_shallBeThreading(false), m_sortTask(0), m_sortReverse(false), m_currentSortingCriteria(SORT_NONE),
    m_searchValidity(RESULT_INVALIDATED)
{
    m_delayedPrune = new QTimer(this);
    m_delayedPrune->setSingleShot(true);
    m_delayedPrune->setInterval(0);
    connect(m_delayedPrune, SIGNAL(timeout()), this, SLOT(delayedPrune()));
}

void ThreadingMsgListModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    threading.clear();
    ptrToInternal.clear();
    unknownUids.clear();
    threadedRootIds.clear();
    m_currentSortResult.clear();
    m_searchValidity = RESULT_INVALIDATED;

    if (this->sourceModel()) {
        // there's already something, so take care to disconnect all signals
        this->sourceModel()->disconnect(this);
    }

    RESET_MODEL;

    if (!sourceModel)
        return;

    Imap::Mailbox::MsgListModel *msgList = qobject_cast<Imap::Mailbox::MsgListModel *>(sourceModel);
    Q_ASSERT(msgList);
    QAbstractProxyModel::setSourceModel(msgList);

    // FIXME: will need to be expanded when Model supports more signals...
    connect(sourceModel, SIGNAL(modelReset()), this, SLOT(resetMe()));
    connect(sourceModel, SIGNAL(layoutAboutToBeChanged()), this, SIGNAL(layoutAboutToBeChanged()));
    connect(sourceModel, SIGNAL(layoutChanged()), this, SIGNAL(layoutChanged()));
    connect(sourceModel, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)),
            this, SLOT(handleDataChanged(const QModelIndex &, const QModelIndex &)));
    connect(sourceModel, SIGNAL(rowsAboutToBeRemoved(const QModelIndex &, int, int)),
            this, SLOT(handleRowsAboutToBeRemoved(const QModelIndex &, int,int)));
    connect(sourceModel, SIGNAL(rowsRemoved(const QModelIndex &, int, int)),
            this, SLOT(handleRowsRemoved(const QModelIndex &, int,int)));
    connect(sourceModel, SIGNAL(rowsAboutToBeInserted(const QModelIndex &, int, int)),
            this, SLOT(handleRowsAboutToBeInserted(const QModelIndex &, int,int)));
    connect(sourceModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)),
            this, SLOT(handleRowsInserted(const QModelIndex &, int,int)));
    resetMe();
}

QVariant ThreadingMsgListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (sourceModel()) {
        return sourceModel()->headerData(section, orientation, role);
    } else {
        return QVariant();
    }
}

void ThreadingMsgListModel::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    // We don't support updates which concern multiple rows at this time.
    // Doing that would very likely require a completely different codepath due to threading...
    Q_ASSERT(topLeft.parent() == bottomRight.parent());
    Q_ASSERT(topLeft.row() == bottomRight.row());
    QModelIndex translated = mapFromSource(topLeft);

    emit dataChanged(translated, translated.sibling(translated.row(), bottomRight.column()));

    // We provide funny data like "does this thread contain unread messages?". Now the original signal might mean that flags of a
    // nested message have changed. In order to always be consistent, we have to find the thread root and emit dataChanged() on that
    // as well.
    QModelIndex rootCandidate = translated;
    while (rootCandidate.parent().isValid()) {
        rootCandidate = rootCandidate.parent();
    }
    if (rootCandidate != translated) {
        // We're really an embedded message
        emit dataChanged(rootCandidate, rootCandidate.sibling(rootCandidate.row(), bottomRight.column()));
    }

    QSet<TreeItem*>::iterator persistent = unknownUids.find(static_cast<TreeItem*>(topLeft.internalPointer()));
    if (persistent != unknownUids.end()) {
        // The message wasn't fully synced before, and now it is
        persistent = unknownUids.erase(persistent);
        if (unknownUids.isEmpty()) {
            wantThreading();
        }
    }
}

QModelIndex ThreadingMsgListModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_ASSERT(!parent.isValid() || parent.model() == this);

    if (threading.isEmpty()) {
        // mapping not available yet
        return QModelIndex();
    }

    if (row < 0 || column < 0 || column >= MsgListModel::COLUMN_COUNT)
        return QModelIndex();

    if (parent.isValid() && parent.column() != 0) {
        // only the first column should have children
        return QModelIndex();
    }

    uint parentId = parent.isValid() ? parent.internalId() : 0;

    QHash<uint,ThreadNodeInfo>::const_iterator it = threading.constFind(parentId);
    Q_ASSERT(it != threading.constEnd());

    if (it->children.size() <= row)
        return QModelIndex();

    return createIndex(row, column, it->children[row]);
}

QModelIndex ThreadingMsgListModel::parent(const QModelIndex &index) const
{
    if (! index.isValid() || index.model() != this)
        return QModelIndex();

    if (threading.isEmpty())
        return QModelIndex();

    if (index.row() < 0 || index.column() < 0 || index.column() >= MsgListModel::COLUMN_COUNT)
        return QModelIndex();

    QHash<uint,ThreadNodeInfo>::const_iterator node = threading.constFind(index.internalId());
    if (node == threading.constEnd())
        return QModelIndex();

    QHash<uint,ThreadNodeInfo>::const_iterator parentNode = threading.constFind(node->parent);
    Q_ASSERT(parentNode != threading.constEnd());
    Q_ASSERT(parentNode->internalId == node->parent);

    if (parentNode->internalId == 0)
        return QModelIndex();

    return createIndex(parentNode->offset, 0, parentNode->internalId);
}

bool ThreadingMsgListModel::hasChildren(const QModelIndex &parent) const
{
    if (parent.isValid() && parent.column() != 0)
        return false;

    return ! threading.isEmpty() && ! threading.value(parent.internalId()).children.isEmpty();
}

int ThreadingMsgListModel::rowCount(const QModelIndex &parent) const
{
    if (threading.isEmpty())
        return 0;

    if (parent.isValid() && parent.column() != 0)
        return 0;

    return threading.value(parent.internalId()).children.size();
}

int ThreadingMsgListModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid() && parent.column() != 0)
        return 0;

    return MsgListModel::COLUMN_COUNT;
}

QModelIndex ThreadingMsgListModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid() || !proxyIndex.internalId())
        return QModelIndex();

    if (threading.isEmpty())
        return QModelIndex();

    Imap::Mailbox::MsgListModel *msgList = qobject_cast<Imap::Mailbox::MsgListModel *>(sourceModel());
    Q_ASSERT(msgList);

    QHash<uint,ThreadNodeInfo>::const_iterator node = threading.constFind(proxyIndex.internalId());
    if (node == threading.constEnd())
        return QModelIndex();

    if (node->ptr) {
        return msgList->createIndex(node->ptr->row(), proxyIndex.column(), node->ptr);
    } else {
        // it's a fake message
        return QModelIndex();
    }
}

QModelIndex ThreadingMsgListModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid())
        return QModelIndex();

    Q_ASSERT(sourceIndex.model() == sourceModel());

    QHash<void *,uint>::const_iterator it = ptrToInternal.constFind(sourceIndex.internalPointer());
    if (it == ptrToInternal.constEnd())
        return QModelIndex();

    const uint internalId = *it;

    QHash<uint,ThreadNodeInfo>::const_iterator node = threading.constFind(internalId);
    if (node == threading.constEnd()) {
        // The filtering criteria say that this index shall not be visible
        return QModelIndex();
    }
    Q_ASSERT(node != threading.constEnd());

    return createIndex(node->offset, sourceIndex.column(), internalId);
}

QVariant ThreadingMsgListModel::data(const QModelIndex &proxyIndex, int role) const
{
    if (! proxyIndex.isValid() || proxyIndex.model() != this)
        return QVariant();

    QHash<uint,ThreadNodeInfo>::const_iterator it = threading.constFind(proxyIndex.internalId());
    Q_ASSERT(it != threading.constEnd());

    if (it->ptr) {
        // It's a real item which exists in the underlying model
        if (role == RoleThreadRootWithUnreadMessages) {
            if (proxyIndex.parent().isValid()) {
                // We don't support this kind of questions for other messages than the roots of the threads.
                // Other components, like the QML bindings, are however happy to request that, so let's just return
                // a reasonable result instead of whinning about callers requesting useless stuff.
                return false;
            } else {
                return threadContainsUnreadMessages(it->internalId);
            }
        } else {
            return QAbstractProxyModel::data(proxyIndex, role);
        }
    }

    switch (role) {
    case Qt::DisplayRole:
        if (proxyIndex.column() == 0)
            return tr("[Message is missing]");
        break;
    case Qt::ToolTipRole:
        return tr("This thread refers to an extra message, but that message is not present in the "
                  "selected mailbox, or is missing from the current search context.");
    }
    return QVariant();
}

Qt::ItemFlags ThreadingMsgListModel::flags(const QModelIndex &index) const
{
    if (! index.isValid() || index.model() != this)
        return Qt::NoItemFlags;

    QHash<uint,ThreadNodeInfo>::const_iterator it = threading.constFind(index.internalId());
    Q_ASSERT(it != threading.constEnd());
    if (it->ptr && it->uid)
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;

    return Qt::NoItemFlags;

}

void ThreadingMsgListModel::handleRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(!parent.isValid());

    for (int i = start; i <= end; ++i) {
        QModelIndex index = sourceModel()->index(i, 0, parent);
        Q_ASSERT(index.isValid());
        QModelIndex translated = mapFromSource(index);

        unknownUids.remove(static_cast<TreeItem*>(index.internalPointer()));

        if (!translated.isValid()) {
            // The index being removed wasn't visible in our mapping anyway
            continue;
        }

        Q_ASSERT(translated.isValid());
        QHash<uint,ThreadNodeInfo>::iterator it = threading.find(translated.internalId());
        Q_ASSERT(it != threading.end());
        it->uid = 0;
        it->ptr = 0;
    }
}

void ThreadingMsgListModel::handleRowsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(!parent.isValid());
    Q_UNUSED(start);
    Q_UNUSED(end);
    if (!m_delayedPrune->isActive())
        m_delayedPrune->start();
}

void ThreadingMsgListModel::delayedPrune()
{
    emit layoutAboutToBeChanged();
    updatePersistentIndexesPhase1();
    pruneTree();
    updatePersistentIndexesPhase2();
    emit layoutChanged();
}

void ThreadingMsgListModel::handleRowsAboutToBeInserted(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(!parent.isValid());

    int myStart = threading[0].children.size();
    int myEnd = myStart + (end - start);
    beginInsertRows(QModelIndex(), myStart, myEnd);
}

void ThreadingMsgListModel::handleRowsInserted(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(!parent.isValid());

    for (int i = start; i <= end; ++i) {
        QModelIndex index = sourceModel()->index(i, 0);
        uint uid = index.data(RoleMessageUid).toUInt();
        ThreadNodeInfo node;
        node.internalId = ++threadingHelperLastId;
        node.uid = uid;
        node.ptr = static_cast<TreeItem *>(index.internalPointer());
        node.offset = threading[0].children.size();
        threading[node.internalId] = node;
        threading[0].children << node.internalId;
        ptrToInternal[node.ptr] = node.internalId;
        if (!node.uid) {
            unknownUids << static_cast<TreeItem*>(index.internalPointer());
        } else {
            threadedRootIds.append(node.internalId);
        }
    }
    endInsertRows();

    if (!m_sortTask || !m_sortTask->isPersistent()) {
        m_currentSortResult.clear();
        if (m_searchValidity == RESULT_FRESH)
            m_searchValidity = RESULT_INVALIDATED;
    }

    if (m_shallBeThreading)
        wantThreading();
}

void ThreadingMsgListModel::resetMe()
{
    // Prevent possible recursion here
    if (modelResetInProgress)
        return;

    modelResetInProgress = true;
    threading.clear();
    ptrToInternal.clear();
    unknownUids.clear();
    threadedRootIds.clear();
    m_currentSortResult.clear();
    m_searchValidity = RESULT_INVALIDATED;
    RESET_MODEL;
    updateNoThreading();
    modelResetInProgress = false;

    if (m_shallBeThreading)
        wantThreading();
}

void ThreadingMsgListModel::updateNoThreading()
{
    threadingHelperLastId = 0;

    if (!sourceModel()) {
        // Maybe we got reset because the parent model is no longer here...
        if (! threading.isEmpty()) {
            beginRemoveRows(QModelIndex(), 0, rowCount() - 1);
            threading.clear();
            ptrToInternal.clear();
            endRemoveRows();
        }
        unknownUids.clear();
        return;
    }

    emit layoutAboutToBeChanged();
    updatePersistentIndexesPhase1();
    threading.clear();
    ptrToInternal.clear();
    unknownUids.clear();
    threadedRootIds.clear();

    int upstreamMessages = sourceModel()->rowCount();
    QList<uint> allIds;
    QHash<uint,ThreadNodeInfo> newThreading;
    QHash<void *,uint> newPtrToInternal;

    if (upstreamMessages) {
        // Prefer the direct pointer access instead of going through the MVC API -- similar to how applyThreading() works.
        // This improves the speed of the testSortingPerformance benchmark by 18%.
        QModelIndex firstMessageIndex = sourceModel()->index(0, 0);
        Q_ASSERT(firstMessageIndex.isValid());
        const Model *realModel = 0;
        TreeItem *firstMessagePtr = Model::realTreeItem(firstMessageIndex, &realModel);
        Q_ASSERT(firstMessagePtr);
        // If the next asserts fails, it means that the implementation of MsgListModel has changed and uses its own pointers
        Q_ASSERT(firstMessagePtr == firstMessageIndex.internalPointer());
        TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(firstMessagePtr->parent());
        Q_ASSERT(list);

        for (int i = 0; i < upstreamMessages; ++i) {
            TreeItemMessage *ptr = static_cast<TreeItemMessage*>(list->m_children[i]);
            Q_ASSERT(ptr);
            ThreadNodeInfo node;
            node.internalId = i + 1;
            node.uid = ptr->uid();
            node.ptr = ptr;
            node.offset = i;
            newThreading[node.internalId] = node;
            allIds.append(node.internalId);
            newPtrToInternal[node.ptr] = node.internalId;
            if (!node.uid) {
                unknownUids << ptr;
            }
        }
    }

    if (newThreading.size()) {
        threading = newThreading;
        ptrToInternal = newPtrToInternal;
        threading[ 0 ].children = allIds;
        threading[ 0 ].ptr = 0;
        threadingHelperLastId = newThreading.size();
        threadedRootIds = threading[0].children;
    }
    updatePersistentIndexesPhase2();
    emit layoutChanged();
}

void ThreadingMsgListModel::wantThreading(const SkipSortSearch skipSortSearch)
{
    if (!sourceModel() || !sourceModel()->rowCount() || !m_shallBeThreading) {
        updateNoThreading();
        if (skipSortSearch == AUTO_SORT_SEARCH) {
            searchSortPreferenceImplementation(m_currentSearchConditions, m_currentSortingCriteria, m_sortReverse ? Qt::DescendingOrder : Qt::AscendingOrder);
        }
        return;
    }

    if (threadingInFlight) {
        // Imagine the following scenario:
        // <<< "* 3 EXISTS"
        // Message 2 has unknown UID
        // >>> "y4 UID FETCH 66:* (FLAGS)"
        // >>> "y5 UID THREAD REFS utf-8 ALL"
        // <<< "* 3 FETCH (UID 66 FLAGS ())"
        // Got UID for seq# 3
        // ThreadingMsgListModel::wantThreading: THREAD contains info about UID 1 (or higher), mailbox has 66
        //    *** this is the interesting part ***
        // <<< "y4 OK fetch"
        // <<< "* THREAD (1)(2)(66)"
        // <<< "y5 OK thread"
        // >>> "y6 UID THREAD REFS utf-8 ALL"
        //
        // See, at the indicated (***) place, we already have an in-flight THREAD request and receive UID for newly arrived
        // message.  We certainly don't want to ask for threading once again; it's better to wait a bit and only ask when the
        // to-be-received THREAD does not contain all required UIDs.
        if (skipSortSearch == AUTO_SORT_SEARCH) {
            searchSortPreferenceImplementation(m_currentSearchConditions, m_currentSortingCriteria, m_sortReverse ? Qt::DescendingOrder : Qt::AscendingOrder);
        }
        return;
    }

    const Imap::Mailbox::Model *realModel;
    QModelIndex someMessage = sourceModel()->index(0,0);
    QModelIndex realIndex;
    Imap::Mailbox::Model::realTreeItem(someMessage, &realModel, &realIndex);
    QModelIndex mailbox = realIndex.parent().parent();
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList*>(static_cast<TreeItem*>(realIndex.parent().internalPointer()));
    Q_ASSERT(list);

    // Something has happened and we want to process the THREAD response
    QVector<Imap::Responses::ThreadingNode> mapping = realModel->cache()->messageThreading(mailbox.data(RoleMailboxName).toString());

    // Find the UID of the last message in the mailbox
    uint highestUidInMailbox = findHighestUidInMailbox(list);
    uint highestUidInThreadingLowerBound = findHighEnoughNumber(mapping, highestUidInMailbox);

    logTrace(QString::fromUtf8("ThreadingMsgListModel::wantThreading: THREAD contains info about UID %1 (or higher), mailbox has %2")
             .arg(QString::number(highestUidInThreadingLowerBound), QString::number(highestUidInMailbox)));

    if (highestUidInThreadingLowerBound >= highestUidInMailbox) {
        // There's no point asking for data at this point, we shall just apply threading
        applyThreading(mapping);
    } else {
        // There's apparently at least one known UID whose threading info we do not know; that means that we have to ask the
        // server here.
        auto roughlyLastKnown = const_cast<Model*>(realModel)->findMessageOrNextOneByUid(list, highestUidInThreadingLowerBound);
        if (list->m_children.end() - roughlyLastKnown >= 50 || roughlyLastKnown == list->m_children.begin()) {
            askForThreading();
        } else {
            askForThreading(static_cast<TreeItemMessage*>(*roughlyLastKnown)->uid() + 1);
        }
    }
}

uint ThreadingMsgListModel::findHighestUidInMailbox(TreeItemMsgList *list)
{
    uint highestUidInMailbox = 0;
    for (int i = sourceModel()->rowCount() - 1; i > -1 && !highestUidInMailbox; --i) {
        highestUidInMailbox = dynamic_cast<TreeItemMessage*>(list->m_children[i])->uid();
    }
    return highestUidInMailbox;
}

uint ThreadingMsgListModel::findHighEnoughNumber(const QVector<Responses::ThreadingNode> &mapping, uint marker)
{
    if (mapping.isEmpty())
        return 0;

    // Find the highest UID for which we have the threading info
    uint highestUidInThreadingLowerBound = 0;

    // If the threading already contains everything we need, we could have a higher chance of finding the high enough UID at the
    // end of the list.  On the other hand, in case when the cached THREAD response does not cintain everything we need, we're out
    // of luck, we have absolutely no guarantee about relative greatness of parent/child/siblings in the tree.
    // Searching backward could lead to faster lookups, but we cannot avoid a full lookup in the bad case.
    for (int i = mapping.size() - 1; i >= 0; --i) {
        if (highestUidInThreadingLowerBound < mapping[i].num) {
            highestUidInThreadingLowerBound = mapping[i].num;

            if (highestUidInThreadingLowerBound >= marker) {
                // There's no point going further, we already know that we shall ask for threading
                return highestUidInThreadingLowerBound;
            }
        }

        // OK, we have to consult our children
        highestUidInThreadingLowerBound = qMax(highestUidInThreadingLowerBound, findHighEnoughNumber(mapping[i].children, marker));
        if (highestUidInThreadingLowerBound >= marker) {
            return highestUidInThreadingLowerBound;
        }
    }
    return highestUidInThreadingLowerBound;
}

void ThreadingMsgListModel::askForThreading(const uint firstUnknownUid)
{
    Q_ASSERT(m_shallBeThreading);
    Q_ASSERT(sourceModel());
    Q_ASSERT(sourceModel()->rowCount());

    const Imap::Mailbox::Model *realModel;
    QModelIndex someMessage = sourceModel()->index(0,0);
    QModelIndex realIndex;
    Imap::Mailbox::Model::realTreeItem(someMessage, &realModel, &realIndex);
    QModelIndex mailboxIndex = realIndex.parent().parent();

    if (realModel->capabilities().contains(QLatin1String("THREAD=REFS"))) {
        requestedAlgorithm = "REFS";
    } else if (realModel->capabilities().contains(QLatin1String("THREAD=REFERENCES"))) {
        requestedAlgorithm = "REFERENCES";
    } else if (realModel->capabilities().contains(QLatin1String("THREAD=ORDEREDSUBJECT"))) {
        requestedAlgorithm = "ORDEREDSUBJECT";
    }

    if (! requestedAlgorithm.isEmpty()) {
        threadingInFlight = true;
        ThreadTask *threadTask;
        if (firstUnknownUid && realModel->capabilities().contains(QLatin1String("INCTHREAD"))) {
            threadTask = realModel->m_taskFactory->
                    createIncrementalThreadTask(const_cast<Model *>(realModel), mailboxIndex, requestedAlgorithm,
                                                                    QStringList() << "INTHREAD" << requestedAlgorithm << "UID" <<
                                                                        Sequence::startingAt(firstUnknownUid).toByteArray());
            connect(threadTask, SIGNAL(incrementalThreadingAvailable(Responses::ESearch::IncrementalThreadingData_t)),
                    this, SLOT(slotIncrementalThreadingAvailable(Responses::ESearch::IncrementalThreadingData_t)));
            connect(threadTask, SIGNAL(failed(QString)), this, SLOT(slotIncrementalThreadingFailed()));
        } else {
            threadTask = realModel->m_taskFactory->createThreadTask(const_cast<Model *>(realModel), mailboxIndex,
                                                                    requestedAlgorithm, QStringList() << QLatin1String("ALL"));
            connect(realModel, SIGNAL(threadingAvailable(QModelIndex,QByteArray,QStringList,QVector<Imap::Responses::ThreadingNode>)),
                    this, SLOT(slotThreadingAvailable(QModelIndex,QByteArray,QStringList,QVector<Imap::Responses::ThreadingNode>)));
            connect(realModel, SIGNAL(threadingFailed(QModelIndex,QByteArray,QStringList)),
                    this, SLOT(slotThreadingFailed(QModelIndex,QByteArray,QStringList)));
        }
    }
}

/** @short Gather all UIDs present in the mapping and push them into the "uids" vector */
static void gatherAllUidsFromThreadNode(QVector<uint> &uids, const QVector<Responses::ThreadingNode> &list)
{
    for (QVector<Responses::ThreadingNode>::const_iterator it = list.constBegin(); it != list.constEnd(); ++it) {
        uids.push_back(it->num);
        gatherAllUidsFromThreadNode(uids, it->children);
    }
}

void ThreadingMsgListModel::slotIncrementalThreadingAvailable(const Responses::ESearch::IncrementalThreadingData_t &data)
{
    // Preparation: get through to the real model
    const Imap::Mailbox::Model *realModel;
    QModelIndex someMessage = sourceModel()->index(0,0);
    Q_ASSERT(someMessage.isValid());
    QModelIndex realIndex;
    Imap::Mailbox::Model::realTreeItem(someMessage, &realModel, &realIndex);
    QModelIndex mailboxIndex = realIndex.parent().parent();
    Q_ASSERT(mailboxIndex.isValid());

    // First phase: remove all messages mentioned in the incremental responses from their original placement
    QVector<uint> affectedUids;
    for (Responses::ESearch::IncrementalThreadingData_t::const_iterator it = data.constBegin(); it != data.constEnd(); ++it) {
        gatherAllUidsFromThreadNode(affectedUids, it->thread);
    }
    qSort(affectedUids);
    QList<TreeItemMessage*> affectedMessages = const_cast<Model*>(realModel)->
            findMessagesByUids(static_cast<TreeItemMailbox*>(mailboxIndex.internalPointer()), affectedUids.toList());
    QHash<uint,void *> uidToPtrCache;


    emit layoutAboutToBeChanged();
    updatePersistentIndexesPhase1();
    for (QList<TreeItemMessage*>::const_iterator it = affectedMessages.constBegin(); it != affectedMessages.constEnd(); ++it) {
        QHash<void *,uint>::const_iterator ptrMappingIt = ptrToInternal.constFind(*it);
        Q_ASSERT(ptrMappingIt != ptrToInternal.constEnd());
        QHash<uint,ThreadNodeInfo>::iterator threadIt = threading.find(*ptrMappingIt);
        Q_ASSERT(threadIt != threading.end());
        uidToPtrCache[(*it)->uid()] = threadIt->ptr;
        threadIt->ptr = 0;
    }
    pruneTree();
    updatePersistentIndexesPhase2();
    emit layoutChanged();

    // Second phase: for each message whose UID is returned by the server, update the threading data
    QSet<uint> usedNodes;
    emit layoutAboutToBeChanged();
    updatePersistentIndexesPhase1();
    for (Responses::ESearch::IncrementalThreadingData_t::const_iterator it = data.constBegin(); it != data.constEnd(); ++it) {
        registerThreading(it->thread, 0, uidToPtrCache, usedNodes);
        int actualOffset = threading[0].children.size() - 1;
        int expectedOffsetOfPrevious = threading[0].children.indexOf(it->previousThreadRoot);
        if (actualOffset == expectedOffsetOfPrevious + 1) {
            // it's on the correct position, yay!
        } else {
            // move the new subthread to a correct place
            threading[0].children.insert(expectedOffsetOfPrevious + 1, threading[0].children.takeLast());
            // push the rest (including the new arrival) forward
            for (int i = expectedOffsetOfPrevious + 1; i < threading[0].children.size(); ++i) {
                threading[threading[0].children[i]].offset = i;
            }
        }
    }
    updatePersistentIndexesPhase2();
    emit layoutChanged();
}

void ThreadingMsgListModel::slotIncrementalThreadingFailed()
{
}

bool ThreadingMsgListModel::shouldIgnoreThisThreadingResponse(const QModelIndex &mailbox, const QByteArray &algorithm,
        const QStringList &searchCriteria, const Model **realModel)
{
    QModelIndex someMessage = sourceModel()->index(0,0);
    if (!someMessage.isValid())
        return true;
    const Model *model;
    QModelIndex realIndex;
    Imap::Mailbox::Model::realTreeItem(someMessage, &model, &realIndex);
    QModelIndex mailboxIndex = realIndex.parent().parent();
    if (mailboxIndex != mailbox) {
        // this is for another mailbox
        return true;
    }

    if (algorithm != requestedAlgorithm) {
        logTrace(QString::fromUtf8("Weird, asked for threading via %1 but got %2 instead -- ignoring")
                 .arg(QString::fromUtf8(requestedAlgorithm), QString::fromUtf8(algorithm)));
        return true;
    }

    if (searchCriteria.size() != 1 || searchCriteria.front() != QLatin1String("ALL")) {
        QString buf;
        QTextStream ss(&buf);
        logTrace(QString::fromUtf8("Weird, requesting messages matching ALL, but got this instead: %1")
                 .arg(searchCriteria.join(QLatin1String(", "))));
        return true;
    }

    if (realModel)
        *realModel = model;
    return false;
}

void ThreadingMsgListModel::slotThreadingFailed(const QModelIndex &mailbox, const QByteArray &algorithm, const QStringList &searchCriteria)
{
    // Better safe than sorry -- prevent infinite waiting to the maximal possible extent
    threadingInFlight = false;

    if (shouldIgnoreThisThreadingResponse(mailbox, algorithm, searchCriteria))
        return;

    disconnect(sender(), 0, this,
               SLOT(slotThreadingAvailable(QModelIndex,QByteArray,QStringList,QVector<Imap::Responses::ThreadingNode>)));
    disconnect(sender(), 0, this,
               SLOT(slotThreadingFailed(QModelIndex,QByteArray,QStringList)));

    updateNoThreading();
}

void ThreadingMsgListModel::slotThreadingAvailable(const QModelIndex &mailbox, const QByteArray &algorithm,
        const QStringList &searchCriteria,
        const QVector<Imap::Responses::ThreadingNode> &mapping)
{
    // Better safe than sorry -- prevent infinite waiting to the maximal possible extent
    threadingInFlight = false;

    const Model *model = 0;
    if (shouldIgnoreThisThreadingResponse(mailbox, algorithm, searchCriteria, &model))
        return;

    disconnect(sender(), 0, this,
               SLOT(slotThreadingAvailable(QModelIndex,QByteArray,QStringList,QVector<Imap::Responses::ThreadingNode>)));
    disconnect(sender(), 0, this,
               SLOT(slotThreadingFailed(QModelIndex,QByteArray,QStringList)));

    model->cache()->setMessageThreading(mailbox.data(RoleMailboxName).toString(), mapping);

    // Indirect processing here -- the wantThreading() will check that the received response really contains everything we need
    // and if it does, simply applyThreading() that.  If there's something missing, it will ask for the threading again.
    if (m_shallBeThreading)
        wantThreading();
}

void ThreadingMsgListModel::slotSortingAvailable(const QList<uint> &uids)
{
    if (!m_sortTask->isPersistent()) {
        disconnect(m_sortTask, 0, this, SLOT(slotSortingAvailable(QList<uint>)));
        disconnect(m_sortTask, 0, this, SLOT(slotSortingFailed()));
        disconnect(m_sortTask, 0, this, SLOT(slotSortingIncrementalUpdate(Imap::Responses::ESearch::IncrementalContextData_t)));

        m_sortTask = 0;
    }

    m_currentSortResult = uids;
    if (m_searchValidity == RESULT_ASKED)
        m_searchValidity = RESULT_FRESH;
    wantThreading();
}

void ThreadingMsgListModel::slotSortingFailed()
{
    disconnect(m_sortTask, 0, this, SLOT(slotSortingAvailable(QList<uint>)));
    disconnect(m_sortTask, 0, this, SLOT(slotSortingFailed()));
    disconnect(m_sortTask, 0, this, SLOT(slotSortingIncrementalUpdate(Imap::Responses::ESearch::IncrementalContextData_t)));

    m_sortTask = 0;
    m_sortReverse = false;
    calculateNullSort();
    applySort();
    emit sortingFailed();
}

void ThreadingMsgListModel::slotSortingIncrementalUpdate(const Responses::ESearch::IncrementalContextData_t &updates)
{
    for (Responses::ESearch::IncrementalContextData_t::const_iterator it = updates.constBegin(); it != updates.constEnd(); ++it) {
        switch (it->modification) {
        case Responses::ESearch::ContextIncrementalItem::ADDTO:
            for (int i = 0; i < it->uids.size(); ++i)  {
                int offset = it->offset + i;
                if (offset < 0 || offset >= m_currentSortResult.size()) {
                    throw MailboxException("ESEARCH: ADDTO out of bounds");
                }
                m_currentSortResult.insert(offset, it->uids[i]);
            }
            break;

        case Responses::ESearch::ContextIncrementalItem::REMOVEFROM:
            for (int i = 0; i < it->uids.size(); ++i)  {
                if (it->offset == 0) {
                    // When the offset is not given, we have to find it ourselves
                    m_currentSortResult.removeOne(it->uids[i]);
                } else {
                    // We're given an offset, so let's make sure it is a correct one
                    int offset = it->offset + i - 1;
                    if (offset < 0 || offset >= m_currentSortResult.size()) {
                        throw MailboxException("ESEARCH: REMOVEFROM out of bounds");
                    }
                    if (m_currentSortResult[offset] != it->uids[i]) {
                        throw MailboxException("ESEARCH: REMOVEFROM UID mismatch");
                    }
                    m_currentSortResult.removeAt(offset);
                }
            }
            break;
        }
    }
    m_searchValidity = RESULT_FRESH;
    wantThreading();
}

/** @short Store UIDs of the thread roots as the "current search order" */
void ThreadingMsgListModel::calculateNullSort()
{
    m_currentSortResult.clear();
#if QT_VERSION >= 0x040700
    m_currentSortResult.reserve(threadedRootIds.size());
#endif
    Q_FOREACH(const uint internalId, threadedRootIds) {
        QHash<uint,ThreadNodeInfo>::const_iterator it = threading.constFind(internalId);
        if (it == threading.constEnd())
            continue;
        if (it->uid)
            m_currentSortResult.append(it->uid);
    }
    m_searchValidity = RESULT_FRESH;
}

void ThreadingMsgListModel::applyThreading(const QVector<Imap::Responses::ThreadingNode> &mapping)
{
    if (! unknownUids.isEmpty()) {
        // Some messages have UID zero, which means that they weren't loaded yet. Too bad.
        logTrace(QString::fromUtf8("%1 messages have 0 UID").arg(unknownUids.size()));
        return;
    }

    emit layoutAboutToBeChanged();

    updatePersistentIndexesPhase1();

    threading.clear();
    ptrToInternal.clear();
    // Default-construct the root node
    threading[ 0 ].ptr = 0;

    // At first, initialize threading nodes for all messages which are right now available in the mailbox.
    // We risk that we will have to delete some of them later on, but this is likely better than doing a lookup
    // for each UID individually (remember, the THREAD response might contain UIDs in crazy order).
    int upstreamMessages = sourceModel()->rowCount();
    QHash<uint,void *> uidToPtrCache;
    QSet<uint> usedNodes;
    uidToPtrCache.reserve(upstreamMessages);
    threading.reserve(upstreamMessages);
    ptrToInternal.reserve(upstreamMessages);

    if (upstreamMessages) {
        // Work with pointers instead going through the MVC API for performance.
        // This matters (at least that's what by benchmarks said).
        QModelIndex firstMessageIndex = sourceModel()->index(0, 0);
        Q_ASSERT(firstMessageIndex.isValid());
        const Model *realModel = 0;
        TreeItem *firstMessagePtr = Model::realTreeItem(firstMessageIndex, &realModel);
        Q_ASSERT(firstMessagePtr);
        // If the next asserts fails, it means that the implementation of MsgListModel has changed and uses its own pointers
        Q_ASSERT(firstMessagePtr == firstMessageIndex.internalPointer());
        TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(firstMessagePtr->parent());
        Q_ASSERT(list);
        for (int i = 0; i < upstreamMessages; ++i) {
            ThreadNodeInfo node;
            node.uid = dynamic_cast<TreeItemMessage *>(list->m_children[i])->uid();
            if (! node.uid) {
                throw UnknownMessageIndex("Encountered a message with zero UID when threading. This is a bug in Trojita, sorry.");
            }

            node.internalId = i + 1;
            node.ptr = list->m_children[i];
            uidToPtrCache[node.uid] = node.ptr;
            threadingHelperLastId = node.internalId;
            // We're creating a new node here
            Q_ASSERT(!threading.contains(node.internalId));
            threading[ node.internalId ] = node;
            ptrToInternal[ node.ptr ] = node.internalId;
        }
    }

    // Mark the root node as always present
    usedNodes.insert(0);

    // Set up parents and find the list of all used nodes
    registerThreading(mapping, 0, uidToPtrCache, usedNodes);

    // Now remove all messages which were not referenced in the THREAD response from our mapping
    QHash<uint,ThreadNodeInfo>::iterator it = threading.begin();
    while (it != threading.end()) {
        if (usedNodes.contains(it.key())) {
            // this message should be shown
            ++it;
        } else {
            // this message is not included in the list of messages actually to be shown
            ptrToInternal.remove(it->ptr);
            it = threading.erase(it);
        }
    }
    pruneTree();
    updatePersistentIndexesPhase2();
    if (rowCount())
        threadedRootIds = threading[0].children;
    emit layoutChanged();

    // If the sorting was active before, we shall reactivate it now
    searchSortPreferenceImplementation(m_currentSearchConditions, m_currentSortingCriteria, m_sortReverse ? Qt::DescendingOrder : Qt::AscendingOrder);
}

void ThreadingMsgListModel::registerThreading(const QVector<Imap::Responses::ThreadingNode> &mapping, uint parentId, const QHash<uint,void *> &uidToPtr, QSet<uint> &usedNodes)
{
    Q_FOREACH(const Imap::Responses::ThreadingNode &node, mapping) {
        uint nodeId;
        QHash<uint,void *>::const_iterator ptrIt;
        if (node.num == 0 ||
                (ptrIt = uidToPtr.find(node.num)) == uidToPtr.constEnd()) {
            // Either this is an empty node, or the THREAD response references a UID which is no longer in the mailbox.
            // This is a valid scenario; it can happen e.g. when reusing data from cache, or when a message got
            // expunged after the untagged THREAD was received, but before the tagged OK.
            // We cannot just ignore this node, though, because it might have some children which we would otherwise
            // simply hide.
            // The ptrIt which is initialized by the condition is used in the else branch.
            ThreadNodeInfo fake;
            fake.internalId = ++threadingHelperLastId;
            fake.parent = parentId;
            Q_ASSERT(threading.contains(parentId));
            // The child will be registered to the list of parent's children after the if/else branch
            threading[ fake.internalId ] = fake;
            nodeId = fake.internalId;
        } else {
            QHash<void *,uint>::const_iterator nodeIt = ptrToInternal.constFind(*ptrIt);
            // The following assert would fail if there was a node with a valid UID, but not in our ptrToInternal mapping.
            // That is however non-issue, as we pre-create nodes for all messages beforehand.
            Q_ASSERT(nodeIt != ptrToInternal.constEnd());
            nodeId = *nodeIt;
            // This is needed for the incremental stuff
            threading[nodeId].ptr = static_cast<TreeItem*>(*ptrIt);
        }
        threading[nodeId].offset = threading[parentId].children.size();
        threading[ parentId ].children.append(nodeId);
        threading[ nodeId ].parent = parentId;
        usedNodes.insert(nodeId);
        registerThreading(node.children, nodeId, uidToPtr, usedNodes);
    }
}

/** @short Gather a list of persistent indexes which we have to transform after out layout change */
void ThreadingMsgListModel::updatePersistentIndexesPhase1()
{
    oldPersistentIndexes = persistentIndexList();
    oldPtrs.clear();
    Q_FOREACH(const QModelIndex &idx, oldPersistentIndexes) {
        // the index could get invalidated by the pruneTree() or something else manipulating our threading
        bool isOk = idx.isValid() && threading.contains(idx.internalId());
        if (!isOk) {
            oldPtrs << 0;
            continue;
        }
        QModelIndex translated = mapToSource(idx);
        if (!translated.isValid()) {
            // another stale item
            oldPtrs << 0;
            continue;
        }
        oldPtrs << translated.internalPointer();
    }
}

/** @short Update the gathered persistent indexes after our change in the layout */
void ThreadingMsgListModel::updatePersistentIndexesPhase2()
{
    Q_ASSERT(oldPersistentIndexes.size() == oldPtrs.size());
    QList<QModelIndex> updatedIndexes;
    for (int i = 0; i < oldPersistentIndexes.size(); ++i) {
        QHash<void *,uint>::const_iterator ptrIt = ptrToInternal.constFind(oldPtrs[i]);
        if (ptrIt == ptrToInternal.constEnd()) {
            // That message is no longer there
            updatedIndexes.append(QModelIndex());
            continue;
        }
        QHash<uint,ThreadNodeInfo>::const_iterator it = threading.constFind(*ptrIt);
        if (it == threading.constEnd()) {
            // Filtering doesn't accept this index, let's declare it dead
            updatedIndexes.append(QModelIndex());
        } else {
            updatedIndexes.append(createIndex(it->offset, oldPersistentIndexes[i].column(), it->internalId));
        }
    }
    Q_ASSERT(oldPersistentIndexes.size() == updatedIndexes.size());
    changePersistentIndexList(oldPersistentIndexes, updatedIndexes);
    oldPersistentIndexes.clear();
    oldPtrs.clear();
}

void ThreadingMsgListModel::pruneTree()
{
    // Our mapping (threading) is completely unsorted, which means that we simply don't have any way of walking the tree from
    // the top. Instead, we got to work with a random walk, processing nodes in an unspecified order.  If we iterated on the QHash
    // directly, we'd hit an issue with iterator ordering (basically, we want to be able to say "hey, I don't care at which point
    // of the iteration I'm right now, the next node to process should be that one, and then we should resume with the rest").
    QList<uint> pending = threading.keys();
    for (QList<uint>::iterator id = pending.begin(); id != pending.end(); /* nothing */) {
        // Convert to the hashmap
        // The "it" iterator point to the current node in the threading mapping
        QHash<uint, ThreadNodeInfo>::iterator it = threading.find(*id);
        if (it == threading.end()) {
            // We've already seen this node, that's due to promoting
            ++id;
            continue;
        }

        if (it->internalId == 0) {
            // A special root item; we should not delete that one :)
            ++id;
            continue;
        }
        if (it->ptr) {
            // regular and valid message -> skip
            ++id;
        } else {
            // a fake one

            // each node has a parent
            QHash<uint, ThreadNodeInfo>::iterator parent = threading.find(it->parent);
            Q_ASSERT(parent != threading.end());

            // and the node itself has to be found in its parent's children
            QList<uint>::iterator childIt = qFind(parent->children.begin(), parent->children.end(), it->internalId);
            Q_ASSERT(childIt != parent->children.end());
            // Check that its offset is correct
            Q_ASSERT(childIt - parent->children.begin() == it->offset);

            if (it->children.isEmpty()) {
                // This is a leaf node, so we can just remove it
                childIt = parent->children.erase(childIt);
                threadedRootIds.removeOne(it->internalId);
                threading.erase(it);
                ++id;

                // Update offsets of all further nodes, siblings to the one we've just deleted
                while (childIt != parent->children.end()) {
                    QHash<uint, ThreadNodeInfo>::iterator sibling = threading.find(*childIt);
                    Q_ASSERT(sibling != threading.end());
                    --sibling->offset;
                    Q_ASSERT(sibling->offset >= 0);
                    ++childIt;
                }
            } else {
                // This node has some children, so we can't just delete it. Instead of that, we promote its first child
                // to replace this node.
                QHash<uint, ThreadNodeInfo>::iterator replaceWith = threading.find(it->children.first());
                Q_ASSERT(replaceWith != threading.end());

                // Make sure that the offsets are still correct
                Q_ASSERT(parent->children[it->offset] == it->internalId);

                // Replace the node
                replaceWith->offset = it->offset;
                *childIt = it->children.first();
                replaceWith->parent = parent->internalId;

                // Now merge the lists of children
                it->children.removeFirst();
                replaceWith->children = replaceWith->children + it->children;

                // Fix parent and offset information of all children of the replacement node
                for (int i = 0; i < replaceWith->children.size(); ++i) {
                    QHash<uint, ThreadNodeInfo>::iterator sibling = threading.find(replaceWith->children[i]);
                    Q_ASSERT(sibling != threading.end());

                    sibling->parent = replaceWith.key();
                    sibling->offset = i;
                }

                if (parent->internalId == 0) {
                    // Update the list of all thread roots
                    QList<uint>::iterator rootIt = qFind(threadedRootIds.begin(), threadedRootIds.end(), it->internalId);
                    if (rootIt != threadedRootIds.end())
                        *rootIt = replaceWith->internalId;
                }

                // Now that all references are gone, remove the original node
                threading.erase(it);

                if (!replaceWith->ptr) {
                    // If the just-promoted item is also a fake one, we'll have to visit it as well. This assignment is safe,
                    // because we've already processed the current item and are completely done with it. The worst which can
                    // happen is that we'll visit the same node twice, which is reasonably acceptable.
                    *id = replaceWith.key();
                }
            }
        }
    }
}

QStringList ThreadingMsgListModel::supportedCapabilities()
{
    return QStringList() << QLatin1String("THREAD=REFS") << QLatin1String("THREAD=REFERENCES") << QLatin1String("THREAD=ORDEREDSUBJECT");
}

QStringList ThreadingMsgListModel::mimeTypes() const
{
    return sourceModel() ? sourceModel()->mimeTypes() : QStringList();
}

QMimeData *ThreadingMsgListModel::mimeData(const QModelIndexList &indexes) const
{
    if (! sourceModel())
        return 0;

    QModelIndexList translated;
    Q_FOREACH(const QModelIndex &idx, indexes) {
        translated << mapToSource(idx);
    }
    return sourceModel()->mimeData(translated);
}

Qt::DropActions ThreadingMsgListModel::supportedDropActions() const
{
    return sourceModel() ? sourceModel()->supportedDropActions() : Qt::DropActions(0);
}

bool ThreadingMsgListModel::threadContainsUnreadMessages(const uint root) const
{
    // FIXME: cache the value somewhere...
    QList<uint> queue;
    queue.append(root);
    while (! queue.isEmpty()) {
        uint current = queue.takeFirst();
        QHash<uint,ThreadNodeInfo>::const_iterator it = threading.constFind(current);
        Q_ASSERT(it != threading.constEnd());
        if (it->ptr) {
            // Because of the delayed delete via pruneTree, we can hit a null pointer here
            TreeItemMessage *message = dynamic_cast<TreeItemMessage *>(it->ptr);
            Q_ASSERT(message);
            if (! message->isMarkedAsRead())
                return true;
        }
        queue.append(it->children);
    }
    return false;
}

/** @short Pass a debugging message to the real Model, if possible

If we don't know what the real model is, just dump it through the qDebug(); that's better than nothing.
*/
void ThreadingMsgListModel::logTrace(const QString &message)
{
    if (!sourceModel()) {
        qDebug() << message;
        return;
    }
    QModelIndex idx = sourceModel()->index(0, 0);
    if (!idx.isValid()) {
        qDebug() << message;
        return;
    }

    // Got to find out the real model and also translate the index to one belonging to a real Model
    Q_ASSERT(idx.model());
    const Model *realModel;
    QModelIndex realIndex;
    Model::realTreeItem(idx, &realModel, &realIndex);
    Q_ASSERT(realModel);
    QModelIndex mailboxIndex = const_cast<Model *>(realModel)->findMailboxForItems(QModelIndexList() << realIndex);
    const_cast<Model *>(realModel)->logTrace(mailboxIndex, Common::LOG_OTHER,
            QString::fromUtf8("ThreadingMsgListModel for %1").arg(mailboxIndex.data(RoleMailboxName).toString()), message);
}

void ThreadingMsgListModel::setUserWantsThreading(bool enable)
{
    m_shallBeThreading = enable;
    if (m_shallBeThreading) {
        wantThreading();
    } else {
        updateNoThreading();
    }
}

bool ThreadingMsgListModel::setUserSearchingSortingPreference(const QStringList &searchConditions, const SortCriterium criterium, const Qt::SortOrder order)
{
    wantThreading(SKIP_SORT_SEARCH);
    return searchSortPreferenceImplementation(searchConditions, criterium, order);
}

/** @short The workhorse behind setUserSearchingSortingPreference() */
bool ThreadingMsgListModel::searchSortPreferenceImplementation(const QStringList &searchConditions, const SortCriterium criterium, const Qt::SortOrder order)
{
    Q_ASSERT(sourceModel());
    if (!sourceModel()->rowCount()) {
        return false;
    }

    const Model *realModel;
    QModelIndex someMessage = sourceModel()->index(0,0);
    QModelIndex realIndex;
    Model::realTreeItem(someMessage, &realModel, &realIndex);
    QModelIndex mailboxIndex = realIndex.parent().parent();

    bool hasDisplaySort = false;
    bool hasSort = false;
    if (realModel->capabilities().contains(QLatin1String("SORT=DISPLAY"))) {
        hasDisplaySort = true;
        hasSort = true;
    } else if (realModel->capabilities().contains(QLatin1String("SORT"))) {
        // just the regular sort
        hasSort = true;
    }

    m_sortReverse = order == Qt::DescendingOrder;
    QStringList sortOptions;
    switch (criterium) {
    case SORT_ARRIVAL:
        sortOptions << QLatin1String("ARRIVAL");
        break;
    case SORT_CC:
        sortOptions << QLatin1String("CC");
        break;
    case SORT_DATE:
        sortOptions << QLatin1String("DATE");
        break;
    case SORT_FROM:
        sortOptions << (hasDisplaySort ? QLatin1String("DISPLAYFROM") : QLatin1String("FROM"));
        break;
    case SORT_SIZE:
        sortOptions << QLatin1String("SIZE");
        break;
    case SORT_SUBJECT:
        sortOptions << QLatin1String("SUBJECT");
        break;
    case SORT_TO:
        sortOptions << (hasDisplaySort ? QLatin1String("DISPLAYTO") : QLatin1String("TO"));
        break;
    case SORT_NONE:
        if (m_sortTask && m_sortTask->isPersistent() &&
                (m_currentSearchConditions != searchConditions || m_currentSortingCriteria != criterium)) {
            // Any change shall result in us killing that sort task
            m_sortTask->cancelSortingUpdates();
        }

        m_currentSortingCriteria = criterium;

        if (searchConditions.isEmpty()) {
            // This operation is special, it will immediately restore the original shape of the mailbox
            m_currentSearchConditions = searchConditions;
            calculateNullSort();
            applySort();
            return true;
        } else if (searchConditions != m_currentSearchConditions || m_searchValidity != RESULT_FRESH) {
            // We have to update our search conditions
            m_sortTask = realModel->m_taskFactory->createSortTask(const_cast<Model *>(realModel), mailboxIndex, searchConditions,
                                                                  QStringList());
            connect(m_sortTask, SIGNAL(sortingAvailable(QList<uint>)), this, SLOT(slotSortingAvailable(QList<uint>)));
            connect(m_sortTask, SIGNAL(sortingFailed()), this, SLOT(slotSortingFailed()));
            connect(m_sortTask, SIGNAL(incrementalSortUpdate(Imap::Responses::ESearch::IncrementalContextData_t)),
                    this, SLOT(slotSortingIncrementalUpdate(Imap::Responses::ESearch::IncrementalContextData_t)));
            m_currentSearchConditions = searchConditions;
            m_searchValidity = RESULT_ASKED;
        } else {
            // A result of SEARCH has just arrived
            Q_ASSERT(m_searchValidity == RESULT_FRESH);
            applySort();
        }

        return true;
    }

    if (!hasSort) {
        // sorting is completely unsupported
        return false;
    }

    Q_ASSERT(!sortOptions.isEmpty());

    if (m_currentSortingCriteria == criterium && m_currentSearchConditions == searchConditions &&
            m_searchValidity != RESULT_INVALIDATED) {
        applySort();
    } else {
        m_currentSearchConditions = searchConditions;
        m_currentSortingCriteria = criterium;
        calculateNullSort();
        applySort();

        if (m_sortTask && m_sortTask->isPersistent())
            m_sortTask->cancelSortingUpdates();

        m_sortTask = realModel->m_taskFactory->createSortTask(const_cast<Model *>(realModel), mailboxIndex, searchConditions, sortOptions);
        connect(m_sortTask, SIGNAL(sortingAvailable(QList<uint>)), this, SLOT(slotSortingAvailable(QList<uint>)));
        connect(m_sortTask, SIGNAL(sortingFailed()), this, SLOT(slotSortingFailed()));
        connect(m_sortTask, SIGNAL(incrementalSortUpdate(Imap::Responses::ESearch::IncrementalContextData_t)),
                this, SLOT(slotSortingIncrementalUpdate(Imap::Responses::ESearch::IncrementalContextData_t)));
        m_searchValidity = RESULT_ASKED;
    }

    return true;
}

void ThreadingMsgListModel::applySort()
{
    if (!sourceModel()->rowCount()) {
        // empty mailbox is a corner case and it's already sorted anyway
        return;
    }

    const Imap::Mailbox::Model *realModel;
    QModelIndex someMessage = sourceModel()->index(0,0);
    QModelIndex realIndex;
    Model::realTreeItem(someMessage, &realModel, &realIndex);
    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox*>(static_cast<TreeItem*>(realIndex.parent().parent().internalPointer()));
    Q_ASSERT(mailbox);

    emit layoutAboutToBeChanged();
    updatePersistentIndexesPhase1();
    QSet<uint> newlyUnreachable(threading[0].children.toSet());
    threading[0].children.clear();
#if QT_VERSION >= 0x040700
    threading[0].children.reserve(m_currentSortResult.size());
#endif

    QSet<uint> allRootIds(threadedRootIds.toSet());

    for (int i = 0; i < m_currentSortResult.size(); ++i) {
        int offset = m_sortReverse ? m_currentSortResult.size() - 1 - i : i;
        QList<TreeItemMessage *> messages = const_cast<Model*>(realModel)
                ->findMessagesByUids(mailbox, QList<uint>() << m_currentSortResult[offset]);
        if (messages.isEmpty()) {
            // wrong UID, weird
            continue;
        }
        Q_ASSERT(messages.size() == 1);
        QHash<void *,uint>::const_iterator it = ptrToInternal.constFind(messages.front());
        Q_ASSERT(it != ptrToInternal.constEnd());
        if (!allRootIds.contains(*it)) {
            // not a thread root, so don't show it
            continue;
        }
        threading[*it].offset = threading[0].children.size();
        threading[0].children.append(*it);
    }

    // Now remove everything which is no longer reachable from the root of the thread mapping
    // Start working on the top-level orphans
    Q_FOREACH(const uint uid, threading[0].children) {
        newlyUnreachable.remove(uid);
    }
    std::vector<uint> queue(newlyUnreachable.constBegin(), newlyUnreachable.constEnd());
    for (std::vector<uint>::size_type i = 0; i < queue.size(); ++i) {
        QHash<uint,ThreadNodeInfo>::iterator threadingIt = threading.find(queue[i]);
        Q_ASSERT(threadingIt != threading.end());
        queue.insert(queue.end(), threadingIt->children.constBegin(), threadingIt->children.constEnd());
        threading.erase(threadingIt);
    }

    updatePersistentIndexesPhase2();
    emit layoutChanged();
}

QStringList ThreadingMsgListModel::currentSearchCondition() const
{
    return m_currentSearchConditions;
}

ThreadingMsgListModel::SortCriterium ThreadingMsgListModel::currentSortCriterium() const
{
    return m_currentSortingCriteria;
}

Qt::SortOrder ThreadingMsgListModel::currentSortOrder() const
{
    return m_sortReverse ? Qt::DescendingOrder : Qt::AscendingOrder;
}

}
}
