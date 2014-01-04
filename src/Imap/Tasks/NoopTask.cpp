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


#include "NoopTask.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"
#include "KeepMailboxOpenTask.h"

namespace Imap
{
namespace Mailbox
{

NoopTask::NoopTask(Model *model, ImapTask *parentTask) :
    ImapTask(model)
{
    conn = parentTask;
    parentTask->addDependentTask(this);
}

void NoopTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    tag = parser->noop();
}

bool NoopTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {

        if (resp->kind == Responses::OK) {
            // nothing should be needed here
            _completed();
        } else {
            _failed("NOOP failed, strange");
            // FIXME: error handling
        }
        return true;
    } else {
        return false;
    }
}

QVariant NoopTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Checking for new messages")) : QVariant();
}


}
}
