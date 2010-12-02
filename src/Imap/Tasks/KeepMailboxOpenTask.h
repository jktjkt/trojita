/* Copyright (C) 2007 - 2010 Jan Kundrát <jkt@flaska.net>

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

#ifndef IMAP_KEEPMAILBOXOPENTASK_H
#define IMAP_KEEPMAILBOXOPENTASK_H

#include "ImapTask.h"
#include <QModelIndex>
#include "Parser/Parser.h"

class QTimer;

namespace Imap {

class Parser;

namespace Mailbox {

class TreeItemMailbox;
class ObtainSynchronizedMailboxTask;
class IdleLauncher;

/** @short Maintain a connection to a mailbox

This Task shall maintain a connection to a remote mailbox, updating the mailbox
state while receiving various messages.

Essentially, this Task is responsible for processing stuff like EXPUNGE replies
while the mailbox is selected. It's a bit special task, because it will not emit
completed() unless something else wants to re-use its Parser instance.
*/
class KeepMailboxOpenTask : public ImapTask
{
Q_OBJECT
public:
    /** @short Create new task for maintaining a mailbox

@arg mailboxIndex the new mailbox to open and keep open

@arg formerMailbox the mailbox which was kept open by the previous KeepMailboxOpenTask;
that mailbox now loses its KeepMailboxOpenTask and the underlying parser is reused for this task
*/
    KeepMailboxOpenTask( Model* _model, const QModelIndex& _mailboxIndex, Parser* oldParser );

    virtual void die();

    /** @short Similar to die(), but allow for correct abort of a possible IDLE command */
    void stopForLogout();

    /** @short Start child processes

This function is called when the synchronizing task finished succesfully, that is, when we are ready
to execute regular tasks which depend on us.
*/
    virtual void perform();

    /** @short Add any other task which somehow needs our current mailbox

This function also automatically registers the depending task in a special list which will make
sure that we won't emit finished() until all the dependant tasks have finished. This essnetially
prevents replacing an "alive" KeepMailboxOpenTask with a different one.
*/
    virtual void addDependentTask( ImapTask* task);

    /** @short Make sure to re-open the mailbox, even if it is already open */
    void resynchronizeMailbox();

    QString debugIdentification() const;

private slots:
    void slotTaskDeleted( QObject* object );

    /** @short Start mailbox synchronization process

This function is called when we know that the underlying Parser is no longer in active use
in any mailbox and that it is ready to be used for our purposes. It doesn't matter if that
happened because the older KeepMailboxOpenTask got finished or because new connection got
established and entered the authenticated state; the important part is that we should
initialize synchronization now.
*/
    void slotPerformConnection();

    virtual bool handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp );
    virtual bool handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp );
    virtual bool handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp );

    void slotPerformNoop();

private:
    void terminate();

    /** @short Detach from the TreeItemMailbox, preventing new tasks from hitting us */
    void detachFromMailbox( TreeItemMailbox *mailbox );

protected:
    QPersistentModelIndex mailboxIndex;
    QList<KeepMailboxOpenTask*> waitingTasks;
    /** @short An ImapTask that will be started to actually sync to a mailbox once the connection is free */
    ObtainSynchronizedMailboxTask* synchronizeConn;

    bool shouldExit;
    bool isRunning;

    QTimer* noopTimer;
    bool shouldRunNoop;
    bool shouldRunIdle;
    IdleLauncher* idleLauncher;
    CommandHandle tagIdle;
    friend class IdleLauncher;
};

}
}

#endif // IMAP_KEEPMAILBOXOPENTASK_H
