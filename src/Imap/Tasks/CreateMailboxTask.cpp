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


#include "CreateMailboxTask.h"
#include "Common/InvokeMethod.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"
#include "GetAnyConnectionTask.h"

namespace Imap
{
namespace Mailbox
{


CreateMailboxTask::CreateMailboxTask(Model *model, const QString &mailbox):
    ImapTask(model), mailbox(mailbox)
{
    conn = model->m_taskFactory->createGetAnyConnectionTask(model);
    conn->addDependentTask(this);
}

void CreateMailboxTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    tagCreate = parser->create(mailbox);
}

bool CreateMailboxTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tagCreate) {
        if (resp->kind == Responses::OK) {
            EMIT_LATER(model, mailboxCreationSucceded, Q_ARG(QString, mailbox));
            if (_dead) {
                // Got to check if we're still allowed to execute before launching yet another command
                _failed("Asked to die");
                return true;
            }
            tagList = parser->list(QLatin1String(""), mailbox);
            // Don't call _completed() yet, we're going to update mbox list before that
        } else {
            EMIT_LATER(model, mailboxCreationFailed, Q_ARG(QString, mailbox), Q_ARG(QString, resp->message));
            _failed("Cannot create mailbox");
        }
        return true;
    } else if (resp->tag == tagList) {
        if (resp->kind == Responses::OK) {
            model->finalizeIncrementalList(parser, mailbox);
            _completed();
        } else {
            _failed("Error with the LIST command after the CREATE");
        }
        return true;
    } else {
        return false;
    }
}

QVariant CreateMailboxTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Creating mailbox")) : QVariant();
}


}
}
