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


#include "FetchMsgMetadataTask.h"
#include "CreateConnectionTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap {
namespace Mailbox {

FetchMsgMetadataTask::FetchMsgMetadataTask( Model* _model, const QModelIndexList& _messages ) :
    ImapTask( _model )
{
    if ( _messages.isEmpty() ) {
        throw CantHappen( "FetchMsgMetadataTask called with empty message set");
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
            throw CantHappen( "FetchMsgMetadataTask called with messages from several mailboxes");
        }
        messages << index;
    }
    conn = new CreateConnectionTask( _model, mailbox );
    conn->addDependentTask( this );
}

void FetchMsgMetadataTask::perform()
{
    Sequence seq;
    bool first = true;

    Q_FOREACH( const QPersistentModelIndex& index, messages ) {
        if ( ! index.isValid() ) {
            // FIXME: add proper fix
            qDebug() << "Some message got removed before we could ask for their metadata";
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
        qDebug() << "All messages got removed before we could ask for their metadata";
        _completed();
        return;
    }

    parser = conn->parser;
    model->_parsers[ parser ].activeTasks.append( this );

    // we do not want to use _onlineMessageFetch because it contains UID and FLAGS
    tag = parser->uidFetch( seq, QStringList() << QLatin1String("ENVELOPE") << QLatin1String("BODYSTRUCTURE") << QLatin1String("RFC822.SIZE") );
    model->_parsers[ parser ].commandMap[ tag ] = Model::Task( Model::Task::FETCH_MESSAGE_METADATA, 0 );
    emit model->activityHappening( true );
}

bool FetchMsgMetadataTask::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    TreeItemMailbox* mailbox = model->_parsers[ ptr ].currentMbox;
    if ( ! mailbox )
        throw UnexpectedResponseReceived( "Received FETCH reply, but AFAIK we haven't selected any mailbox yet", *resp );

    TreeItemPart* changedPart = 0;
    TreeItemMessage* changedMessage = 0;
    mailbox->handleFetchResponse( model, *resp, &changedPart, &changedMessage );
    if ( changedPart ) {
        QModelIndex index = model->createIndex( changedPart->row(), 0, changedPart );
        emit model->dataChanged( index, index );
    }
    if ( changedMessage ) {
        QModelIndex index = model->createIndex( changedMessage->row(), 0, changedMessage );
        emit model->dataChanged( index, index );
    }
    return true;
}

bool FetchMsgMetadataTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    if ( resp->tag == tag ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( tag, Model::Task::FETCH_MESSAGE_METADATA );

        if ( resp->kind == Responses::OK ) {
            // nothing should be needed here
        } else {
            // FIXME: error handling
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
