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


#include "ThreadTask.h"
#include "ItemRoles.h"
#include "KeepMailboxOpenTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap
{
namespace Mailbox
{


ThreadTask::ThreadTask(Model *model, const QModelIndex &mailbox, const QString &algorithm, const QStringList &searchCriteria):
    ImapTask(model), mailboxIndex(mailbox), algorithm(algorithm), searchCriteria(searchCriteria)
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

    tag = parser->uidThread(algorithm, QLatin1String("utf-8"), searchCriteria);
}

bool ThreadTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {
        if (resp->kind == Responses::OK) {
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
