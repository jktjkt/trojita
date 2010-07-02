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


#include "FetchMsgPartTask.h"
#include "CreateConnectionTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap {
namespace Mailbox {

FetchMsgPartTask::FetchMsgPartTask( Model* _model, Imap::Parser* _parser, TreeItemPart* _item ) :
    ImapTask( _model, _parser ), item(_item)
{
    CreateConnectionTask* conn = new CreateConnectionTask( _model, _parser );
    conn->addDependentTask( this );
}

void FetchMsgPartTask::perform()
{
    model->_parsers[ parser ].activeTasks.append( this );
    tag = parser->fetch( Sequence( item->message()->row() + 1 ),
            QStringList() << QString::fromAscii("BODY.PEEK[%1]").arg(
                    item->mimeType() == QLatin1String("message/rfc822") ?
                        QString::fromAscii("%1.HEADER").arg( item->partId() ) :
                        item->partId()
                    ) );
    model->_parsers[ parser ].commandMap[ tag ] = Model::Task( Model::Task::FETCH_PART, item );
    emit model->activityHappening( true );
}

bool FetchMsgPartTask::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
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

bool FetchMsgPartTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    // OK/NO/BAD/PREAUTH/BYE
    using namespace Imap::Responses;

    if ( resp->tag == tag ) {
        QMap<CommandHandle, Model::Task>::iterator command = model->_parsers[ ptr ].commandMap.find( tag );
        if ( command == model->_parsers[ ptr ].commandMap.end() ) {
            qDebug() << "This command is not valid anymore" << tag;
            return false;
        }

        Q_ASSERT( command->kind == Model::Task::FETCH_PART );
        model->_finalizeFetchPart( ptr, command );
        model->parsersMightBeIdling();
        _completed();
        return true;
    } else {
        return false;
    }
}

}
}
