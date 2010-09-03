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


#include "CopyMoveMessagesTask.h"
#include "CreateConnectionTask.h"
#include "UpdateFlagsTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap {
namespace Mailbox {


CopyMoveMessagesTask::CopyMoveMessagesTask( Model* _model, const QModelIndexList& _messages,
                                            const QString& _targetMailbox, const CopyMoveOperation _op ):
    ImapTask( _model ), targetMailbox(_targetMailbox), shouldDelete( _op == MOVE)
{
    if ( _messages.isEmpty() ) {
        throw CantHappen( "CopyMoveMessagesTask called with empty message set");
    }
    TreeItemMailbox* mailbox = 0;
    Q_FOREACH( const QModelIndex& index, _messages ) {
        TreeItem* item = static_cast<TreeItem*>( index.internalPointer() );
        Q_ASSERT(item);
        TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( item->parent() );
        Q_ASSERT(list);
        TreeItemMailbox* currentMailbox = dynamic_cast<TreeItemMailbox*>( list->parent() );
        Q_ASSERT(currentMailbox);
        if ( ! mailbox ) {
            mailbox = currentMailbox;
        } else if ( mailbox != currentMailbox ) {
            throw CantHappen( "CopyMoveMessagesTask called with messages from several mailboxes");
        }
        messages << index;
    }
    conn = new CreateConnectionTask( _model, mailbox );
    conn->addDependentTask( this );
}

void CopyMoveMessagesTask::perform()
{
    Sequence seq;
    bool first = true;

    Q_FOREACH( const QPersistentModelIndex& index, messages ) {
        if ( ! index.isValid() ) {
            // FIXME: add proper fix
            qDebug() << "Some message got removed before we could copy them";
        } else {
            TreeItem* item = static_cast<TreeItem*>( index.internalPointer() );
            Q_ASSERT(item);
            TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( item );
            Q_ASSERT(message);
            if ( first ) {
                seq = Sequence( message->uid() );
                first = false;
            } else {
                seq.add( message->uid() );
            }
        }
    }

    if ( first ) {
        // No valid messages
        qDebug() << "All messages got removed before we could've copied them";
        _completed();
        return;
    }

    parser = conn->parser;
    model->_parsers[ parser ].activeTasks.append( this );

    copyTag = parser->uidCopy( seq, targetMailbox );
    model->_parsers[ parser ].commandMap[ copyTag ] = Model::Task( Model::Task::COPY, 0 );
    emit model->activityHappening( true );
}

bool CopyMoveMessagesTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    if ( resp->tag == copyTag ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( copyTag, Model::Task::COPY );

        if ( resp->kind == Responses::OK ) {
            if ( shouldDelete ) {
                new UpdateFlagsTask( model, this, messages, QLatin1String("+FLAGS"), QLatin1String("\\Deleted") );
            }
            _completed();
        } else {
            // FIXME: error handling
            qDebug() << "COPY failed:" << resp->message;
            _completed();
        }
        IMAP_TASK_CLEANUP_COMMAND;
        return true;
    } else {
        return false;
    }
}


}
}
