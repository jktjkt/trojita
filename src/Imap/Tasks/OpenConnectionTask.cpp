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

#include "OpenConnectionTask.h"
#include <QTimer>
#include "IdleLauncher.h"

namespace Imap {
namespace Mailbox {

OpenConnectionTask::OpenConnectionTask( Model* _model ) :
    ImapTask( _model ), waitingForGreetings(true), gotPreauth(false)
{    
    parser = new Parser( model, model->_socketFactory->create(), ++model->lastParserId );
    Model::ParserState parserState = Model::ParserState( parser, 0, Model::ReadOnly, CONN_STATE_NONE, 0 );
    parserState.idleLauncher = new IdleLauncher( model, parser );
    connect( parser, SIGNAL(responseReceived()), model, SLOT(responseReceived()) );
    connect( parser, SIGNAL(disconnected(const QString)), model, SLOT(slotParserDisconnected(const QString)) );
    connect( parser, SIGNAL(connectionStateChanged(Imap::ConnectionState)), model, SLOT(handleSocketStateChanged(Imap::ConnectionState)) );
    connect( parser, SIGNAL(sendingCommand(QString)), model, SLOT(parserIsSendingCommand(QString)) );
    connect( parser, SIGNAL(parseError(QString,QString,QByteArray,int)), model, SLOT(slotParseError(QString,QString,QByteArray,int)) );
    connect( parser, SIGNAL(lineReceived(QByteArray)), model, SLOT(slotParserLineReceived(QByteArray)) );
    connect( parser, SIGNAL(lineSent(QByteArray)), model, SLOT(slotParserLineSent(QByteArray)) );
    connect( parser, SIGNAL(idleTerminated()), model, SLOT(idleTerminated()) );
    if ( model->_startTls ) {
        startTlsCmd = parser->startTls();
        model->_parsers[ parser ].commandMap[ startTlsCmd ] = Model::Task( Model::Task::STARTTLS, 0 );
        emit model->activityHappening( true );
    }
    parserState.activeTasks.append( this );
    model->_parsers[ parser ] = parserState;
}

void OpenConnectionTask::perform()
{
    // nothing should happen here
}

/** @short Process the "state" response originating from the IMAP server */
bool OpenConnectionTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    if ( waitingForGreetings ) {
        handleInitialResponse( ptr, resp );
        return true;
    }

    if ( resp->tag.isEmpty() )
        return false;

    if ( resp->tag == capabilityCmd ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( capabilityCmd, Model::Task::CAPABILITY );
        if ( resp->kind == Responses::OK ) {
            if ( gotPreauth ) {
                // The greetings indicated that we're already in the auth state, and now we
                // know capabilities, too, so we're done here
                _completed();
            } else {
                // We want to log in, but we might have to STARTTLS before
                if ( model->_parsers[ ptr ].capabilities.contains( QLatin1String("LOGINDISABLED") ) ) {
                    qDebug() << "Can't login yet, trying STARTTLS";
                    // ... and we are forbidden from logging in, so we have to try the STARTTLS
                    startTlsCmd = ptr->startTls();
                    model->_parsers[ ptr ].commandMap[ startTlsCmd ] = Model::Task( Model::Task::STARTTLS, 0 );
                    emit model->activityHappening( true );
                } else {
                    // Apparently no need for STARTTLS and we are free to login
                    loginCmd = model->performAuthentication( ptr );
                }
            }
        } else {
            // FIXME: Tasks API error handling
        }
        IMAP_TASK_CLEANUP_COMMAND;
        return true;
    } else if ( resp->tag == loginCmd ) {
        // The LOGIN command is finished, and we know capabilities already
        Q_ASSERT( model->_parsers[ ptr ].capabilitiesFresh );
        IMAP_TASK_ENSURE_VALID_COMMAND( loginCmd, Model::Task::LOGIN );
        if ( resp->kind == Responses::OK ) {
            _completed();
        } else {
            // FIXME: error handling
        }
        IMAP_TASK_CLEANUP_COMMAND;
        return true;
    } else if ( resp->tag == startTlsCmd ) {
        // So now we've got a secure connection, but we will have to login. Additionally,
        // we are obliged to forget any capabilities.
        model->_parsers[ ptr ].capabilitiesFresh = false;
        // FIXME: why do I have to comment that out?
        //IMAP_TASK_ENSURE_VALID_COMMAND( startTlsCmd, Model::Task::STARTTLS );
        QMap<CommandHandle, Model::Task>::iterator command = model->_parsers[ ptr ].commandMap.find( startTlsCmd );
        if ( resp->kind == Responses::OK ) {
            capabilityCmd = ptr->capability();
            model->_parsers[ ptr ].commandMap[ capabilityCmd ] = Model::Task( Model::Task::CAPABILITY, 0 );
            emit model->activityHappening( true );
        } else {
            emit model->connectionError( tr("Can't establish a secure connection to the server (STARTTLS failed). Refusing to proceed.") );
            // FIXME: error handling
        }
        //IMAP_TASK_CLEANUP_COMMAND;
        if ( command != model->_parsers[ ptr ].commandMap.end() )
            model->_parsers[ ptr ].commandMap.erase( command );
        model->parsersMightBeIdling();
        return true;
    } else {
        return false;
    }
}

/** @short Helper for dealing with the very first response from the server */
void OpenConnectionTask::handleInitialResponse( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    waitingForGreetings = false;
    if ( ! resp->tag.isEmpty() ) {
        throw Imap::UnexpectedResponseReceived(
                "Waiting for initial OK/BYE/PREAUTH, but got tagged response instead",
                *resp );
    }

    using namespace Imap::Responses;
    switch ( resp->kind ) {
    case PREAUTH:
        {
            // Cool, we're already authenticated. Now, let's see if we have to issue CAPABILITY or if we already know that
            gotPreauth = true;
            model->changeConnectionState( ptr, CONN_STATE_AUTHENTICATED);
            if ( ! model->_parsers[ ptr ].capabilitiesFresh ) {
                capabilityCmd = ptr->capability();
                model->_parsers[ ptr ].commandMap[ capabilityCmd ] = Model::Task( Model::Task::CAPABILITY, 0 );
                emit model->activityHappening( true );
            } else {
                _completed();
            }
            break;
        }
    case OK:
        if ( model->_startTls ) {
            // The STARTTLS command is already queued -> no need to issue it once again
        } else {
            // The STARTTLS surely has not been issued yet
            if ( ! model->_parsers[ ptr ].capabilitiesFresh ) {
                capabilityCmd = ptr->capability();
                model->_parsers[ ptr ].commandMap[ capabilityCmd ] = Model::Task( Model::Task::CAPABILITY, 0 );
                emit model->activityHappening( true );
            } else if ( model->_parsers[ ptr ].capabilities.contains( QLatin1String("LOGINDISABLED") ) ) {
                qDebug() << "Can't login yet, trying STARTTLS";
                // ... and we are forbidden from logging in, so we have to try the STARTTLS
                startTlsCmd = ptr->startTls();
                model->_parsers[ ptr ].commandMap[ startTlsCmd ] = Model::Task( Model::Task::STARTTLS, 0 );
                emit model->activityHappening( true );
            } else {
                // Apparently no need for STARTTLS and we are free to login
                loginCmd = model->performAuthentication( ptr );
            }
        }
        break;
    case BYE:
        model->changeConnectionState( ptr, CONN_STATE_LOGOUT );
        model->_parsers[ ptr ].responseHandler = 0;
        // FIXME: Tasks error handling
        break;
    case BAD:
        // If it was an ALERT, we've already warned the user
        if ( resp->respCode != ALERT ) {
            emit model->alertReceived( tr("The server replied with the following BAD response:\n%1").arg( resp->message ) );
        }
        // FIXME: Tasks error handling
        break;
    default:
        throw Imap::UnexpectedResponseReceived(
                "Waiting for initial OK/BYE/BAD/PREAUTH, but got this instead",
                *resp );
    }
}

}
}
