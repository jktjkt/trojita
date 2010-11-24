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

    if ( oldParser ) {
        // We're asked to re-use an existing connection. Let's see if there's something associated with it
        if ( TreeItemMailbox* formerMailbox = model->_parsers[ oldParser ].mailbox ) {
            // there's some mailbox associated here
            Q_ASSERT( formerMailbox->maintainingTask );
            // Got to copy the parser information
            parser = formerMailbox->maintainingTask->parser;
            Q_ASSERT( parser == oldParser );
            // Note that the following will set the maintainigTask's parser to nullptr
            formerMailbox->maintainingTask->addDependentTask( this );
            synchronizeConn = model->_taskFactory->createObtainSynchronizedMailboxTask( _model, mailboxIndex, this );
        } else {
            // and existing parser, but no associated handler yet
            parser = oldParser;
            synchronizeConn = model->_taskFactory->createObtainSynchronizedMailboxTask( _model, mailboxIndex, this );
            QTimer::singleShot( 0, this, SLOT(slotPerformConnection()) );
        }
    } else {
        // create new connection
        ImapTask* conn = model->_taskFactory->createOpenConnectionTask( model );
        // we don't register ourselves as a "dependant task", as we don't want connHavingParser to call perform() on us
        connect( conn, SIGNAL(completed()), this, SLOT(slotPerformConnection()) );
        synchronizeConn = model->_taskFactory->createObtainSynchronizedMailboxTask( _model, mailboxIndex, conn );
    }
    synchronizeConn->addDependentTask( this );
    mailbox->maintainingTask = this;

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
    if ( ! parser ) {
        // Well, this happens iff we created a new connection. We have to steal its parser here.
        parser = synchronizeConn->conn->parser;
    }
    Q_ASSERT( parser );
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
        // We're asked to terminate
        waitingTasks.append( keepTask );
        shouldExit = true;
        if ( idleLauncher ) {
            // got to break the IDLE cycle and especially make sure it won't restart
            idleLauncher->die();
        }
        if ( dependentTasks.isEmpty() ) {
            terminate();
        }
    } else {
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

    if ( shouldExit && dependentTasks.isEmpty() ) {
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
    Q_ASSERT( shouldExit );
    Q_ASSERT( dependentTasks.isEmpty() );

    // Mark current mailbox as "orphaned by the housekeeping task"
    Q_ASSERT( mailboxIndex.isValid() );
    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailboxIndex.internalPointer() ) );
    Q_ASSERT( mailbox );
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
        QTimer::singleShot( 0, first, SLOT(slotPerformConnection()) );
    }
    _finished = true;
    emit completed();
}

void KeepMailboxOpenTask::perform()
{
    Q_ASSERT( synchronizeConn );
    parser = synchronizeConn->parser;
    synchronizeConn = 0; // will get deleted by Model

    Q_ASSERT( parser );
    model->_parsers[ parser ].activeTasks.append( this );

    isRunning = true;
    Q_FOREACH( ImapTask* task, dependentTasks ) {
        if ( ! task->isFinished() )
            task->perform();
    }

    if ( model->_parsers[ parser ].capabilitiesFresh && model->_parsers[ parser ].capabilities.contains( "IDLE" ) )
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
    // FIXME: tests!
    if ( resp->kind == Imap::Responses::EXPUNGE ) {
        Model::ParserState& parser = model->_parsers[ ptr ];
        Q_ASSERT( parser.mailbox );
        parser.mailbox->handleExpunge( model, *resp );
        parser.mailbox->syncState.setExists( parser.mailbox->syncState.exists() - 1 );
        model->cache()->setMailboxSyncState( parser.mailbox->mailbox(), parser.mailbox->syncState );
        return true;
    } else if ( resp->kind == Imap::Responses::EXISTS ) {
        // EXISTS is already updated by AuthenticatedHandler
        Model::ParserState& parser = model->_parsers[ ptr ];
        parser.mailbox->handleExistsSynced( model, ptr, *resp );
        return true;
    } else {
        return false;
    }
}

bool KeepMailboxOpenTask::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    model->_genericHandleFetch( ptr, resp );
    return true;
}

void KeepMailboxOpenTask::slotPerformNoop()
{
    new NoopTask( model, this );
}

bool KeepMailboxOpenTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
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

}
}
