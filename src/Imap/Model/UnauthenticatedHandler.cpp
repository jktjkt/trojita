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
#include "UnauthenticatedHandler.h"

namespace Imap {
namespace Mailbox {

UnauthenticatedHandler::UnauthenticatedHandler( Model* _m ): ModelStateHandler(_m)
{
}

void UnauthenticatedHandler::handleResponseCode( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    throw CantHappen("UnauthenticatedHandler::handleResponseCode: porting in progress...");
}

void UnauthenticatedHandler::handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    throw CantHappen("UnauthenticatedHandler::handleState: porting in progress...");
}

void UnauthenticatedHandler::handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "Numeric reply in unauthenticated state", *resp );
}

void UnauthenticatedHandler::handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "LIST reply in unauthenticated state", *resp );
}

void UnauthenticatedHandler::handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "FLAGS reply in unauthenticated state", *resp );
}

void UnauthenticatedHandler::handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "SEARCH reply in unauthenticated state", *resp );
}

void UnauthenticatedHandler::handleSort( Imap::Parser* ptr, const Imap::Responses::Sort* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "SORT response in unauthenticated state", *resp );
}

void UnauthenticatedHandler::handleThread( Imap::Parser* ptr, const Imap::Responses::Thread* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "THREAD reply in unauthenticated state", *resp );
}

void UnauthenticatedHandler::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "FETCH reply in unauthenticated state", *resp );
}

}
}
