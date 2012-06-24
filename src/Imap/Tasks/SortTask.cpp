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


#include "SortTask.h"
#include "ItemRoles.h"
#include "KeepMailboxOpenTask.h"
#include "Model.h"
#include "MailboxTree.h"

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

    if (model->accessParser(parser).capabilitiesFresh &&
            model->accessParser(parser).capabilities.contains(QLatin1String("ESORT"))) {
        // ESORT's better than regular SORT, if only for its embedded reference to the command tag
        if (model->accessParser(parser).capabilities.contains(QLatin1String("CONTEXT=SORT"))) {
            // Hurray, this IMAP server supports incremental SORT updates
            m_persistentSearch = true;
            sortTag = parser->uidESort(sortCriteria, QLatin1String("utf-8"), QStringList() << QLatin1String("ALL"),
                                   QStringList() << QLatin1String("ALL") << QLatin1String("UPDATE"));
        } else {
            // ESORT without CONTEXT is still worth the effort, if only for the tag reference
            sortTag = parser->uidESort(sortCriteria, QLatin1String("utf-8"), QStringList() << QLatin1String("ALL"), QStringList());
        }
    } else {
        // Plain "old" SORT
        sortTag = parser->uidSort(sortCriteria, QLatin1String("utf-8"), QStringList() << QLatin1String("ALL"));
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
                emit persistentSortAborted();

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
            if (!m_persistentSearch) {
                // This is a one-shot operation, we shall remain as an acitve task, listening for further updates
                _completed();
            }
        } else {
            _failed("Sorting command has failed");
        }
        return true;
    } else if (resp->tag == cancelUpdateTag) {
        m_persistentSearch = false;
        emit persistentSortAborted();
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
        throw UnexpectedResponseReceived("ESEARCH response to UID SORT doesn't contain any of the ALL, ADDTO or REMOVEFROM data",
                                         *resp);
    }

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

void SortTask::cancelSortingUpdates()
{
    Q_ASSERT(m_persistentSearch);
    Q_ASSERT(!sortTag.isEmpty());
    cancelUpdateTag = parser->cancelUpdate(sortTag);
}

}
}
