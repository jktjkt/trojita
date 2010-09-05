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


#include "DeleteMailboxTask.h"
#include "GetAnyConnectionTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap {
namespace Mailbox {


DeleteMailboxTask::DeleteMailboxTask( Model* _model, const QString& _mailbox ):
    ImapTask( _model ), mailbox(_mailbox)
{
    conn = model->_taskFactory->createGetAnyConnectionTask( _model );
    conn->addDependentTask( this );
}

void DeleteMailboxTask::perform()
{
    parser = conn->parser;
    model->_parsers[ parser ].activeTasks.append( this );

    tag = parser->deleteMailbox( mailbox );
    model->_parsers[ parser ].commandMap[ tag ] = Model::Task( Model::Task::DELETE, 0 );
    emit model->activityHappening( true );
}

bool DeleteMailboxTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    if ( resp->tag == tag ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( tag, Model::Task::DELETE );

        if ( resp->kind == Responses::OK ) {
            TreeItemMailbox* mailboxPtr = model->findMailboxByName( mailbox );
            if ( mailboxPtr ) {
                TreeItem* parentPtr = mailboxPtr->parent();
                QModelIndex parentIndex = parentPtr == model->_mailboxes ? QModelIndex() : model->createIndex( parentPtr->row(), 0, parentPtr );
                model->beginRemoveRows( parentIndex, mailboxPtr->row(), mailboxPtr->row() );
                mailboxPtr->parent()->_children.removeAt( mailboxPtr->row() );
                model->endRemoveRows();
                delete mailboxPtr;
            } else {
                qDebug() << "The IMAP server just told us that it succeded to delete mailbox named" <<
                        mailbox << ", yet wo don't know of any such mailbox. Message from the server:" <<
                        resp->message;
            }
            emit model->mailboxDeletionSucceded( mailbox );
        } else {
            emit model->mailboxDeletionFailed( mailbox, resp->message );
            // FIXME: Tasks API error handling
        }
        _completed();
        IMAP_TASK_CLEANUP_COMMAND;
        return true;
    } else {
        return false;
    }
}


}
}
