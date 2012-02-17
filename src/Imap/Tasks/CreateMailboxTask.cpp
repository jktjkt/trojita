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


#include "CreateMailboxTask.h"
#include "GetAnyConnectionTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap {
namespace Mailbox {


CreateMailboxTask::CreateMailboxTask( Model* _model, const QString& _mailbox ):
    ImapTask( _model ), mailbox(_mailbox)
{
    conn = model->_taskFactory->createGetAnyConnectionTask( _model );
    conn->addDependentTask( this );
}

void CreateMailboxTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    tagCreate = parser->create( mailbox );
}

bool CreateMailboxTask::handleStateHelper( const Imap::Responses::State* const resp )
{
    if ( resp->tag.isEmpty() )
        return false;

    if ( resp->tag == tagCreate ) {
        if ( resp->kind == Responses::OK ) {
            emit model->mailboxCreationSucceded( mailbox );
            if (_dead) {
                // Got to check if we're still allowed to execute before launching yet another command
                _failed("Asked to die");
                return true;
            }
            tagList = parser->list( QLatin1String(""), mailbox );
            // Don't call _completed() yet, we're going to update mbox list before that
        } else {
            emit model->mailboxCreationFailed( mailbox, resp->message );
            _failed("Cannot create mailbox");
        }
        return true;
    } else if ( resp->tag == tagList ) {
        if ( resp->kind == Responses::OK ) {
            model->_finalizeIncrementalList( parser, mailbox );
            _completed();
        } else {
            _failed("Error with the LIST command after the CREATE");
        }
        return true;
    } else {
        return false;
    }
}


}
}
