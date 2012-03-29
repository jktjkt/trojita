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

#ifndef IMAP_IMAPTASK_H
#define IMAP_IMAPTASK_H

#include <QObject>
#include "../Parser/Parser.h"
#include "../Model/Logging.h"

namespace Imap
{

namespace Responses
{
class State;
class Capability;
class NumberResponse;
class List;
class Flags;
class Search;
class Status;
class Fetch;
class Namespace;
class Sort;
class Thread;
}

namespace Mailbox
{

class Model;

/** @short Parent class for all IMAP-related jobs */
class ImapTask : public QObject
{
    Q_OBJECT
public:
    ImapTask(Model *_model);
    virtual ~ImapTask();

    virtual void perform() = 0;

    /** @short Used for informing the task that it should cease from performing *any* activities immediately and that it will die soon

    This is crucial for any tasks which could perform some periodical activities involving Parser*, and should
    also be implemented for those that want to restore the rest of the world to a reasonable and consistent state
    before they get killed.

    This function is really a hard requirement -- as soon as this function got called, it's an error for this task to talk to the
    parser at all.
    */
    virtual void die();

    /** @short Abort the current activity of this task in a safe manner

    This function is executed in contexts where someone/something has decided that this task shall not really proceed any further.
    In case the activity is already in the middle of a critical section, it shall however proceed further and finish.
    */
    virtual void abort();

    virtual void addDependentTask(ImapTask *task);
    void updateParentTask(ImapTask *newParent);

    bool handleState(const Imap::Responses::State *const resp);
    virtual bool handleStateHelper(const Imap::Responses::State *const resp);
    virtual bool handleCapability(const Imap::Responses::Capability *const resp);
    virtual bool handleNumberResponse(const Imap::Responses::NumberResponse *const resp);
    virtual bool handleList(const Imap::Responses::List *const resp);
    virtual bool handleFlags(const Imap::Responses::Flags *const resp);
    virtual bool handleSearch(const Imap::Responses::Search *const resp);
    virtual bool handleStatus(const Imap::Responses::Status *const resp);
    virtual bool handleFetch(const Imap::Responses::Fetch *const resp);
    virtual bool handleNamespace(const Imap::Responses::Namespace *const resp);
    virtual bool handleSort(const Imap::Responses::Sort *const resp);
    virtual bool handleThread(const Imap::Responses::Thread *const resp);
    virtual bool handleId(const Imap::Responses::Id *const resp);

    /** @short Return true if this task has already finished and can be safely deleted */
    bool isFinished() const { return _finished; }

    /** @short Return true if this task doesn't depend on anything can be run immediately */
    virtual bool isReadyToRun() const;

    /** @short Obtain some additional information for the purpose of this task for debugging purposes

    The meaning of this function is to be able to tell what any given Task is supposed to do. It's useful
    especially when the Model is compiled with DEBUG_TASK_ROUTING.
    */
    virtual QString debugIdentification() const;

protected:
    void _completed();

    void _failed(const QString &errorMessage);

    /** @short Kill all pending tasks that are waiting for this one to success */
    virtual void killAllPendingTasks();

    /** @short Get a debug logger associated with this task

    Use this function to obtain a logger which can be used for recording debug information associated with the current
    task. The events will be marked with an identification of the task which triggered them, and could be accessed from
    the GUI if logging is enabled.
    */
    void log(const QString &message, const LogKind kind=LOG_TASKS);

    /** @short Priority of the task activation that controls where it gets placed in the queue */
    typedef enum {
        TASK_APPEND, /**< @short Normal mode -- task goes at the end of the list */
        TASK_PREPEND /**< @short Special mode -- this task shall have higher priority for all event processing and hence goes to the top */
    } TaskActivatingPosition;
    void markAsActiveTask(const TaskActivatingPosition place=TASK_APPEND);

private:
    void handleResponseCode(const Imap::Responses::State *const resp);

signals:
    /** @short This signal is emitted if the job failed in some way */
    void failed(QString errorMessage);
    /** @short This signal is emitted upon succesfull completion of a job */
    void completed(ImapTask *task);

public:
    Imap::Parser *parser;
    ImapTask *parentTask;

protected:
    Model *model;
    QList<ImapTask *> dependentTasks;
    bool _finished;
    bool _dead;
    bool _aborted;

    friend class TaskPresentationModel; // needs access to the TaskPresentationModel
};

#define IMAP_TASK_CHECK_ABORT_DIE \
    if (_dead) {\
        _failed("Asked to die");\
        return;\
    } \
    if (_aborted) {\
        _failed("Aborted");\
        return;\
    }

}
}

#endif // IMAP_IMAPTASK_H
