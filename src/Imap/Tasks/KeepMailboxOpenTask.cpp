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
#include "IdleLauncher.h"
#include "MailboxTree.h"
#include "Model.h"
#include "TaskFactory.h"
#include "NoopTask.h"

namespace Imap {
namespace Mailbox {

/*
FIXME: we should eat "* OK [CLOSED] former mailbox closed", or somehow let it fall down to the model, which shouldn't delegate it to AuthenticatedHandler
*/

KeepMailboxOpenTask::KeepMailboxOpenTask( Model* _model, const QModelIndex& _mailboxIndex, Parser* oldParser ) :
    ImapTask( _model ), mailboxIndex(_mailboxIndex), synchronizeConn(0), shouldExit(false), isRunning(false),
    shouldRunNoop(false), shouldRunIdle(false), idleLauncher(0)
{
    Q_ASSERT( mailboxIndex.isValid() );
    Q_ASSERT( mailboxIndex.model() == model );
    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*> ( _mailboxIndex.internalPointer() ) );
    Q_ASSERT( mailbox );

    // We're the latest KeepMailboxOpenTask, so it makes a lot of sense to add us as the active
    // maintainingTask to the target mailbox
    mailbox->maintainingTask = this;

    if ( oldParser ) {
        // We're asked to re-use an existing connection. Let's see if there's something associated with it

        // Find if there's a KeepMailboxOpenTask already associated; if it is, we have to register with it
        if ( model->accessParser( oldParser ).maintainingTask ) {
            // The parser looks busy -- some task is associated with it and has a mailbox open, so
            // let's just wait till we get a chance to play
            // Got to copy the parser information; also make sure our assumptions about who's who really hold
            parser = oldParser;
            Q_ASSERT( model->accessParser( oldParser ).maintainingTask->parser == oldParser );
            model->accessParser( oldParser ).maintainingTask->addDependentTask( this );
            synchronizeConn = model->_taskFactory->createObtainSynchronizedMailboxTask( _model, mailboxIndex, this );
            // our synchronizeConn will be triggered by that other KeepMailboxOpenTask
        } else {
            // The parser is free, or at least there's no KeepMailboxOpenTask associated with it
            // That means that we can steal its parser...
            parser = oldParser;
            // ...and simply schedule us for immediate execution. We can do that, because there's
            // no mailbox besides us in the game, yet.
            synchronizeConn = model->_taskFactory->createObtainSynchronizedMailboxTask( _model, mailboxIndex, this );
            model->accessParser( oldParser ).maintainingTask = this;
            QTimer::singleShot( 0, this, SLOT(slotPerformConnection()) );
            // We'll also register with the model, so that all other KeepMailboxOpenTask which could
            // get constructed in future know about us and don't step on our toes
        }
    } else {
        // Create new connection
        ImapTask* conn = model->_taskFactory->createOpenConnectionTask( model );
        parser = conn->parser;
        Q_ASSERT(parser);
        model->accessParser( parser ).maintainingTask = this;
        // We don't register ourselves as a "dependant task" on conn, as we don't want it to call perform() on us;
        // instead of that, we listen for its completion and trigger the slotPerformConnection()
        connect( conn, SIGNAL(completed()), this, SLOT(slotPerformConnection()) );
        synchronizeConn = model->_taskFactory->createObtainSynchronizedMailboxTask( _model, mailboxIndex, conn );
    }
    // This will make sure that synchronizeConn will call perform() on us when it's finished
    synchronizeConn->addDependentTask( this );

    // Setup the timer for NOOPing. It won't get started at this time, though.
    noopTimer = new QTimer(this);
    connect( noopTimer, SIGNAL(timeout()), this, SLOT(slotPerformNoop()) );
    bool ok;
    int timeout = model->property( "trojita-imap-noop-period" ).toUInt( &ok );
    if ( ! ok )
        timeout = 2 * 60 * 1000; // once every two minutes
    noopTimer->setInterval( timeout );
    noopTimer->setSingleShot( true );
}

void KeepMailboxOpenTask::slotPerformConnection()
{
    Q_ASSERT( synchronizeConn );
    Q_ASSERT( ! synchronizeConn->isFinished() );
    synchronizeConn->perform();
}

void KeepMailboxOpenTask::addDependentTask( ImapTask* task )
{
    Q_ASSERT( task );

    if ( idleLauncher && idleLauncher->idling() ) {
        // If we're idling right now, we should immediately abort
        idleLauncher->finishIdle();
    }

    KeepMailboxOpenTask* keepTask = qobject_cast<KeepMailboxOpenTask*>( task );
    if ( keepTask ) {
        // Another KeepMailboxOpenTask would like to replace us, so we shall die, eventually.

        waitingTasks.append( keepTask );
        shouldExit = true;

        if ( dependentTasks.isEmpty() && ( ! synchronizeConn || synchronizeConn->isFinished() ) ) {
            terminate();
        }
    } else {
        Q_ASSERT( ! shouldExit ); // if this was true, the task should've chosen something else...
        connect( task, SIGNAL(destroyed(QObject*)), this, SLOT(slotTaskDeleted(QObject*)) );
        ImapTask::addDependentTask( task );

        if ( isRunning ) {
            // this function is typically called from task's constructor -> got to call that later
            QTimer::singleShot( 0, task, SLOT(slotPerform()) );
        }
    }
}

void KeepMailboxOpenTask::slotTaskDeleted( QObject *object )
{
    // Now, object is no longer an ImapTask*, as this gets emitted from inside QObject's destructor. However,
    // we can't use the passed pointer directly, and therefore we have to perform the cast here. It is safe
    // to do that here, as we're only interested in raw pointer value.
    dependentTasks.removeOne( static_cast<ImapTask*>( object ) );

    if ( shouldExit && dependentTasks.isEmpty() && ( ! synchronizeConn || synchronizeConn->isFinished() ) ) {
        terminate();
    } else if ( shouldRunNoop ) {
        // A command just completed, and NOOPing is active, so let's schedule it again
        noopTimer->start();
    } else if ( shouldRunIdle ) {
        // A command just completed and IDLE is supported, so let's queue it
        idleLauncher->enterIdleLater();
    }
}

void KeepMailboxOpenTask::terminate()
{
    Q_ASSERT( dependentTasks.isEmpty() );

    // Break periodic activities
    if ( idleLauncher ) {
        // got to break the IDLE cycle and especially make sure it won't restart
        idleLauncher->die();
    }
    shouldRunIdle = false;
    shouldRunNoop = false;

    // Mark current mailbox as "orphaned by the housekeeping task"
    Q_ASSERT( mailboxIndex.isValid() );
    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailboxIndex.internalPointer() ) );
    Q_ASSERT( mailbox );

    // Don't leave dangling pointers
    if ( mailbox->maintainingTask == this )
        mailbox->maintainingTask = 0;

    // Don't forget to disable NOOPing
    noopTimer->stop();

    // ...and idling, too
    if ( idleLauncher )
        idleLauncher->die();

    // Merge the lists of waiting tasks
    if ( ! waitingTasks.isEmpty() ) {
        KeepMailboxOpenTask* first = waitingTasks.takeFirst();
        first->waitingTasks = waitingTasks + first->waitingTasks;
        model->accessParser( parser ).maintainingTask = first;
        QTimer::singleShot( 0, first, SLOT(slotPerformConnection()) );
    }
    _finished = true;
    emit completed();
}

void KeepMailboxOpenTask::perform()
{
    Q_ASSERT( synchronizeConn );
    Q_ASSERT( synchronizeConn->isFinished() );
    parser = synchronizeConn->parser;
    synchronizeConn = 0; // will get deleted by Model
    Q_ASSERT( parser );

    model->accessParser( parser ).activeTasks.append( this );

    if ( ! waitingTasks.isEmpty() && dependentTasks.isEmpty() ) {
        // We're basically useless, but we have to die reasonably
        shouldExit = true;
        terminate();
        return;
    }

    isRunning = true;
    Q_FOREACH( ImapTask* task, dependentTasks ) {
        if ( ! task->isFinished() )
            task->perform();
    }

    if ( model->accessParser( parser ).capabilitiesFresh && model->accessParser( parser ).capabilities.contains( "IDLE" ) )
        shouldRunIdle = true;
    else
        shouldRunNoop = true;

    if ( shouldRunNoop ) {
        noopTimer->start();
    } else if ( shouldRunIdle ) {
        idleLauncher = new IdleLauncher( this );
        if ( dependentTasks.isEmpty() ) {
            // There's no task yet, so we have to start IDLE now
            idleLauncher->enterIdleLater();
        }
    }
}

void KeepMailboxOpenTask::resynchronizeMailbox()
{
    if ( isRunning ) {
        // FIXME: would be cool to wait for completion of current tasks...
        // Instead of wild magic with re-creating synchronizeConn, it's way easier to
        // just have us replaced by another KeepMailboxOpenTask
        model->_taskFactory->createKeepMailboxOpenTask( model, mailboxIndex, parser );
    } else {
        // We aren't running yet, which means that the sync hadn't happened yet, and therefore
        // we don't have to do it "once again" -- it will happen automatically later on.
    }
}

bool KeepMailboxOpenTask::handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp )
{
    // FIXME: add proper boundaries
    if ( shouldExit || ! isRunning )
        return false;

    TreeItemMailbox *mailbox = Model::mailboxForSomeItem( mailboxIndex );
    Q_ASSERT(mailbox);
    // FIXME: tests!
    if ( resp->kind == Imap::Responses::EXPUNGE ) {
        mailbox->handleExpunge( model, *resp );
        mailbox->syncState.setExists( mailbox->syncState.exists() - 1 );
        model->cache()->setMailboxSyncState( mailbox->mailbox(), mailbox->syncState );
        return true;
    } else if ( resp->kind == Imap::Responses::EXISTS ) {
        // EXISTS is already updated by AuthenticatedHandler
        mailbox->handleExistsSynced( model, ptr, *resp );
        return true;
    } else {
        return false;
    }
}

bool KeepMailboxOpenTask::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    // FIXME: add proper boundaries
    if ( shouldExit || ! isRunning )
        return false;

    TreeItemMailbox *mailbox = Model::mailboxForSomeItem( mailboxIndex );
    Q_ASSERT(mailbox);
    model->_genericHandleFetch( mailbox, resp );
    return true;
}

void KeepMailboxOpenTask::slotPerformNoop()
{
    new NoopTask( model, this );
}

bool KeepMailboxOpenTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    // FIXME: checks for shouldExit and proper boundaries?

    if ( resp->tag.isEmpty() )
        return false;

    if ( resp->tag == tagIdle ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( tagIdle, Model::Task::IDLE );

        Q_ASSERT( idleLauncher );
        if ( resp->kind == Responses::OK ) {
            // The IDLE got terminated for whatever reason, so we should schedule its restart
            idleLauncher->enterIdleLater();
        } else {
            // FIXME: we should get nervous because it failed -- we won't restart it anyway...
        }
        tagIdle.clear();
        IMAP_TASK_CLEANUP_COMMAND;
        return true;
    } else {
        return false;
    }
}

void KeepMailboxOpenTask::die()
{
    if ( noopTimer )
        noopTimer->stop();
    if ( idleLauncher )
        idleLauncher->die();
}

QString KeepMailboxOpenTask::debugIdentification() const
{
    if ( ! mailboxIndex.isValid() )
        return QString::fromAscii("[invalid mailboxIndex]");

    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailboxIndex.internalPointer() ) );
    Q_ASSERT(mailbox);
    return QString::fromAscii("attached to %1").arg( mailbox->mailbox() );
}

}
}
