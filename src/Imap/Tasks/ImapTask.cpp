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

#include "ImapTask.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/TaskPresentationModel.h"
#include "KeepMailboxOpenTask.h"

namespace Imap
{
namespace Mailbox
{

ImapTask::ImapTask(Model *model) :
    QObject(model), parser(0), parentTask(0), model(model), _finished(false), _dead(false), _aborted(false)
{
    connect(this, SIGNAL(destroyed(QObject *)), model, SLOT(slotTaskDying(QObject *)));
    CHECK_TASK_TREE;
}

ImapTask::~ImapTask()
{
}

/** @short Schedule another task to get a go when this one completes

This function informs the current task (this) that when it terminates successfully, the dependant task (@arg task) shall be started.
Subclasses are free to reimplement this method (@see KeepMailboxOpenTask), but they must not forget to update the parentTask of
the depending task.
*/
void ImapTask::addDependentTask(ImapTask *task)
{
    CHECK_TASK_TREE
    Q_ASSERT(task);
    dependentTasks.append(task);
    task->updateParentTask(this);
    CHECK_TASK_TREE
}

/** @short Set this task's parent to the specified value */
void ImapTask::updateParentTask(ImapTask *newParent)
{
    Q_ASSERT(!parentTask);
    Q_ASSERT(newParent);
    parentTask = newParent;
    CHECK_TASK_TREE
    model->m_taskModel->slotTaskGotReparented(this);
    if (parser) {
        Q_ASSERT(!model->accessParser(parser).activeTasks.contains(this));
        //log(tr("Reparented to %1").arg(newParent->debugIdentification()));
    }
    CHECK_TASK_TREE
}

/** @short Tells the Model that we're from now on an active task */
void ImapTask::markAsActiveTask(const TaskActivatingPosition place)
{
    CHECK_TASK_TREE
    Q_ASSERT(parser);
    switch (place) {
    case TASK_APPEND:
        model->accessParser(parser).activeTasks.append(this);
        break;
    case TASK_PREPEND:
        model->accessParser(parser).activeTasks.prepend(this);
        break;
    }
    if (parentTask) {
        parentTask->dependentTasks.removeAll(this);
    }
    // As we're an active task, we no longer have a parent task
    parentTask = 0;
    model->m_taskModel->slotTaskGotReparented(this);

    if (model->accessParser(parser).maintainingTask && model->accessParser(parser).maintainingTask != this) {
        // Got to inform the currently responsible maintaining task about our demise
        connect(this, SIGNAL(destroyed(QObject*)), model->accessParser(parser).maintainingTask, SLOT(slotTaskDeleted(QObject*)));
    }

    log(QLatin1String("Activated"));
    CHECK_TASK_TREE
}

bool ImapTask::handleState(const Imap::Responses::State *const resp)
{
    handleResponseCode(resp);
    return handleStateHelper(resp);
}

bool ImapTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleCapability(const Imap::Responses::Capability *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleNumberResponse(const Imap::Responses::NumberResponse *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleList(const Imap::Responses::List *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleFlags(const Imap::Responses::Flags *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleSearch(const Imap::Responses::Search *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleESearch(const Imap::Responses::ESearch *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleStatus(const Imap::Responses::Status *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleFetch(const Imap::Responses::Fetch *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleNamespace(const Imap::Responses::Namespace *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleSort(const Imap::Responses::Sort *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleThread(const Imap::Responses::Thread *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleId(const Responses::Id *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleEnabled(const Responses::Enabled *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleVanished(const Responses::Vanished *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleGenUrlAuth(const Responses::GenUrlAuth *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleSocketEncryptedResponse(const Imap::Responses::SocketEncryptedResponse *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleSocketDisconnectedResponse(const Imap::Responses::SocketDisconnectedResponse *const resp)
{
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleParseErrorResponse(const Imap::Responses::ParseErrorResponse *const resp)
{
    Q_UNUSED(resp);
    return false;
}

void ImapTask::_completed()
{
    _finished = true;
    log("Completed");
    Q_FOREACH(ImapTask* task, dependentTasks) {
        if (!task->isFinished())
            task->perform();
    }
    emit completed(this);
}

void ImapTask::_failed(const QString &errorMessage)
{
    _finished = true;
    killAllPendingTasks();
    log(QString::fromUtf8("Failed: %1").arg(errorMessage));
    emit failed(errorMessage);
}

void ImapTask::killAllPendingTasks()
{
    Q_FOREACH(ImapTask *task, dependentTasks) {
        task->die();
    }
}

void ImapTask::handleResponseCode(const Imap::Responses::State *const resp)
{
    using namespace Imap::Responses;
    // Check for common stuff like ALERT and CAPABILITIES update
    switch (resp->respCode) {
    case ALERT:
        emit model->alertReceived(tr("The server sent the following ALERT:\n%1").arg(resp->message));
        break;
    case CAPABILITIES:
    {
        const RespData<QStringList> *const caps = dynamic_cast<const RespData<QStringList>* const>(resp->respCodeData.data());
        if (caps) {
            model->updateCapabilities(parser, caps->data);
        }
    }
    break;
    case BADCHARSET:
    case PARSE:
        qDebug() << "The server was having troubles with parsing message data:" << resp->message;
        break;
    default:
        // do nothing here, it must be handled later
        break;
    }
}

bool ImapTask::isReadyToRun() const
{
    return false;
}

void ImapTask::die()
{
    _dead = true;
    if (!_finished)
        _failed("Asked to die");
}

void ImapTask::abort()
{
    _aborted = true;
    Q_FOREACH(ImapTask* task, dependentTasks) {
        task->abort();
    }
}

QString ImapTask::debugIdentification() const
{
    return QString();
}

void ImapTask::log(const QString &message, const Common::LogKind kind)
{
    Q_ASSERT(model);
    QString dbg = debugIdentification();
    if (!dbg.isEmpty()) {
        dbg.prepend(QLatin1Char(' '));
    }
    model->logTrace(parser ? parser->parserId() : 0, kind, metaObject()->className() + dbg, message);
    model->m_taskModel->slotTaskMighHaveChanged(this);
}

}

}
