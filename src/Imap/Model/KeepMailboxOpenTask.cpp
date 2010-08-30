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
#include "ObtainSynchronizedMailboxTask.h"
#include "Model.h"

namespace Imap {
namespace Mailbox {

KeepMailboxOpenTask::KeepMailboxOpenTask( Model* _model, const QModelIndex& mailbox ) :
    ImapTask( _model ), shouldExit(false)
{
    obtainTask = new ObtainSynchronizedMailboxTask( _model, mailbox );
    connect( obtainTask, SIGNAL(completed()), this, SLOT(slotMailboxObtained()));
}

void KeepMailboxOpenTask::slotMailboxObtained()
{
    Q_FOREACH( ImapTask* task, dependentTasks ) {
        task->perform();
    }
    dependentTasks.clear();
}

void KeepMailboxOpenTask::registerUsingTask( ImapTask* user )
{
    Q_ASSERT(user);
    connect( user, SIGNAL(destroyed(QObject*)), this, SLOT(slotTaskDeleted(QObject*)) );
    users.insert( user );
}

void KeepMailboxOpenTask::slotTaskDeleted( QObject *object )
{
    ImapTask* task = qobject_cast<ImapTask*>( object );
    Q_ASSERT( task );
    users.remove( task );

    if ( shouldExit && users.isEmpty() )
        _completed();
}

void KeepMailboxOpenTask::terminate()
{
    shouldExit = true;
    if ( users.isEmpty() )
        _completed();
}


}
}
