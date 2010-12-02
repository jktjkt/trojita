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

ImapTask::ImapTask( Model* _model ) :
    QObject(_model), parser(0), model(_model), _finished(false)
{
    connect( this, SIGNAL(destroyed()), model, SLOT(slotTaskDying()) );
}

ImapTask::~ImapTask()
{
}

void ImapTask::addDependentTask( ImapTask *task )
{
    dependentTasks.append( task );
}

bool ImapTask::handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    handleResponseCode( ptr, resp );
    return handleStateHelper( ptr, resp );
}

bool ImapTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
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

void ImapTask::_completed()
{
    _finished = true;
    Q_FOREACH( ImapTask* task, dependentTasks ) {
        if ( ! task->isFinished() )
            task->perform();
    }
    emit completed();
}

void ImapTask::handleResponseCode( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    using namespace Imap::Responses;
    // Check for common stuff like ALERT and CAPABILITIES update
    switch ( resp->respCode ) {
        case ALERT:
            {
                emit model->alertReceived( tr("The server sent the following ALERT:\n%1").arg( resp->message ) );
            }
            break;
        case CAPABILITIES:
            {
                const RespData<QStringList>* const caps = dynamic_cast<const RespData<QStringList>* const>(
                        resp->respCodeData.data() );
                if ( caps ) {
                    model->updateCapabilities( ptr, caps->data );
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
}

QString ImapTask::debugIdentification() const
{
    return QString();
}

}

}
