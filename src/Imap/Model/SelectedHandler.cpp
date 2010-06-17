/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#include "MailboxTree.h"
#include "SelectedHandler.h"

namespace Imap {
namespace Mailbox {

SelectedHandler::SelectedHandler( Model* _m ): ModelStateHandler(_m)
{
}


void SelectedHandler::handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    m->authenticatedHandler->handleState( ptr, resp );
}

void SelectedHandler::handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp )
{
    m->authenticatedHandler->handleNumberResponse( ptr, resp );

    if ( resp->kind == Imap::Responses::EXPUNGE ) {
        Model::ParserState& parser = m->_parsers[ ptr ];
        Q_ASSERT( parser.currentMbox );
        parser.currentMbox->handleExpunge( m, *resp );
        parser.syncState.setExists( parser.syncState.exists() - 1 );
        m->cache()->setMailboxSyncState( parser.currentMbox->mailbox(), parser.syncState );
    } else if ( resp->kind == Imap::Responses::EXISTS ) {
        // EXISTS is already updated by AuthenticatedHandler
        Model::ParserState& parser = m->_parsers[ ptr ];
        Q_ASSERT( parser.currentMbox );
        if ( parser.selectingAnother == 0 ) {
            parser.currentMbox->handleExistsSynced( m, ptr, *resp );
        }
    }
}

void SelectedHandler::handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp )
{
    m->authenticatedHandler->handleList( ptr, resp );
}

void SelectedHandler::handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp )
{
    Q_UNUSED(ptr); Q_UNUSED(resp);
    // FIXME
}

void SelectedHandler::handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp )
{
    Q_UNUSED(ptr);
    // FIXME
    throw UnexpectedResponseReceived( "SEARCH reply, wtf?", *resp );
}

void SelectedHandler::handleSort( Imap::Parser* ptr, const Imap::Responses::Sort* const resp )
{
    Q_UNUSED(ptr);
    // FIXME
    throw UnexpectedResponseReceived( "SORT reply, wtf?", *resp );
}

void SelectedHandler::handleThread( Imap::Parser* ptr, const Imap::Responses::Thread* const resp )
{
    Q_UNUSED(ptr);
    // FIXME
    throw UnexpectedResponseReceived( "THREAD reply, wtf?", *resp );
}

void SelectedHandler::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    TreeItemMailbox* mailbox = m->_parsers[ ptr ].currentMbox;
    if ( ! mailbox )
        throw UnexpectedResponseReceived( "Received FETCH reply, but AFAIK we haven't selected any mailbox yet", *resp );

    TreeItemPart* changedPart = 0;
    TreeItemMessage* changedMessage = 0;
    mailbox->handleFetchResponse( m, *resp, &changedPart, &changedMessage );
    if ( changedPart ) {
        QModelIndex index = m->createIndex( changedPart->row(), 0, changedPart );
        emit m->dataChanged( index, index );
    }
    if ( changedMessage ) {
        QModelIndex index = m->createIndex( changedMessage->row(), 0, changedMessage );
        emit m->dataChanged( index, index );
    }
}

}
}
