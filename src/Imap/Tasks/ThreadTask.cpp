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

    if (m_incrementalMode) {
        tag = parser->uidEThread(algorithm, "utf-8", searchCriteria, QStringList() << "INCREMENTAL");
    } else {
        tag = parser->uidThread(algorithm, "utf-8", searchCriteria);
    }
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

    if (m_incrementalMode) {
        emit incrementalThreadingAvailable(resp->incThreadData);
        return true;
    }

    throw UnexpectedResponseReceived("ESEARCH response to UID THREAD received outside of the incremental mode");
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
