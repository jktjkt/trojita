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

#ifndef IMAP_SORT_TASK_H
#define IMAP_SORT_TASK_H

#include <QPersistentModelIndex>
#include "ImapTask.h"

namespace Imap
{
namespace Mailbox
{

/** @short Send a SORT command and take care of its processing */
class SortTask : public ImapTask
{
    Q_OBJECT
public:
    SortTask(Model *model, const QModelIndex &mailbox, const QStringList &searchConditions, const QStringList &sortCriteria);
    virtual void perform();
    virtual void abort();

    virtual bool handleStateHelper(const Imap::Responses::State *const resp);
    virtual bool handleSort(const Imap::Responses::Sort *const resp);
    virtual bool handleSearch(const Imap::Responses::Search *const resp);
    virtual bool handleESearch(const Responses::ESearch *const resp);
    virtual QVariant taskData(const int role) const;
    virtual bool needsMailbox() const {return true;}

    bool isPersistent() const;
    bool isJustUpdatingNow() const;

    void cancelSortingUpdates();

signals:
    /** @short Sort result has arrived */
    void sortingAvailable(const QList<uint> &uids);

    /** @short Sort operation has failed */
    void sortingFailed();

    /** @short An incremental update to the sorting criteria according to CONTEXT=SORT */
    void incrementalSortUpdate(const Imap::Responses::ESearch::IncrementalContextData_t &updates);

protected:
    virtual void _failed(const QString &errorMessage);
private:
    CommandHandle sortTag;
    CommandHandle cancelUpdateTag;
    ImapTask *conn;
    QPersistentModelIndex mailboxIndex;
    QStringList searchConditions;
    QStringList sortCriteria;
    QList<uint> sortResult;

    /** @short Are we supposed to run in a "persistent mode", ie. keep listening for updates? */
    bool m_persistentSearch;

    /** @short Have we received a full (non-incremental) response at first? */
    bool m_firstUntaggedReceived;

    /** @short Did the first command (the ESEARCH/ESORT) finish properly, including its tagged response? */
    bool m_firstCommandCompleted;
};

}
}

#endif // IMAP_SORT_TASK_H
