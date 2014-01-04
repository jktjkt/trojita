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

#include "SortTask.h"
#include <algorithm>
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/TaskPresentationModel.h"
#include "KeepMailboxOpenTask.h"

namespace Imap
{
namespace Mailbox
{


SortTask::SortTask(Model *model, const QModelIndex &mailbox, const QStringList &searchConditions, const QStringList &sortCriteria):
    ImapTask(model), mailboxIndex(mailbox), searchConditions(searchConditions), sortCriteria(sortCriteria),
    m_persistentSearch(false), m_firstUntaggedReceived(false), m_firstCommandCompleted(false)
{
    conn = model->findTaskResponsibleFor(mailbox);
    conn->addDependentTask(this);
    if (searchConditions.isEmpty())
        this->searchConditions << QLatin1String("ALL");
}

void SortTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    if (! mailboxIndex.isValid()) {
        _failed("Mailbox vanished before we could ask for threading info");
        return;
    }

    // We can be killed when appropriate
    KeepMailboxOpenTask *keepTask = dynamic_cast<KeepMailboxOpenTask*>(conn);
    Q_ASSERT(keepTask);
    keepTask->feelFreeToAbortCaller(this);

    if (sortCriteria.isEmpty()) {
        if (model->accessParser(parser).capabilitiesFresh &&
                model->accessParser(parser).capabilities.contains(QLatin1String("ESEARCH"))) {
            // We always prefer ESEARCH over SEARCH, if only for its embedded reference to the command tag
            if (model->accessParser(parser).capabilities.contains(QLatin1String("CONTEXT=SEARCH"))) {
                // Hurray, this IMAP server supports incremental ESEARCH updates
                m_persistentSearch = true;
                sortTag = parser->uidESearch("utf-8", searchConditions,
                                             QStringList() << QLatin1String("ALL") << QLatin1String("UPDATE"));
            } else {
                // ESORT without CONTEXT is still worth the effort, if only for the tag reference
                sortTag = parser->uidESearch("utf-8", searchConditions, QStringList() << QLatin1String("ALL"));
            }
        } else {
            // Plain "old" SORT
            sortTag = parser->uidSearch(searchConditions, "utf-8");
        }
    } else {
        // SEARCH and SORT combined
        if (model->accessParser(parser).capabilitiesFresh &&
                model->accessParser(parser).capabilities.contains(QLatin1String("ESORT"))) {
            // ESORT's better than regular SORT, if only for its embedded reference to the command tag
            if (model->accessParser(parser).capabilities.contains(QLatin1String("CONTEXT=SORT"))) {
                // Hurray, this IMAP server supports incremental SORT updates
                m_persistentSearch = true;
                sortTag = parser->uidESort(sortCriteria, "utf-8", searchConditions,
                                       QStringList() << QLatin1String("ALL") << QLatin1String("UPDATE"));
            } else {
                // ESORT without CONTEXT is still worth the effort, if only for the tag reference
                sortTag = parser->uidESort(sortCriteria, "utf-8", searchConditions, QStringList() << QLatin1String("ALL"));
            }
        } else {
            // Plain "old" SORT
            sortTag = parser->uidSort(sortCriteria, "utf-8", searchConditions);
        }
    }
}

bool SortTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty()) {
        if (resp->kind == Responses::NO && resp->respCode == Responses::NOUPDATE) {
            // * NO [NOUPDATE "tag"] means that the server won't be providing further updates for our SEARCH/SORT criteria
            const Responses::RespData<QString> *const untaggedTag = dynamic_cast<const Responses::RespData<QString>* const>(
                        resp->respCodeData.data());
            Q_ASSERT(untaggedTag);
            if (untaggedTag->data == sortTag) {
                m_persistentSearch = false;
                model->m_taskModel->slotTaskMighHaveChanged(this);

                if (m_firstCommandCompleted) {
                    // The server decided that it will no longer inform us about the updated SORT order, and the original
                    // response has been already received and processed. That means that we're done here and shall declare
                    // ourselves as completed.
                    _completed();
                }
                // We actually support even more benevolent mode of operation where the server can tell us at any time that
                // this context updating is no longer supported. Yay for that; let's hope that it's reasonably bug-free now.
                return true;
            }
        }
        return false;
    }

    if (resp->tag == sortTag) {
        m_firstCommandCompleted = true;
        if (resp->kind == Responses::OK) {
            emit sortingAvailable(sortResult);
            if (!m_persistentSearch || _aborted) {
                // This is a one-shot operation, we shall not remain as an active task, listening for further updates
                _completed();
            } else {
                // got to prod the TaskPresentationModel
                model->m_taskModel->slotTaskMighHaveChanged(this);

                // Even though we aren't "finished" at this point, the KeepMailboxOpenTask is now free to issue its IDLE thing,
                // as that won't interfere with our mode of operation. Let's kick it around.
                KeepMailboxOpenTask *keepTask = dynamic_cast<KeepMailboxOpenTask*>(conn);
                Q_ASSERT(keepTask);
                keepTask->activateTasks();
            }
        } else {
            _failed("Sorting command has failed");
        }
        return true;
    } else if (resp->tag == cancelUpdateTag) {
        m_persistentSearch = false;
        model->m_taskModel->slotTaskMighHaveChanged(this);
        _completed();
        return true;
    } else {
        return false;
    }
}

bool SortTask::handleSort(const Imap::Responses::Sort *const resp)
{
    sortResult = resp->numbers;
    return true;
}

bool SortTask::handleSearch(const Imap::Responses::Search *const resp)
{
    if (searchConditions == QStringList() << QLatin1String("ALL")) {
        // We're really a SORT task, so we shouldn't process this stuff
        return false;
    }

    // The actual data for the SEARCH response can be split into several responses.
    // Possible performance optimization might be to call sort & unique only after receiving the tagged OK,
    // but that'd also mean that one has to keep track of whether we're doing a SORT or SEARCH from there.
    // That just doesn't look like worth it.

    sortResult += resp->items;
    qSort(sortResult);
    sortResult.erase(std::unique(sortResult.begin(), sortResult.end()), sortResult.end());
    return true;
}

bool SortTask::handleESearch(const Responses::ESearch *const resp)
{
    if (resp->tag != sortTag)
        return false;

    if (resp->seqOrUids != Imap::Responses::ESearch::UIDS)
        throw UnexpectedResponseReceived("ESEARCH response to a UID SORT command with matching tag uses "
                                         "sequence numbers instead of UIDs", *resp);

    Responses::ESearch::CompareListDataIdentifier<Responses::ESearch::ListData_t> allComparator("ALL");
    Responses::ESearch::ListData_t::const_iterator allIterator =
            std::find_if(resp->listData.constBegin(), resp->listData.constEnd(), allComparator);

    if (allIterator != resp->listData.constEnd()) {
        m_firstUntaggedReceived = true;
        sortResult = allIterator->second;

        ++allIterator;
        if (std::find_if(allIterator, resp->listData.constEnd(), allComparator) != resp->listData.constEnd())
            throw UnexpectedResponseReceived("ESEARCH contains the ALL key too many times", *resp);

        if (!resp->incrementalContextData.isEmpty())
            throw UnexpectedResponseReceived("ESEARCH contains both ALL result set and some incremental updates", *resp);

        return true;
    }

    Q_ASSERT(allIterator == resp->listData.constEnd());

    if (resp->incrementalContextData.isEmpty()) {
        sortResult.clear();
        // This means that there have been no matches
        // FIXME: cover this in the test suite!
        return true;
    }

    Q_ASSERT(!resp->incrementalContextData.isEmpty());

    if (!m_persistentSearch)
        throw UnexpectedResponseReceived("ESEARCH contains incremental responses even though we haven't requested that", *resp);

    emit incrementalSortUpdate(resp->incrementalContextData);

    return true;
}

QVariant SortTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Sorting mailbox")) : QVariant();
}

void SortTask::_failed(const QString &errorMessage)
{
    // FIXME: show this in the GUI
    emit sortingFailed();
    ImapTask::_failed(errorMessage);
}

bool SortTask::isPersistent() const
{
    return m_persistentSearch;
}

/** @short Return true if this task has already done its job and is now merely listening for further updates */
bool SortTask::isJustUpdatingNow() const
{
    return isPersistent() && m_firstCommandCompleted && !_aborted;
}

void SortTask::cancelSortingUpdates()
{
    Q_ASSERT(m_persistentSearch);
    Q_ASSERT(!sortTag.isEmpty());
    KeepMailboxOpenTask *keepTask = dynamic_cast<KeepMailboxOpenTask*>(conn);
    Q_ASSERT(keepTask);
    keepTask->breakOrCancelPossibleIdle();
    cancelUpdateTag = parser->cancelUpdate(sortTag);
}

void SortTask::abort()
{
    if (cancelUpdateTag.isEmpty() && isJustUpdatingNow()) {
        cancelSortingUpdates();
    }
    ImapTask::abort();
}

}
}
