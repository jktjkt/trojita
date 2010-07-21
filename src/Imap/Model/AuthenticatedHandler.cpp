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
    static_cast<UnauthenticatedHandler*>( m->unauthHandler )->handleResponseCode( ptr, resp );

    switch ( resp->respCode ) {
        case Responses::UNSEEN:
        {
            const Responses::RespData<uint>* const num = dynamic_cast<const Responses::RespData<uint>* const>( resp->respCodeData.data() );
            if ( num )
                m->_parsers[ ptr ].currentMbox->syncState.setUnSeen( num->data );
            else
                throw CantHappen( "State response has invalid UNSEEN respCodeData", *resp );
            break;
        }
        case Responses::PERMANENTFLAGS:
        {
            const Responses::RespData<QStringList>* const num = dynamic_cast<const Responses::RespData<QStringList>* const>( resp->respCodeData.data() );
            if ( num )
                m->_parsers[ ptr ].currentMbox->syncState.setPermanentFlags( num->data );
            else
                throw CantHappen( "State response has invalid PERMANENTFLAGS respCodeData", *resp );
            break;
        }
        case Responses::UIDNEXT:
        {
            const Responses::RespData<uint>* const num = dynamic_cast<const Responses::RespData<uint>* const>( resp->respCodeData.data() );
            if ( num )
                m->_parsers[ ptr ].currentMbox->syncState.setUidNext( num->data );
            else
                throw CantHappen( "State response has invalid UIDNEXT respCodeData", *resp );
            break;
        }
        case Responses::UIDVALIDITY:
        {
            const Responses::RespData<uint>* const num = dynamic_cast<const Responses::RespData<uint>* const>( resp->respCodeData.data() );
            if ( num )
                m->_parsers[ ptr ].currentMbox->syncState.setUidValidity( num->data );
            else
                throw CantHappen( "State response has invalid UIDVALIDITY respCodeData", *resp );
            break;
        }
            break;
        default:
            break;
    }
}

void AuthenticatedHandler::handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp )
{
    switch ( resp->kind ) {
        case Imap::Responses::EXISTS:
            m->_parsers[ ptr ].currentMbox->syncState.setExists( resp->number );
            break;
        case Imap::Responses::EXPUNGE:
            // must be handled elsewhere
            break;
        case Imap::Responses::RECENT:
            m->_parsers[ ptr ].currentMbox->syncState.setRecent( resp->number );
            break;
        default:
            throw CantHappen( "Got a NumberResponse of invalid kind. This is supposed to be handled in its constructor!", *resp );
    }
}

void AuthenticatedHandler::handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp )
{
    m->_parsers[ ptr ].listResponses << *resp;
}

void AuthenticatedHandler::handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp )
{
    m->_parsers[ ptr ].currentMbox->syncState.setFlags( resp->flags );
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
