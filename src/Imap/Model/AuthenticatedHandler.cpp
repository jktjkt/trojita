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
#include "AuthenticatedHandler.h"
#include "UnauthenticatedHandler.h"
#include "MailboxTree.h"

namespace Imap {
namespace Mailbox {

AuthenticatedHandler::AuthenticatedHandler( Model* _m ): ModelStateHandler(_m)
{
}


void AuthenticatedHandler::handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    throw CantHappen( "[Port-in-progress] shouldn't have reached AuthenticatedHandler::handleState");
}

void AuthenticatedHandler::handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp )
{
    throw CantHappen( "[Port-in-progress] shouldn't have reached AuthenticatedHandler::handleNumberResponse");
}

void AuthenticatedHandler::handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp )
{
    throw CantHappen("AuthenticatedHandler::handleList: porting in progress...");
}

void AuthenticatedHandler::handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp )
{
    throw CantHappen( "[Port-in-progress] shouldn't have reached AuthenticatedHandler::handleFlags");
}

void AuthenticatedHandler::handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "SEARCH response in authenticated state", *resp );
}

void AuthenticatedHandler::handleSort( Imap::Parser* ptr, const Imap::Responses::Sort* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "SORT response in authenticated state", *resp );
}

void AuthenticatedHandler::handleThread( Imap::Parser* ptr, const Imap::Responses::Thread* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "THREAD response in authenticated state", *resp );
}

void AuthenticatedHandler::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "FETCH response in authenticated state", *resp );
}

}
}
