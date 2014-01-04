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

#include "GetAnyConnectionTask.h"
#include <QTimer>
#include "Imap/Model/MailboxTree.h"
#include "KeepMailboxOpenTask.h"
#include "OfflineConnectionTask.h"
#include "OpenConnectionTask.h"

namespace Imap
{
namespace Mailbox
{

GetAnyConnectionTask::GetAnyConnectionTask(Model *model) :
    ImapTask(model), newConn(0)
{
    QMap<Parser *,ParserState>::iterator it = model->m_parsers.begin();
    while (it != model->m_parsers.end()) {
        if (it->connState == CONN_STATE_LOGOUT) {
            // We cannot possibly use this connection
            ++it;
        } else {
            // we've found it
            break;
        }
    }

    if (it == model->m_parsers.end()) {
        // We're creating a completely new connection
        if (model->networkPolicy() == NETWORK_OFFLINE) {
            // ...but we're offline -> too bad, got to fail
            newConn = new OfflineConnectionTask(model);
            newConn->addDependentTask(this);
        } else {
            newConn = model->m_taskFactory->createOpenConnectionTask(model);
            newConn->addDependentTask(this);
        }
    } else {
        parser = it.key();
        Q_ASSERT(parser);

        if (it->maintainingTask) {
            // The parser already has some maintaining task associated with it
            // We can't ignore the maintaining task, if only because of the IDLE
            newConn = it->maintainingTask;
            newConn->addDependentTask(this);
        } else {
            if (!it->activeTasks.isEmpty() && dynamic_cast<OpenConnectionTask*>(it->activeTasks.front()) &&
                   !it->activeTasks.front()->isFinished()) {
                // The conneciton is still being set up so we cannot just jump to the middle of OpenConnectionTask's
                // process (Redmine #499).
                it->activeTasks.front()->addDependentTask(this);
            } else {
                // The parser doesn't have anything associated with it and it looks like
                // the conneciton is already established, authenticated and what not.
                // This means that we can go ahead and register ourselves as an active task, yay!
                markAsActiveTask();
                QTimer::singleShot(0, model, SLOT(runReadyTasks()));
            }
        }
    }
}

void GetAnyConnectionTask::perform()
{
    // This is special from most ImapTasks' perform(), because the activeTasks could have already been updated
    if (newConn) {
        // We're "dependent" on some connection, so we should update our parser (even though
        // it could be already set), and also register ourselves with the Model
        parser = newConn->parser;
        markAsActiveTask();
    }

    IMAP_TASK_CHECK_ABORT_DIE;

    // ... we don't really have to do any work here, just declare ourselves completed
    _completed();
}

bool GetAnyConnectionTask::isReadyToRun() const
{
    return ! isFinished() && ! newConn;
}

/** @short This is an internal task, calling this function does not make much sense */
QVariant GetAnyConnectionTask::taskData(const int role) const
{
    Q_UNUSED(role);
    return QVariant();
}

}
}
