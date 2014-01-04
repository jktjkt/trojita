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

//#define TROJITA_DEBUG_TASK_TREE

#ifndef IMAP_IMAPTASK_H
#define IMAP_IMAPTASK_H

#include <QObject>
#include <QPointer>
#include "Common/Logging.h"
#include "../Parser/Parser.h"
#include "../Model/FlagsOperation.h"

namespace Imap
{

namespace Mailbox
{

class Model;

/** @short Parent class for all IMAP-related jobs

Each ImapTask serves a distinct purpose; some of them are for establishing a connection to the IMAP server, others are responsible
for updating FLAGS of some messages, other tasks maintain a given mailbox synchronized with the server's responses and yet others
deal with listing mailboxes, to name a few examples.

Each task signals its successful completion by the completed() signal.  Should the activity fail, failed() is emitted.

Some tasks perform activity which could be interrupted by the user without huge trouble, for example when downloading huge
attachments.  Tasks which support this graceful abort shall do so when asked through the abort() method.

Sometimes a task will have to deal with the fact that it is forbidden to use the network connection anymore.  That's what the
die() command is for.
*/
class ImapTask : public QObject
{
    Q_OBJECT
public:
    explicit ImapTask(Model *model);
    virtual ~ImapTask();

    /** @short Start performing the job of the task

    This method should be overrided by the task implementations.  It gets called when the dispatcher considers this task ready to
    run.
    */
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

    /** @short Another task wants to depend on this one

    When this task finishes successfully, the dependent task gets called.  If this task fails, the child task will not get called.
    */
    virtual void addDependentTask(ImapTask *task);
    void updateParentTask(ImapTask *newParent);

    bool handleState(const Imap::Responses::State *const resp);
    virtual bool handleStateHelper(const Imap::Responses::State *const resp);
    virtual bool handleCapability(const Imap::Responses::Capability *const resp);
    virtual bool handleNumberResponse(const Imap::Responses::NumberResponse *const resp);
    virtual bool handleList(const Imap::Responses::List *const resp);
    virtual bool handleFlags(const Imap::Responses::Flags *const resp);
    virtual bool handleSearch(const Imap::Responses::Search *const resp);
    virtual bool handleESearch(const Imap::Responses::ESearch *const resp);
    virtual bool handleStatus(const Imap::Responses::Status *const resp);
    virtual bool handleFetch(const Imap::Responses::Fetch *const resp);
    virtual bool handleNamespace(const Imap::Responses::Namespace *const resp);
    virtual bool handleSort(const Imap::Responses::Sort *const resp);
    virtual bool handleThread(const Imap::Responses::Thread *const resp);
    virtual bool handleId(const Imap::Responses::Id *const resp);
    virtual bool handleEnabled(const Imap::Responses::Enabled *const resp);
    virtual bool handleVanished(const Imap::Responses::Vanished *const resp);
    virtual bool handleGenUrlAuth(const Imap::Responses::GenUrlAuth *const resp);
    virtual bool handleSocketEncryptedResponse(const Imap::Responses::SocketEncryptedResponse *const resp);
    virtual bool handleSocketDisconnectedResponse(const Imap::Responses::SocketDisconnectedResponse *const resp);
    virtual bool handleParseErrorResponse(const Imap::Responses::ParseErrorResponse *const resp);

    /** @short Return true if this task has already finished and can be safely deleted */
    bool isFinished() const { return _finished; }

    /** @short Return true if this task doesn't depend on anything can be run immediately */
    virtual bool isReadyToRun() const;

    /** @short Return true if this task needs properly maintained state of the mailbox

    Tasks which don't care about whether the connection has any mailbox opened (like listing mailboxes, performing STATUS etc)
    return true.
    */
    virtual bool needsMailbox() const = 0;

    /** @short Obtain some additional information for the purpose of this task for debugging purposes

    The meaning of this function is to be able to tell what any given Task is supposed to do. It's useful
    especially when the Model is compiled with DEBUG_TASK_ROUTING.
    */
    virtual QString debugIdentification() const;

    /** @short Implemente fetching of data for TaskPresentationModel */
    virtual QVariant taskData(const int role) const = 0;

protected:
    void _completed();

    virtual void _failed(const QString &errorMessage);

    /** @short Kill all pending tasks that are waiting for this one to success */
    virtual void killAllPendingTasks();

    /** @short Get a debug logger associated with this task

    Use this function to obtain a logger which can be used for recording debug information associated with the current
    task. The events will be marked with an identification of the task which triggered them, and could be accessed from
    the GUI if logging is enabled.
    */
    void log(const QString &message, const Common::LogKind kind = Common::LOG_TASKS);

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
    /** @short This signal is emitted upon successful completion of a job */
    void completed(Imap::Mailbox::ImapTask *task);

public:
    Imap::Parser *parser;
    QPointer<ImapTask> parentTask;

protected:
    Model *model;
    QList<ImapTask *> dependentTasks;
    bool _finished;
    bool _dead;
    bool _aborted;

    friend class TaskPresentationModel; // needs access to the TaskPresentationModel
    friend class KeepMailboxOpenTask; // needs access to dependentTasks for removing stuff
#ifdef TROJITA_DEBUG_TASK_TREE
    friend class Model; // needs access to dependentTasks for verification
#endif
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

#ifdef TROJITA_DEBUG_TASK_TREE
#define CHECK_TASK_TREE {model->checkTaskTreeConsistency();}
#else
#define CHECK_TASK_TREE {}
#endif

}
}

Q_DECLARE_METATYPE(Imap::Mailbox::ImapTask *)

#endif // IMAP_IMAPTASK_H
