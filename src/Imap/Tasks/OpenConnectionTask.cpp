/* Copyright (C) 2007 - 2011 Jan Kundrát <jkt@flaska.net>

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
#include "Model/TaskPresentationModel.h"

namespace Imap
{
namespace Mailbox
{

OpenConnectionTask::OpenConnectionTask(Model *model) :
    ImapTask(model), waitingForGreetings(true), gotPreauth(false)
{
    // Offline mode shall be checked by the caller who decides to create the conneciton
    Q_ASSERT(model->networkPolicy() != Model::NETWORK_OFFLINE);
    parser = new Parser(model, model->_socketFactory->create(), ++model->lastParserId);
    ParserState parserState(parser);
    connect(parser, SIGNAL(responseReceived(Imap::Parser *)), model, SLOT(responseReceived(Imap::Parser *)));
    connect(parser, SIGNAL(disconnected(Imap::Parser *,const QString)), model, SLOT(slotParserDisconnected(Imap::Parser *,const QString)));
    connect(parser, SIGNAL(connectionStateChanged(Imap::Parser *,Imap::ConnectionState)), model, SLOT(handleSocketStateChanged(Imap::Parser *,Imap::ConnectionState)));
    connect(parser, SIGNAL(sendingCommand(Imap::Parser *,QString)), model, SLOT(parserIsSendingCommand(Imap::Parser *,QString)));
    connect(parser, SIGNAL(parseError(Imap::Parser *,QString,QString,QByteArray,int)), model, SLOT(slotParseError(Imap::Parser *,QString,QString,QByteArray,int)));
    connect(parser, SIGNAL(lineReceived(Imap::Parser *,QByteArray)), model, SLOT(slotParserLineReceived(Imap::Parser *,QByteArray)));
    connect(parser, SIGNAL(lineSent(Imap::Parser *,QByteArray)), model, SLOT(slotParserLineSent(Imap::Parser *,QByteArray)));
    model->_parsers[ parser ] = parserState;
    model->m_taskModel->slotParserCreated(parser);
    markAsActiveTask();
}

OpenConnectionTask::OpenConnectionTask(Model *model, void *dummy):
    ImapTask(model)
{
    Q_UNUSED(dummy);
}

void OpenConnectionTask::perform()
{
    // nothing should happen here
}

/** @short Decide what to do next based on the received response and the current state of the connection

CONN_STATE_NONE:
CONN_STATE_HOST_LOOKUP:
CONN_STATE_CONNECTING:
 - not allowed

CONN_STATE_CONNECTED_PRETLS_PRECAPS:
 -> CONN_STATE_AUTHENTICATED iff "* PREAUTH [CAPABILITIES ...]"
    - done
 -> CONN_STATE_POSTAUTH_PRECAPS iff "* PREAUTH"
    - requesting capabilities
 -> CONN_STATE_TLS if "* OK [CAPABILITIES ...]"
    - calling STARTTLS
 -> CONN_STATE_CONNECTED_PRETLS if caps not known
    - asking for capabilities
 -> CONN_STATE_LOGIN iff capabilities are provided and LOGINDISABLED is not there and configuration doesn't want STARTTLS
    - trying to LOGIN.
 -> CONN_STATE_LOGOUT if the initial greeting asks us to leave
    - fail

CONN_STATE_CONNECTED_PRETLS: checks result of the capability command
 -> CONN_STATE_STARTTLS
    - calling STARTTLS
 -> CONN_STATE_LOGIN
    - calling login
 -> fail

CONN_STATE_STARTTLS: checks result of STARTTLS command
 -> CONN_STATE_ESTABLISHED_PRECAPS
    - asking for capabilities
 -> fail

CONN_STATE_ESTABLISHED_PRECAPS: checks for the result of capabilities
 -> CONN_STATE_LOGIN
 -> fail

CONN_STATE_POSTAUTH_PRECAPS: checks result of the capability command
*/
bool OpenConnectionTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (_dead) {
        _failed("Asked to die");
        return true;
    }
    using namespace Imap::Responses;

    if (model->accessParser(parser).connState == CONN_STATE_CONNECTED_PRETLS_PRECAPS) {
        if (!resp->tag.isEmpty()) {
            throw Imap::UnexpectedResponseReceived("Waiting for initial OK/BYE/PREAUTH, but got tagged response instead", *resp);
        }
    } else if (model->accessParser(parser).connState > CONN_STATE_CONNECTED_PRETLS_PRECAPS) {
        if (resp->tag.isEmpty()) {
            return false;
        }
    }

    switch (model->accessParser(parser).connState) {

    case CONN_STATE_AUTHENTICATED:
    case CONN_STATE_SELECTING:
    case CONN_STATE_SYNCING:
    case CONN_STATE_SELECTED:
    case CONN_STATE_FETCHING_PART:
    case CONN_STATE_FETCHING_MSG_METADATA:
    case CONN_STATE_LOGOUT:
        // These shall not ever be reached by this code
        Q_ASSERT(false);
        return false;

    case CONN_STATE_NONE:
    case CONN_STATE_HOST_LOOKUP:
    case CONN_STATE_CONNECTING:
        // Looks like the corresponding stateChanged() signal could be delayed, at least with QProcess-based sockets
    case CONN_STATE_CONNECTED_PRETLS_PRECAPS:
        // We're connected now -- this is our initial state.
    {
        switch (resp->kind) {
        case PREAUTH:
            // Cool, we're already authenticated. Now, let's see if we have to issue CAPABILITY or if we already know that
            if (model->accessParser(parser).capabilitiesFresh) {
                // We're done here
                model->changeConnectionState(parser, CONN_STATE_AUTHENTICATED);
                onComplete();
            } else {
                model->changeConnectionState(parser, CONN_STATE_POSTAUTH_PRECAPS);
                capabilityCmd = parser->capability();
            }
            return true;

        case OK:
            if (!model->accessParser(parser).capabilitiesFresh) {
                model->changeConnectionState(parser, CONN_STATE_CONNECTED_PRETLS);
                capabilityCmd = parser->capability();
            } else {
                startTlsOrLoginNow();
            }
            return true;

        case BYE:
            logout(tr("Server has closed the conection"));
            return true;

        case BAD:
            model->changeConnectionState(parser, CONN_STATE_LOGOUT);
            // If it was an ALERT, we've already warned the user
            if (resp->respCode != ALERT) {
                emit model->alertReceived(tr("The server replied with the following BAD response:\n%1").arg(resp->message));
            }
            logout(tr("Server has greeted us with a BAD response"));
            return true;

        default:
            throw Imap::UnexpectedResponseReceived("Waiting for initial OK/BYE/BAD/PREAUTH, but got this instead", *resp);
        }
        break;
    }

    case CONN_STATE_CONNECTED_PRETLS:
        // We've asked for capabilities upon the initial interaction
    {
        bool wasCaps = checkCapabilitiesResult(resp);
        if (wasCaps && !_finished) {
            startTlsOrLoginNow();
        }
        return wasCaps;
    }

    case CONN_STATE_STARTTLS:
    {
        if (resp->tag == startTlsCmd) {
            if (resp->kind == OK) {
                model->changeConnectionState(parser, CONN_STATE_ESTABLISHED_PRECAPS);
                model->accessParser(parser).capabilitiesFresh = false;
                capabilityCmd = parser->capability();
            } else {
                logout(tr("STARTTLS failed: %1").arg(resp->message));
            }
            return true;
        }
        return false;
    }

    case CONN_STATE_ESTABLISHED_PRECAPS:
        // Connection is established and we're waiting for updated capabilities
    {
        bool wasCaps = checkCapabilitiesResult(resp);
        if (wasCaps && !_finished) {
            if (model->accessParser(parser).capabilities.contains(QLatin1String("LOGINDISABLED"))) {
                logout(tr("Capabilities still contain LOGINDISABLED even after STARTTLS"));
            } else {
                model->changeConnectionState(parser, CONN_STATE_LOGIN);
                loginCmd = model->performAuthentication(parser);
            }
        }
        return wasCaps;
    }

    case CONN_STATE_LOGIN:
        // Check the result of the LOGIN command
    {
        if (resp->tag == loginCmd) {
            // The LOGIN command is finished
            if (resp->kind == OK) {
                if (resp->respCode == CAPABILITIES) {
                    // Capabilities are already known
                    model->changeConnectionState(parser, CONN_STATE_AUTHENTICATED);
                    onComplete();
                } else {
                    // Got to ask for the capabilities
                    model->changeConnectionState(parser, CONN_STATE_POSTAUTH_PRECAPS);
                    capabilityCmd = parser->capability();
                }
            } else {
                // Login failed
                QString message;
                switch (resp->respCode) {
                case Responses::UNAVAILABLE:
                    message = tr("Temporary failure because a subsystem is down.");
                    break;
                case Responses::AUTHENTICATIONFAILED:
                    message = tr("Authentication failed for some reason on which the server is "
                                 "unwilling to elaborate.  Typically, this includes \"unknown "
                                 "user\" and \"bad password\".");
                    break;
                case Responses::AUTHORIZATIONFAILED:
                    message = tr("Authentication succeeded in using the authentication identity, "
                                 "but the server cannot or will not allow the authentication "
                                 "identity to act as the requested authorization identity.");
                    break;
                case Responses::EXPIRED:
                    message = tr("Either authentication succeeded or the server no longer had the "
                                 "necessary data; either way, access is no longer permitted using "
                                 "that passphrase.  You should get a new passphrase.");
                    break;
                case Responses::PRIVACYREQUIRED:
                    message = tr("The operation is not permitted due to a lack of privacy.");
                    break;
                case Responses::CONTACTADMIN:
                    message = tr("You should contact the system administrator or support desk.");
                    break;
                default:
                    break;
                }

                if (message.isEmpty()) {
                    message = tr("Login failed: %1").arg(resp->message);
                } else {
                    message = tr("%1\r\n\r\n%2").arg(message, resp->message);
                }
                model->emitAuthFailed(message);
                if (model->accessParser(parser).connState == CONN_STATE_LOGOUT) {
                    // The server has closed the conenction
                    _failed(QString::fromAscii("Connection closed after a failed login"));
                    return true;
                }
                loginCmd = model->performAuthentication(parser);

                if (loginCmd == CommandHandle()) {
                    // The user has given up
                    logout(tr("No credentials returned in response to a direct request to the user"));
                } else {
                    // This is not a failure yet; we're retrying again
                }
            }
            return true;
        }
        return false;
    }

    case CONN_STATE_POSTAUTH_PRECAPS:
    {
        bool wasCaps = checkCapabilitiesResult(resp);
        if (wasCaps && !_finished) {
            model->changeConnectionState(parser, CONN_STATE_AUTHENTICATED);
            onComplete();
        }
        return wasCaps;
    }

    }
}

/** @short Either call STARTTLS or go ahead and try to LOGIN */
void OpenConnectionTask::startTlsOrLoginNow()
{
    if (model->_startTls || model->accessParser(parser).capabilities.contains(QLatin1String("LOGINDISABLED"))) {
        // Should run STARTTLS later and already have the capabilities
        Q_ASSERT(model->accessParser(parser).capabilitiesFresh);
        if (!model->accessParser(parser).capabilities.contains(QLatin1String("STARTTLS"))) {
            logout(tr("Server does not support STARTTLS"));
        } else {
            startTlsCmd = parser->startTls();
            model->changeConnectionState(parser, CONN_STATE_STARTTLS);
        }
    } else {
        // We're requested to authenticate even without STARTTLS
        Q_ASSERT(!model->accessParser(parser).capabilities.contains(QLatin1String("LOGINDISABLED")));
        model->changeConnectionState(parser, CONN_STATE_LOGIN);
        loginCmd = model->performAuthentication(parser);
    }
}

bool OpenConnectionTask::checkCapabilitiesResult(const Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == capabilityCmd) {
        if (!model->accessParser(parser).capabilitiesFresh) {
            logout(tr("Server did not provide useful capabilities"));
            return true;
        }
        if (resp->kind != Responses::OK) {
            logout(tr("CAPABILITIES command has failed"));
        }
        return true;
    }

    return false;
}

void OpenConnectionTask::onComplete()
{
    // Optionally issue the ID command
    if (model->accessParser(parser).capabilities.contains(QLatin1String("ID"))) {
        model->_taskFactory->createIdTask(model, this);
    }

    // But do terminate this task
    _completed();
}

void OpenConnectionTask::logout(const QString &message)
{
    emit model->connectionError(message);
    model->changeConnectionState(parser, CONN_STATE_LOGOUT);
    model->accessParser(parser).logoutCmd = parser->logout();
    _failed(message);
}

}
}
