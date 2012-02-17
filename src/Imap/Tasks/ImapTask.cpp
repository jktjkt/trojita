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

#include "ImapTask.h"
#include "Model.h"
#include "TaskPresentationModel.h"

namespace Imap {
namespace Mailbox {

ImapTask::ImapTask(Model* _model) :
    QObject(_model), parser(0), parentTask(0), model(_model), _finished(false), _dead(false), _aborted(false)
{
    connect(this, SIGNAL(destroyed(QObject*)), model, SLOT(slotTaskDying(QObject*)));
}

ImapTask::~ImapTask()
{
}

/** @short Schedule another task to get a go when this one completes

This function informs the current task (this) that when it terminates succesfully, the dependant task (@arg task) shall be started.
Subclasses are free to reimplement this method (@see KeepMailboxOpenTask), but they must not forget to update the parentTask of
the depending task.
*/
void ImapTask::addDependentTask(ImapTask *task)
{
    Q_ASSERT(task);
    dependentTasks.append(task);
    task->updateParentTask(this);
}

/** @short Set this task's parent to the specified value */
void ImapTask::updateParentTask(ImapTask *newParent)
{
    Q_ASSERT(newParent);
    parentTask = newParent;
    model->m_taskModel->slotTaskGotReparented(this);
}

/** @short Tells the Model that we're from now on an active task */
void ImapTask::markAsActiveTask(const TaskActivatingPosition place)
{
    Q_ASSERT(parser);
    switch(place) {
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

void ImapTask::_completed()
{
    log("Completed");
    _finished = true;
    Q_FOREACH(ImapTask* task, dependentTasks) {
        if (!task->isFinished())
            task->perform();
    }
    emit completed(this);
}

void ImapTask::_failed(const QString &errorMessage)
{
    log(QString::fromAscii("Failed: %1").arg(errorMessage));
    _finished = true;
    killAllPendingTasks();
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
        const RespData<QStringList>* const caps = dynamic_cast<const RespData<QStringList>* const>(resp->respCodeData.data());
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
    killAllPendingTasks();
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

void ImapTask::log(const QString &message, const LogKind kind)
{
    Q_ASSERT(model);
    Q_ASSERT(parser);
    QString dbg = debugIdentification();
    if (!dbg.isEmpty()) {
        dbg.prepend(QLatin1Char(' '));
    }
    model->logTrace(parser->parserId(), kind, metaObject()->className() + dbg, message);
    model->m_taskModel->slotTaskMighHaveChanged(this);
}

}

}
