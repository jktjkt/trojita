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

#ifndef IMAP_THREADINGMSGLISTMODEL_H
#define IMAP_THREADINGMSGLISTMODEL_H

#include <QAbstractProxyModel>
#include <QPointer>
#include <QSet>
#include "Imap/Parser/Response.h"

class QTimer;
class ImapModelThreadingTest;

/** @short Namespace for IMAP interaction */
namespace Imap
{

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox
{

class SortTask;
class TreeItem;
class TreeItemMsgList;

/** @short A node in tree structure used for threading representation */
struct ThreadNodeInfo {
    /** @short Internal unique identifier used for model indexes */
    uint internalId;
    /** @short A UID of the message in a mailbox */
    uint uid;
    /** @short internalId of a parent of this message */
    uint parent;
    /** @short List of children of current node */
    QList<uint> children;
    /** @short Pointer to the TreeItemMessage* of the corresponding message */
    TreeItem *ptr;
    /** @short Position among our parent's children */
    int offset;
    ThreadNodeInfo(): internalId(0), uid(0), parent(0), ptr(0), offset(0) {}
};

QDebug operator<<(QDebug debug, const ThreadNodeInfo &node);

/** @short A model implementing view of the whole IMAP server

The problem with threading is that due to the extremely asynchronous nature of the IMAP Model, we often get informed about indexes
to messages which "just arrived", and therefore do not have even their UID available. That sucks, because we have to somehow handle
them.  Situation gets a bit more complicated by the initial syncing -- this ThreadingMsgListModel can't tell whether the rowsInserted()
signals mean that the underlying model is getting populated, or whether it's a sign of a just-arrived message.  On a plus side, the Model
guarantees that the only occurrence when a message could have UID 0 is when the mailbox has been synced previously, and the message is a new
arrival.  In all other contexts (that is, during the mailbox re-synchronization), there is a hard guarantee that the UID of any message
available via the MVC API will always be non-zero.

The model should also refrain from sending extra THREAD commands to the server, and cache the responses locally.  This is pretty easy for
message deletions, as it should be only a matter of replacing some node in the threading info with a fake ThreadNodeInfo node and running
the pruneTree() method, except that we might not know the UID of the message in question, and hence can't know what to delete.

*/
class ThreadingMsgListModel: public QAbstractProxyModel
{
    Q_OBJECT

public:

    /** @short On which column to sort

    The possible columns are described in RFC 5256, section 3.  No support for multiple columns is present.

    Trojitá will automatically upgrade to the display-based search criteria from RFC 5957 if support for that RFC is indicated by
    the server.
    */
    typedef enum {
        /** @short Don't do any explicit sorting

        If threading is not active, the order of messages represnets the order in which they appear in the IMAP mailbox.
        In case the display is threaded already, the order depends on the threading algorithm.
        */
        SORT_NONE,

        /** @short RFC5256's ARRIVAL key, ie. the INTERNALDATE */
        SORT_ARRIVAL,

        /** @short The Cc field from the IMAP ENVELOPE */
        SORT_CC,

        /** @short Timestamp when the message was created, if available */
        SORT_DATE,

        /** @short Either the display name or the mailbox of the "sender" of a message from the "From" header */
        SORT_FROM,

        /** @short Size of the message */
        SORT_SIZE,

        /** @short The subject of the e-mail */
        SORT_SUBJECT,

        /** @short Recipient of the message, either their mailbox or their display name */
        SORT_TO
    } SortCriterium;

    explicit ThreadingMsgListModel(QObject *parent);
    virtual void setSourceModel(QAbstractItemModel *sourceModel);

    virtual QModelIndex index(int row, int column, const QModelIndex &parent=QModelIndex()) const;
    virtual QModelIndex parent(const QModelIndex &index) const;
    virtual int rowCount(const QModelIndex &parent=QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent=QModelIndex()) const;
    virtual QModelIndex mapToSource(const QModelIndex &proxyIndex) const;
    virtual QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;
    virtual bool hasChildren(const QModelIndex &parent=QModelIndex()) const;
    virtual QVariant data(const QModelIndex &proxyIndex, int role) const;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    virtual QStringList mimeTypes() const;
    virtual QMimeData *mimeData(const QModelIndexList &indexes) const;
    virtual Qt::DropActions supportedDropActions() const;

    /** @short List of capabilities which could be used for threading

    If any of them are present in server's capabilities, at least some level of threading will be possible.
    */
    static QStringList supportedCapabilities();

    QStringList currentSearchCondition() const;
    SortCriterium currentSortCriterium() const;
    Qt::SortOrder currentSortOrder() const;

public slots:
    void resetMe();
    void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void handleRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end);
    void handleRowsRemoved(const QModelIndex &parent, int start, int end);
    void handleRowsAboutToBeInserted(const QModelIndex &parent, int start, int end);
    void handleRowsInserted(const QModelIndex &parent, int start, int end);
    /** @short Feed this with the data from a THREAD response */
    void slotThreadingAvailable(const QModelIndex &mailbox, const QByteArray &algorithm, const QStringList &searchCriteria,
                                const QVector<Imap::Responses::ThreadingNode> &mapping);
    void slotThreadingFailed(const QModelIndex &mailbox, const QByteArray &algorithm, const QStringList &searchCriteria);
    /** @short Really apply threading to this model */
    void applyThreading(const QVector<Imap::Responses::ThreadingNode> &mapping);

    /** @short SORT response has arrived */
    void slotSortingAvailable(const QList<uint> &uids);

    /** @short SORT has failed */
    void slotSortingFailed();

    /** @short Dynamic update to the current SORT order */
    void slotSortingIncrementalUpdate(const Imap::Responses::ESearch::IncrementalContextData_t &updates);

    void applySort();

    /** @short Enable or disable threading */
    void setUserWantsThreading(bool enable);

    bool setUserSearchingSortingPreference(const QStringList &searchConditions, const SortCriterium criterium,
                                           const Qt::SortOrder order = Qt::AscendingOrder);

    void slotIncrementalThreadingAvailable(const Responses::ESearch::IncrementalThreadingData_t &data);
    void slotIncrementalThreadingFailed();

    void delayedPrune();

signals:
    void sortingFailed();

private:
    /** @short Display messages without any threading at all, as a liner list */
    void updateNoThreading();

    /** @short Ask the model for a THREAD response

    If the firstUnknownUid is different than zero, an incremental response is requested.
    */
    void askForThreading(const uint firstUnknownUid = 0);

    void updatePersistentIndexesPhase1();
    void updatePersistentIndexesPhase2();

    /** @short Shall we ask for SORT/SEARCH automatically? */
    typedef enum {
        AUTO_SORT_SEARCH,
        SKIP_SORT_SEARCH
    } SkipSortSearch;

    /** @short Apply cached THREAD response or ask for threading again */
    void wantThreading(const SkipSortSearch skipSortSearch = AUTO_SORT_SEARCH);

    /** @short Convert the threading from a THREAD response and apply that threading to this model */
    void registerThreading(const QVector<Imap::Responses::ThreadingNode> &mapping, uint parentId,
                           const QHash<uint,void *> &uidToPtr, QSet<uint> &usedNodes);

    bool searchSortPreferenceImplementation(const QStringList &searchConditions, const SortCriterium criterium,
                                            const Qt::SortOrder order = Qt::AscendingOrder);

    /** @short Remove fake messages from the threading tree */
    void pruneTree();

    /** @short Check current thread for "unread messages" */
    bool threadContainsUnreadMessages(const uint root) const;

    /** @short Is this someone else's THREAD response? */
    bool shouldIgnoreThisThreadingResponse(const QModelIndex &mailbox, const QByteArray &algorithm,
                                           const QStringList &searchCriteria, const Model **realModel=0);

    /** @short Return some number from the thread mapping @arg mapping which is either the highest among them, or at least as high as the marker*/
    static uint findHighEnoughNumber(const QVector<Imap::Responses::ThreadingNode> &mapping, uint marker);

    void calculateNullSort();

    uint findHighestUidInMailbox(TreeItemMsgList *list);

    void logTrace(const QString &message);


    ThreadingMsgListModel &operator=(const ThreadingMsgListModel &);  // don't implement
    ThreadingMsgListModel(const ThreadingMsgListModel &);  // don't implement

    /** @short Mapping from the upstream model's internalId to ThreadingMsgListModel's internal IDs */
    QHash<void *,uint> ptrToInternal;

    /** @short Tree for the threading

    This tree is indexed by our internal ID.
    */
    QHash<uint,ThreadNodeInfo> threading;

    /** @short Last assigned internal ID */
    uint threadingHelperLastId;

    /** @short Messages with unknown UIDs */
    QSet<TreeItem*> unknownUids;

    /** @short Threading algorithm we're using for this request */
    QByteArray requestedAlgorithm;

    /** @short Recursion guard for "is the model currently being reset?"

    We can't be sure what happens when we call rowCount() from updateNoThreading(). It is
    possible that the rowCount() would propagate to Model's askForMessagesInMailbox(),
    which could in turn call beginInsertRows, leading to a possible recursion.
    */
    bool modelResetInProgress;

    QModelIndexList oldPersistentIndexes;
    QList<void *> oldPtrs;

    /** @short There's a pending THREAD command for which we haven't received data yet */
    bool threadingInFlight;

    /** @short Is threading enabled, or shall we just use other features like sorting and filtering? */
    bool m_shallBeThreading;

    /** @short Task handling the SORT command */
    QPointer<SortTask> m_sortTask;

    /** @short Shall we sort in a reversed order? */
    bool m_sortReverse;

    /** @short IDs of all thread roots when no sorting or filtering is applied */
    QList<uint> threadedRootIds;

    /** @short Sorting criteria of the current copy of the sort result */
    SortCriterium m_currentSortingCriteria;

    /** @short Search criteria of the current copy of the search/sort result */
    QStringList m_currentSearchConditions;

    /** @short The current result of the SORT operation

    This variable holds the UIDs of all messages in this mailbox, sorted according to the current sorting criteria.
    */
    QList<uint> m_currentSortResult;

    /** @short Is the cached result of SEARCH/SORT fresh enough? */
    typedef enum {
        RESULT_ASKED, /**< We've asked for the data */
        RESULT_FRESH, /**< The response has just arrived and didn't get invalidated since then */
        RESULT_INVALIDATED /**< A new message has arrived, rendering our copy invalid */
    } ResultValidity;

    ResultValidity m_searchValidity;

    QTimer *m_delayedPrune;

    friend class ::ImapModelThreadingTest; // needs access to wantThreading();
};

}

}

#endif /* IMAP_THREADINGMSGLISTMODEL_H */
