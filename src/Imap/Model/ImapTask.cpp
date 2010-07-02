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

#include "ImapTask.h"
#include "Model.h"

namespace Imap {
namespace Mailbox {

ImapTask::ImapTask( Model* _model, Imap::Parser* _parser, ImapTask* parentTask ) :
    QObject(_model), model(_model), parser(_parser)
{
    if ( parentTask )
        parentTask->addDependentTask( this );
}

ImapTask::~ImapTask()
{
    Q_FOREACH( ImapTask* task, dependentTasks ) {
        task->deleteLater();
    }
}

bool ImapTask::handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    Q_UNUSED(ptr);
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleCapability( Imap::Parser* ptr, const Imap::Responses::Capability* const resp )
{
    Q_UNUSED(ptr);
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp )
{
    Q_UNUSED(ptr);
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp )
{
    Q_UNUSED(ptr);
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp )
{
    Q_UNUSED(ptr);
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp )
{
    Q_UNUSED(ptr);
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleStatus( Imap::Parser* ptr, const Imap::Responses::Status* const resp )
{
    Q_UNUSED(ptr);
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    Q_UNUSED(ptr);
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleNamespace( Imap::Parser* ptr, const Imap::Responses::Namespace* const resp )
{
    Q_UNUSED(ptr);
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleSort( Imap::Parser* ptr, const Imap::Responses::Sort* const resp )
{
    Q_UNUSED(ptr);
    Q_UNUSED(resp);
    return false;
}

bool ImapTask::handleThread( Imap::Parser* ptr, const Imap::Responses::Thread* const resp )
{
    Q_UNUSED(ptr);
    Q_UNUSED(resp);
    return false;
}

}
}
