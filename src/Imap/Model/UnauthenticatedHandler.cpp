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
    using namespace Imap::Responses;
    // Check for common stuff like ALERT and CAPABILITIES update
    switch ( resp->respCode ) {
        case ALERT:
            {
                emit m->alertReceived( tr("The server sent the following ALERT:\n%1").arg( resp->message ) );
            }
            break;
        case CAPABILITIES:
            {
                const RespData<QStringList>* const caps = dynamic_cast<const RespData<QStringList>* const>(
                        resp->respCodeData.data() );
                if ( caps ) {
                    m->_parsers[ ptr ].capabilities = caps->data;
                    m->_parsers[ ptr ].capabilitiesFresh = true;
                    m->updateCapabilities( ptr, caps->data );
                }
            }
            break;
        case BADCHARSET:
        case PARSE:
            qDebug() << "The server was having troubles with parsing message data:" << resp->message;
            break;
        default:
            // do nothing here, it must be handled later
            break;
    }
}

void UnauthenticatedHandler::handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    handleResponseCode( ptr, resp );

    if ( ! resp->tag.isEmpty() ) {
        throw Imap::UnexpectedResponseReceived(
                "Waiting for initial OK/BYE/PREAUTH, but got tagged response instead",
                *resp );
    }

    using namespace Imap::Responses;
    switch ( resp->kind ) {
        case PREAUTH:
        {
            m->changeConnectionState( ptr, CONN_STATE_AUTHENTICATED);
            m->_parsers[ ptr ].responseHandler = m->authenticatedHandler;
            if ( ! m->_parsers[ ptr ].capabilitiesFresh ) {
                CommandHandle cmd = ptr->capability();
                m->_parsers[ ptr ].commandMap[ cmd ] = Model::Task( Model::Task::CAPABILITY, 0 );
                emit m->activityHappening( true );
            }
            //CommandHandle cmd = ptr->namespaceCommand();
            //m->_parsers[ ptr ].commandMap[ cmd ] = Model::Task( Model::Task::NAMESPACE, 0 );
            ptr->authStateReached();
            break;
        }
        case OK:
            if ( m->_startTls ) {
                // The STARTTLS command is already queued -> no need to issue it once again
            } else {
                // The STARTTLS surely has not been issued yet
                if ( ! m->_parsers[ ptr ].capabilitiesFresh ) {
                    CommandHandle cmd = ptr->capability();
                    m->_parsers[ ptr ].commandMap[ cmd ] = Model::Task( Model::Task::CAPABILITY, 0 );
                    emit m->activityHappening( true );
                } else if ( m->_parsers[ ptr ].capabilities.contains( QLatin1String("LOGINDISABLED") ) ) {
                    qDebug() << "Can't login yet, trying STARTTLS";
                    // ... and we are forbidden from logging in, so we have to try the STARTTLS
                    CommandHandle cmd = ptr->startTls();
                    m->_parsers[ ptr ].commandMap[ cmd ] = Model::Task( Model::Task::STARTTLS, 0 );
                    emit m->activityHappening( true );
                } else {
                    // Apparently no need for STARTTLS and we are free to login
                    m->performAuthentication( ptr );
                }
            }
            break;
        case BYE:
            m->changeConnectionState( ptr, CONN_STATE_LOGOUT );
            m->_parsers[ ptr ].responseHandler = 0;
            break;
        case BAD:
            // If it was an ALERT, we've already warned the user
            if ( resp->respCode != ALERT ) {
                emit m->alertReceived( tr("The server replied with the following BAD response:\n%1").arg( resp->message ) );
            }
            break;
        default:
            throw Imap::UnexpectedResponseReceived(
                "Waiting for initial OK/BYE/BAD/PREAUTH, but got this instead",
                *resp );
    }
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
