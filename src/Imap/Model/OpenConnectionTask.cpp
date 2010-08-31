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

bool OpenConnectionTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    if ( waitingForGreetings ) {
        handleInitialResponse( ptr, resp );
        return true;
    } else if ( resp->tag == capabilityCmd ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( capabilityCmd, Model::Task::CAPABILITY );
        if ( resp->kind == Responses::OK ) {
            if ( gotPreauth ) {
                _completed();
            } else {
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
        IMAP_TASK_ENSURE_VALID_COMMAND( loginCmd, Model::Task::LOGIN );
        if ( resp->kind == Responses::OK ) {
            _completed();
        } else {
            // FIXME: error handling
        }
        return true;
    } else if ( resp->tag == startTlsCmd ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( startTlsCmd, Model::Task::STARTTLS );
        if ( resp->kind == Responses::OK ) {
            loginCmd = model->performAuthentication( ptr );
        } else {
            emit model->connectionError( tr("Can't establish a secure connection to the server (STARTTLS failed). Refusing to proceed.") );
            // FIXME: error handling
        }
        return true;
    } else {
        return false;
    }
}

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
            gotPreauth = true;
            model->changeConnectionState( ptr, CONN_STATE_AUTHENTICATED);
            if ( ! model->_parsers[ ptr ].capabilitiesFresh ) {
                capabilityCmd = ptr->capability();
                model->_parsers[ ptr ].commandMap[ capabilityCmd ] = Model::Task( Model::Task::CAPABILITY, 0 );
                emit model->activityHappening( true );
            } else {
                _completed();
            }
            //CommandHandle cmd = ptr->namespaceCommand();
            //m->_parsers[ ptr ].commandMap[ cmd ] = Model::Task( Model::Task::NAMESPACE, 0 );
            ptr->authStateReached();
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
        break;
    case BAD:
        // If it was an ALERT, we've already warned the user
        if ( resp->respCode != ALERT ) {
            emit model->alertReceived( tr("The server replied with the following BAD response:\n%1").arg( resp->message ) );
        }
        break;
    default:
        throw Imap::UnexpectedResponseReceived(
                "Waiting for initial OK/BYE/BAD/PREAUTH, but got this instead",
                *resp );
    }
}

}
}
