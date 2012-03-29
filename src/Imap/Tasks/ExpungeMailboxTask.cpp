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


#include "ExpungeMailboxTask.h"
#include "KeepMailboxOpenTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap
{
namespace Mailbox
{


ExpungeMailboxTask::ExpungeMailboxTask(Model *model, const QModelIndex &mailbox):
    ImapTask(model), mailboxIndex(mailbox)
{
    conn = model->findTaskResponsibleFor(mailbox);
    conn->addDependentTask(this);
}

void ExpungeMailboxTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    if (! mailboxIndex.isValid()) {
        _failed("Mailbox vanished before we could expunge it");
        // FIXME: add proper fix/callback to the Model
        return;
    }

    IMAP_TASK_CHECK_ABORT_DIE;

    tag = parser->expunge();
}

bool ExpungeMailboxTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {
        if (resp->kind == Responses::OK) {
            _completed();
        } else {
            _failed("Expunge failed");
        }
        return true;
    } else {
        return false;
    }
}


}
}
