/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

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

#include "ThreadingMsgListModel.h"
#include <algorithm>
#include <QBuffer>
#include <QDebug>
#include "ItemRoles.h"
#include "MailboxTree.h"
#include "MsgListModel.h"

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
    ss << prefix << "ThreadNodeInfo " << node.internalId << " " << node.uid << " " << node.ptr << " " << node.parent << "\n";
    Q_FOREACH(const uint childId, node.children) {
        ss << dumpThreadNodeInfo(mapping, childId, offset + 1);
    }
    return res;
}
}

namespace Imap
{
namespace Mailbox
{

ThreadingMsgListModel::ThreadingMsgListModel(QObject *parent):
    QAbstractProxyModel(parent), threadingHelperLastId(0), modelResetInProgress(false), threadingInFlight(false)
{
}

void ThreadingMsgListModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    threading.clear();
    ptrToInternal.clear();
    unknownUids.clear();

    if (this->sourceModel()) {
        // there's already something, so take care to disconnect all signals
        this->sourceModel()->disconnect(this);
    }

    reset();

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

void ThreadingMsgListModel::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    QSet<QPersistentModelIndex>::iterator persistent = unknownUids.find(topLeft);
    if (persistent != unknownUids.end()) {
        // The message wasn't fully synced before, and now it is
        persistent = unknownUids.erase(persistent);
        if (unknownUids.isEmpty()) {
            wantThreading();
        }
        return;
    }

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
        emit dataChanged(rootCandidate, rootCandidate.sibling(rootCandidate.row(), translated.column()));
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
        if (role == RoleThreadRootWithUnreadMessages && ! proxyIndex.parent().isValid()) {
            return threadContainsUnreadMessages(it->internalId);
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
    if (it->ptr)
        return QAbstractProxyModel::flags(index);

    return Qt::NoItemFlags;

}

void ThreadingMsgListModel::handleRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(!parent.isValid());

    for (int i = start; i <= end; ++i) {
        QModelIndex index = sourceModel()->index(i, 0, parent);
        Q_ASSERT(index.isValid());
        uint uid = index.data(Imap::Mailbox::RoleMessageUid).toUInt();
        QModelIndex translated = mapFromSource(index);
        Q_ASSERT(translated.isValid());
        QHash<uint,ThreadNodeInfo>::iterator it = threading.find(translated.internalId());
        Q_ASSERT(it != threading.end());
        it->uid = 0;
        it->ptr = 0;
        // it will get cleaned up by the pruneTree call later on
        if (!uid) {
            // removing message without a UID
            unknownUids.remove(index);
        }
    }
    emit layoutAboutToBeChanged();
    updatePersistentIndexesPhase1();
}

void ThreadingMsgListModel::handleRowsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(!parent.isValid());

    Q_UNUSED(start);
    Q_UNUSED(end);

    // It looks like this simplified approach won't really fly when model starts to issue interleaved rowsRemoved signals,
    // as we'll just remove everything upon first rowsRemoved.  I'll just hope that it doesn't matter (much).

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
            logTrace(QString::fromAscii("Message %1 has unknown UID").arg(index.row()));
            unknownUids << index;
        }
    }
    endInsertRows();

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
    reset();
    updateNoThreading();
    modelResetInProgress = false;

    wantThreading();
}

void ThreadingMsgListModel::updateNoThreading()
{
    if (! threading.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, rowCount() - 1);
        threading.clear();
        ptrToInternal.clear();
        endRemoveRows();
    }
    unknownUids.clear();

    if (! sourceModel()) {
        // Maybe we got reset because the parent model is no longer here...
        return;
    }

    int upstreamMessages = sourceModel()->rowCount();
    QList<uint> allIds;
    QHash<uint,ThreadNodeInfo> newThreading;
    QHash<void *,uint> newPtrToInternal;

    for (int i = 0; i < upstreamMessages; ++i) {
        QModelIndex index = sourceModel()->index(i, 0);
        uint uid = index.data(RoleMessageUid).toUInt();
        ThreadNodeInfo node;
        node.internalId = i + 1;
        node.uid = uid;
        node.ptr = static_cast<TreeItem *>(index.internalPointer());
        node.offset = i;
        newThreading[node.internalId] = node;
        allIds.append(node.internalId);
        newPtrToInternal[node.ptr] = node.internalId;
        if (!node.uid) {
            logTrace(QString::fromAscii("Message %1 has unknown UID").arg(index.row()));
            unknownUids << index;
        }
    }

    if (newThreading.size()) {
        beginInsertRows(QModelIndex(), 0, newThreading.size() - 1);
        threading = newThreading;
        ptrToInternal = newPtrToInternal;
        threading[ 0 ].children = allIds;
        threading[ 0 ].ptr = static_cast<MsgListModel *>(sourceModel())->msgList;
        endInsertRows();
    }
}

void ThreadingMsgListModel::wantThreading()
{
    if (!sourceModel() || !sourceModel()->rowCount()) {
        updateNoThreading();
        return;
    }

    if (threadingInFlight) {
        // Imagine the following scenario:
        // <<< "* 3 EXISTS"
        // Message 2 has unkown UID
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
        return;
    }

    const Imap::Mailbox::Model *realModel;
    QModelIndex someMessage = sourceModel()->index(0,0);
    QModelIndex realIndex;
    Imap::Mailbox::Model::realTreeItem(someMessage, &realModel, &realIndex);
    QModelIndex mailbox = realIndex.parent().parent();

    // Something has happened and we want to process the THREAD response
    QVector<Imap::Responses::ThreadingNode> mapping = realModel->cache()->messageThreading(mailbox.data(RoleMailboxName).toString());

    // Find the UID of the last message in the mailbox
    uint highestUidInMailbox = 0;
    for (int i = sourceModel()->rowCount(); i != -1 && !highestUidInMailbox; --i) {
        highestUidInMailbox = sourceModel()->data(sourceModel()->index(i, 0, QModelIndex()), RoleMessageUid).toUInt();
    }

    uint highestUidInThreadingLowerBound = findHighEnoughNumber(mapping, highestUidInMailbox);

    logTrace(QString::fromAscii("ThreadingMsgListModel::wantThreading: THREAD contains info about UID %1 (or higher), mailbox has %2")
             .arg(QString::number(highestUidInThreadingLowerBound), QString::number(highestUidInMailbox)));

    if (highestUidInThreadingLowerBound >= highestUidInMailbox) {
        // There's no point asking for data at this point, we shall just apply threading
        applyThreading(mapping);
    } else {
        // There's apparently at least one known UID whose threading info we do not know; that means that we have to ask the
        // server here.
        askForThreading();
    }
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

void ThreadingMsgListModel::askForThreading()
{
    Q_ASSERT(sourceModel());
    Q_ASSERT(sourceModel()->rowCount());

    const Imap::Mailbox::Model *realModel;
    QModelIndex someMessage = sourceModel()->index(0,0);
    QModelIndex realIndex;
    Imap::Mailbox::Model::realTreeItem(someMessage, &realModel, &realIndex);
    QModelIndex mailboxIndex = realIndex.parent().parent();

    if (realModel->capabilities().contains(QLatin1String("THREAD=REFS"))) {
        requestedAlgorithm = QLatin1String("REFS");
    } else if (realModel->capabilities().contains(QLatin1String("THREAD=REFERENCES"))) {
        requestedAlgorithm = QLatin1String("REFERENCES");
    } else if (realModel->capabilities().contains(QLatin1String("THREAD=ORDEREDSUBJECT"))) {
        requestedAlgorithm = QLatin1String("ORDEREDSUBJECT");
    }

    if (! requestedAlgorithm.isEmpty()) {
        threadingInFlight = true;
        realModel->m_taskFactory->createThreadTask(const_cast<Imap::Mailbox::Model *>(realModel),
                mailboxIndex, requestedAlgorithm,
                QStringList() << QLatin1String("ALL"));
        connect(realModel, SIGNAL(threadingAvailable(QModelIndex,QString,QStringList,QVector<Imap::Responses::ThreadingNode>)),
                this, SLOT(slotThreadingAvailable(QModelIndex,QString,QStringList,QVector<Imap::Responses::ThreadingNode>)));
        connect(realModel, SIGNAL(threadingFailed(QModelIndex,QString,QStringList)), this, SLOT(slotThreadingFailed(QModelIndex,QString,QStringList)));
    }
}

bool ThreadingMsgListModel::shouldIgnoreThisThreadingResponse(const QModelIndex &mailbox, const QString &algorithm,
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
        logTrace(QString::fromAscii("Weird, asked for threading via %1 but got %2 instead -- ignoring")
                 .arg(requestedAlgorithm, algorithm));
        return true;
    }

    if (searchCriteria.size() != 1 || searchCriteria.front() != QLatin1String("ALL")) {
        QString buf;
        QTextStream ss(&buf);
        logTrace(QString::fromAscii("Weird, requesting messages matching ALL, but got this instead: %1")
                 .arg(searchCriteria.join(QString::fromAscii(", "))));
        return true;
    }

    if (realModel)
        *realModel = model;
    return false;
}

void ThreadingMsgListModel::slotThreadingFailed(const QModelIndex &mailbox, const QString &algorithm, const QStringList &searchCriteria)
{
    // Better safe than sorry -- prevent infinite waiting to the maximal possible extent
    threadingInFlight = false;

    if (shouldIgnoreThisThreadingResponse(mailbox, algorithm, searchCriteria))
        return;

    disconnect(sender(), 0, this,
               SLOT(slotThreadingAvailable(QModelIndex,QString,QStringList,QVector<Imap::Responses::ThreadingNode>)));
    disconnect(sender(), 0, this,
               SLOT(slotThreadingFailed(QModelIndex,QString,QStringList)));

    updateNoThreading();
}

void ThreadingMsgListModel::slotThreadingAvailable(const QModelIndex &mailbox, const QString &algorithm,
        const QStringList &searchCriteria,
        const QVector<Imap::Responses::ThreadingNode> &mapping)
{
    // Better safe than sorry -- prevent infinite waiting to the maximal possible extent
    threadingInFlight = false;

    const Model *model = 0;
    if (shouldIgnoreThisThreadingResponse(mailbox, algorithm, searchCriteria, &model))
        return;

    disconnect(sender(), 0, this,
               SLOT(slotThreadingAvailable(QModelIndex,QString,QStringList,QVector<Imap::Responses::ThreadingNode>)));
    disconnect(sender(), 0, this,
               SLOT(slotThreadingFailed(QModelIndex,QString,QStringList)));

    model->cache()->setMessageThreading(mailbox.data(RoleMailboxName).toString(), mapping);

    // Indirect processing here -- the wantThreading() will check that the received response really contains everything we need
    // and if it does, simply applyThreading() that.  If there's something missing, it will ask for the threading again.
    wantThreading();
}

void ThreadingMsgListModel::applyThreading(const QVector<Imap::Responses::ThreadingNode> &mapping)
{
    if (! unknownUids.isEmpty()) {
        // Some messages have UID zero, which means that they weren't loaded yet. Too bad.
        logTrace(QString::fromAscii("%1 messages have 0 UID").arg(unknownUids.size()));
        return;
    }

    emit layoutAboutToBeChanged();

    updatePersistentIndexesPhase1();

    threading.clear();
    ptrToInternal.clear();
    // Default-construct the root node
    threading[ 0 ].ptr = static_cast<MsgListModel *>(sourceModel())->msgList;

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
    emit layoutChanged();
}

void ThreadingMsgListModel::registerThreading(const QVector<Imap::Responses::ThreadingNode> &mapping, uint parentId, const QHash<uint,void *> &uidToPtr, QSet<uint> &usedNodes)
{
    Q_FOREACH(const Imap::Responses::ThreadingNode &node, mapping) {
        uint nodeId;
        if (node.num == 0) {
            ThreadNodeInfo fake;
            fake.internalId = ++threadingHelperLastId;
            fake.parent = parentId;
            Q_ASSERT(threading.contains(parentId));
            // The child will be registered to the list of parent's children after the if/else branch
            threading[ fake.internalId ] = fake;
            nodeId = fake.internalId;
        } else {
            QHash<uint,void *>::const_iterator ptrIt = uidToPtr.find(node.num);
            if (ptrIt == uidToPtr.constEnd()) {
                logTrace(QString::fromAscii("The THREAD response references a message with UID %1, which is not recognized "
                                            "at this point. More information is available in the IMAP protocol log.")
                         .arg(node.num));
                // It's possible that the THREAD response came from cache; in that case, it isn't pretty, but completely harmless
                // FIXME: it'd be great to be able to distinguish between data sent by the IMAP server and a stale cache...
                continue;
            }

            QHash<void *,uint>::const_iterator nodeIt = ptrToInternal.constFind(*ptrIt);
            // The following assert would fail if there was a node with a valid UID, but not in our ptrToInternal mapping.
            // That is however non-issue, as we pre-create nodes for all messages beforehand.
            Q_ASSERT(nodeIt != ptrToInternal.constEnd());
            nodeId = *nodeIt;
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
        Q_ASSERT(it != threading.constEnd());
        updatedIndexes.append(createIndex(it->offset, oldPersistentIndexes[i].column(), it->internalId));
    }
    Q_ASSERT(oldPersistentIndexes.size() == updatedIndexes.size());
    changePersistentIndexList(oldPersistentIndexes, updatedIndexes);
    oldPersistentIndexes.clear();
    oldPtrs.clear();
}

void ThreadingMsgListModel::pruneTree()
{
    // Our mapping (_threading) is completely unsorted, which means that we simply don't have any way of walking the tree from
    // the top. Instead, we got to work with a random walk, processing nodes in an unspecified order.  If we iterated on the QMap
    // directly, we'd hit an issue with iterator ordering (basically, we want to be able to say "hey, I don't care at which point
    // of the iteration I'm right now, the next node to process should be this one, and then we should resume with the rest").
    QList<uint> pending = threading.keys();
    for (QList<uint>::iterator id = pending.begin(); id != pending.end(); /* nothing */) {
        // Convert to the hashmap
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

            if (it->children.isEmpty()) {
                // this is a leaf node, so we can just remove it
                parent->children.erase(childIt);
                threading.erase(it);
                ++id;
            } else {
                // This node has some children, so we can't just delete it. Instead of that, we promote its first child
                // to replace this node.
                QHash<uint, ThreadNodeInfo>::iterator replaceWith = threading.find(it->children.first());
                Q_ASSERT(replaceWith != threading.end());

                replaceWith->offset = it->offset;
                *childIt = it->children.first();
                replaceWith->parent = parent->internalId;

                // set parent of all siblings of the just promoted node to the promoted node, and list them as children
                it->children.removeFirst();
                Q_FOREACH(const uint childId, it->children) {
                    QHash<uint, ThreadNodeInfo>::iterator sibling = threading.find(childId);
                    Q_ASSERT(sibling != threading.end());
                    sibling->parent = replaceWith.key();
                    replaceWith->children.append(sibling.key());
                    // We've moved the first of them one level up, so we got to adjust their offsets
                    --sibling->offset;
                }

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
        Q_ASSERT(it->ptr);
        TreeItemMessage *message = dynamic_cast<TreeItemMessage *>(it->ptr);
        Q_ASSERT(message);
        if (! message->isMarkedAsRead())
            return true;
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
    const_cast<Model *>(realModel)->logTrace(mailboxIndex, LOG_OTHER,
            QString::fromAscii("ThreadingMsgListModel for %1").arg(mailboxIndex.data(RoleMailboxName).toString()), message);
}

}
}
