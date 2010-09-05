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
#include "SelectingHandler.h"

namespace Imap {
namespace Mailbox {

SelectingHandler::SelectingHandler( Model* _m ): ModelStateHandler(_m)
{
}


void SelectingHandler::handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    m->authenticatedHandler->handleState( ptr, resp );
}

void SelectingHandler::handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp )
{
    Q_UNUSED(ptr);
    // FIXME ?
    throw UnexpectedResponseReceived( "Numeric reply while syncing mailbox", *resp );
}

void SelectingHandler::handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp )
{
    m->authenticatedHandler->handleList( ptr, resp );
}

void SelectingHandler::handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "FLAGS reply while syncing mailbox", *resp );
}

void SelectingHandler::handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "SEARCH reply while syncing mailbox", *resp );
}

void SelectingHandler::handleSort( Imap::Parser* ptr, const Imap::Responses::Sort* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "SORT response while syncing mailbox", *resp );
}

void SelectingHandler::handleThread(Imap::Parser *ptr, const Imap::Responses::Thread *const resp)
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "THREAD reply while syncing mailbox", *resp);
}

void SelectingHandler::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    throw CantHappen( "[Port in progress] SelectingHandler::handleFetch shouldn't be triggered");

    /*TreeItemMailbox* mailbox = m->_parsers[ ptr ].currentMbox;
    if ( ! mailbox )
        throw UnexpectedResponseReceived( "Received FETCH reply, but we don't know what mailbox are we syncing", *resp );

    mailbox->handleFetchWhileSyncing( m, ptr, *resp );*/
}

}
}
