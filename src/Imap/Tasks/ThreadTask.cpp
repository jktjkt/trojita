/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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


#include "ThreadTask.h"
#include "ItemRoles.h"
#include "KeepMailboxOpenTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap
{
namespace Mailbox
{


ThreadTask::ThreadTask(Model *model, const QModelIndex &mailbox, const QByteArray &algorithm, const QStringList &searchCriteria,
                       const ThreadingIncrementalMode incrementalMode):
    ImapTask(model), mailboxIndex(mailbox), algorithm(algorithm), searchCriteria(searchCriteria),
    m_incrementalMode(incrementalMode == THREADING_INCREMENTAL)
{
    conn = model->findTaskResponsibleFor(mailbox);
    conn->addDependentTask(this);
}

void ThreadTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    if (! mailboxIndex.isValid()) {
        _failed("Mailbox vanished before we could ask for threading info");
        return;
    }

    tag = parser->uidThread(algorithm, "utf-8", searchCriteria);
}

bool ThreadTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {
        if (resp->kind == Responses::OK) {
            if (!m_incrementalMode)
                emit model->threadingAvailable(mailboxIndex, algorithm, searchCriteria, mapping);
            _completed();
        } else {
            _failed("Threading command has failed");
        }
        mapping.clear();
        return true;
    } else {
        return false;
    }
}

bool ThreadTask::handleThread(const Imap::Responses::Thread *const resp)
{
    mapping = resp->rootItems;
    return true;
}

bool ThreadTask::handleESearch(const Responses::ESearch *const resp)
{
    if (resp->tag != tag)
        return false;

    if (resp->seqOrUids != Imap::Responses::ESearch::UIDS)
        throw UnexpectedResponseReceived("ESEARCH response to a UID THREAD command with matching tag uses "
                                         "sequence numbers instead of UIDs", *resp);

    // Extract the threading information and make sure that it's present at most once
    Responses::ESearch::CompareListDataIdentifier<Responses::ESearch::ThreadingData_t> threadComparator("THREAD");
    Responses::ESearch::ThreadingData_t::const_iterator threadIterator =
            std::find_if(resp->threadingData.constBegin(), resp->threadingData.constEnd(), threadComparator);
    if (threadIterator != resp->threadingData.constEnd()) {
        mapping = threadIterator->second;
        ++threadIterator;
        if (std::find_if(threadIterator, resp->threadingData.constEnd(), threadComparator) != resp->threadingData.constEnd())
            throw UnexpectedResponseReceived("ESEARCH contains the THREAD key too many times", *resp);
    } else {
        mapping.clear();
    }

    if (m_incrementalMode) {
        Responses::ESearch::CompareListDataIdentifier<Responses::ESearch::ListData_t> prevRootComparator("THREADPREVIOUS");
        Responses::ESearch::ListData_t::const_iterator prevRootIterator =
                std::find_if(resp->listData.constBegin(), resp->listData.constEnd(), prevRootComparator);
        if (prevRootIterator == resp->listData.constEnd()) {
            throw UnexpectedResponseReceived("ESEARCH containes THREAD, but no THREADPREVIOUS");
        } else {
            if (std::find_if(prevRootIterator + 1, resp->listData.constEnd(), prevRootComparator) != resp->listData.constEnd())
                throw UnexpectedResponseReceived("ESEARCH contains the THREADPREVIOUS key too many times", *resp);
        }
        if (prevRootIterator->second.size() != 1)
            throw UnexpectedResponseReceived("ESEARCH THREADPREVIOUS contains invalid data", *resp);
        emit incrementalThreadingAvailable(prevRootIterator->second.front(), mapping);
        mapping.clear();
    }

    return true;
}

QVariant ThreadTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Fetching conversations")) : QVariant();
}

void ThreadTask::_failed(const QString &errorMessage)
{
    // FIXME: show this in the GUI
    emit model->threadingFailed(mailboxIndex, algorithm, searchCriteria);
    ImapTask::_failed(errorMessage);
}

}
}
