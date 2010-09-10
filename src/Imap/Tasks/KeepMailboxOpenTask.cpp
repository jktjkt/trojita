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

#include "KeepMailboxOpenTask.h"
#include "OpenConnectionTask.h"
#include "ObtainSynchronizedMailboxTask.h"
#include "MailboxTree.h"
#include "Model.h"
#include "TaskFactory.h"

namespace Imap {
namespace Mailbox {

KeepMailboxOpenTask::KeepMailboxOpenTask( Model* _model, const QModelIndex& _mailboxIndex, TreeItemMailbox* formerMailbox ) :
    ImapTask( _model ), mailboxIndex(_mailboxIndex), synchronizeConn(0), shouldExit(false), isRunning(false)
{
    qDebug() << "+++" << this;
    Q_ASSERT( mailboxIndex.isValid() );
    Q_ASSERT( mailboxIndex.model() == model );
    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*> ( _mailboxIndex.internalPointer() ) );
    Q_ASSERT( mailbox );
    if ( formerMailbox ) {
        qDebug() << "stealing from mailbox" << formerMailbox->mailbox() << formerMailbox;
        Q_ASSERT( formerMailbox->maintainingTask );
        // Got to copy the parser information
        parser = formerMailbox->maintainingTask->parser;
        // Note that the following will set the maintainigTask's parser to nullptr
        formerMailbox->maintainingTask->addDependentTask( this );
        synchronizeConn = model->_taskFactory->createObtainSynchronizedMailboxTask( _model, mailboxIndex, this );
    } else {
        ImapTask* conn = model->_taskFactory->createOpenConnectionTask( model );
        // we don't register ourselves as a "dependant task", as we don't want connHavingParser to call perform() on us
        qDebug() << "created new conn" << conn;
        connect( conn, SIGNAL(completed()), this, SLOT(slotPerformConnection()) );
        synchronizeConn = model->_taskFactory->createObtainSynchronizedMailboxTask( _model, mailboxIndex, conn );
    }
    qDebug() << "synchro: " << synchronizeConn;
    synchronizeConn->addDependentTask( this );
    mailbox->maintainingTask = this;
}

void KeepMailboxOpenTask::slotPerformConnection()
{
    qDebug() << this << "slotPerformConnection" << synchronizeConn;
    Q_ASSERT( synchronizeConn );
    Q_ASSERT( ! synchronizeConn->isFinished() );
    if ( ! parser ) {
        // Well, this happens iff we created a new connection. We have to steal its parser here.
        parser = synchronizeConn->conn->parser;
    }
    Q_ASSERT( parser );
    synchronizeConn->perform();
}

void KeepMailboxOpenTask::addDependentTask( ImapTask* task )
{
    qDebug() << this << "addDependentTask" << task;
    Q_ASSERT( task );
    KeepMailboxOpenTask* keepTask = qobject_cast<KeepMailboxOpenTask*>( task );
    if ( keepTask ) {
        qDebug() << "! registered our replacement";
        waitingTasks.append( keepTask );
        shouldExit = true;
        if ( dependentTasks.isEmpty() ) {
            qDebug() << "will die now";
            terminate();
        }
    } else {
        connect( task, SIGNAL(destroyed(QObject*)), this, SLOT(slotTaskDeleted(QObject*)) );
        ImapTask::addDependentTask( task );

        if ( isRunning ) {
            qDebug() << "will wake immediately" << task;
            // this function is typically called from task's constructor -> got to call that later
            QTimer::singleShot( 0, task, SLOT(slotPerform()) );
        }
    }
}

void KeepMailboxOpenTask::slotTaskDeleted( QObject *object )
{
    ImapTask* task = qobject_cast<ImapTask*>( object );
    Q_ASSERT( task );
    qDebug() << this << "slotTaskDeleted" << task;
    dependentTasks.removeOne( task );

    if ( shouldExit && dependentTasks.isEmpty() )
        terminate();
}

void KeepMailboxOpenTask::terminate()
{
    qDebug() << this << "terminate();";
    Q_ASSERT( shouldExit );
    Q_ASSERT( dependentTasks.isEmpty() );

    // Mark current mailbox as "orphaned by the housekeeping task"
    Q_ASSERT( mailboxIndex.isValid() );
    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailboxIndex.internalPointer() ) );
    Q_ASSERT( mailbox );
    mailbox->maintainingTask = 0;

    // Merge the lists of waiting tasks
    if ( ! waitingTasks.isEmpty() ) {
        KeepMailboxOpenTask* first = waitingTasks.takeFirst();
        first->waitingTasks = waitingTasks + first->waitingTasks;
        QTimer::singleShot( 0, first, SLOT(slotPerformConnection()) );
    }
    _finished = true;
    emit completed();
}

void KeepMailboxOpenTask::perform()
{
    qDebug() << this << "perform();";
    Q_ASSERT( synchronizeConn );
    parser = synchronizeConn->parser;
    synchronizeConn = 0; // will get deleted by Model

    model->_parsers[ parser ].activeTasks.append( this );

    isRunning = true;
    Q_FOREACH( ImapTask* task, dependentTasks ) {
        if ( ! task->isFinished() )
            task->perform();
    }

    // FIXME: we should move the activation of IDLE here, too
}

void KeepMailboxOpenTask::resynchronizeMailbox()
{
    qDebug() << this << "resynchronizeMailbox();";
    if ( isRunning ) {
        // FIXME: would be cool to wait for completion of current tasks...
        Q_ASSERT ( ! synchronizeConn );
        synchronizeConn = model->_taskFactory->createObtainSynchronizedMailboxTask( model, mailboxIndex, this );
        QTimer::singleShot( 0, this, SLOT(slotPerformConnection()) );
        isRunning = false;
    } else {
        // We aren't running yet, which means that the sync hadn't happened yet, and therefore
        // we don't have to do it "once again" -- it will happen automatically later on.
    }
}


}
}
