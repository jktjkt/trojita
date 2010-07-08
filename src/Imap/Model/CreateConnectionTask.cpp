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

#include "CreateConnectionTask.h"
#include "Model.h"
#include <QTimer>

namespace Imap {
namespace Mailbox {

CreateConnectionTask::CreateConnectionTask( Model* _model, TreeItemMailbox* mailbox ) :
    ImapTask( _model ), immediately( ! mailbox )
{
    parser = model->_getParser( mailbox, Model::ReadWrite );
    // This is a special case, because we do not depend on any other job.
    // Therefore, we want to call perform() immediately.
    perform();
}

void CreateConnectionTask::perform()
{
    model->_parsers[ parser ].activeTasks.append( this );
    // FIXME: In future, this should be replaced by proper SELECTs etc
    if ( immediately )
        QTimer::singleShot( 0, this, SLOT(_hackSignalCompletion()) );
    else
        QTimer::singleShot( 500, this, SLOT(_hackSignalCompletion()) );
    // The reason for the second call is that we do really want not
    // to postpone the execution of various dependant tasks "too much".
    // An unfortunate side effect of the tasks still not being fully
    // implemented is that Trojita might seem a bit slow, some tasks
    // will have a 500ms delay associated with them. This should be
    // fixed when we switch to proper dependency tracking.
}

void CreateConnectionTask::_hackSignalCompletion()
{
    _completed();
}

bool CreateConnectionTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    return false;
    // FIXME: the code below should be moved from Model's implementation here
#if 0
    if ( ! resp->tag.isEmpty() ) {
        throw Imap::UnexpectedResponseReceived(
                "Waiting for initial OK/BYE/PREAUTH, but got tagged response instead",
                *resp );
    }

    using namespace Imap::Responses;
    switch ( resp->kind ) {
        case PREAUTH:
        {
            model->changeConnectionState( ptr, CONN_STATE_AUTHENTICATED);
            model->_parsers[ ptr ].responseHandler = m->authenticatedHandler;
            if ( ! model->_parsers[ ptr ].capabilitiesFresh ) {
                CommandHandle cmd = ptr->capability();
                model->_parsers[ ptr ].commandMap[ cmd ] = Model::Task( Model::Task::CAPABILITY, 0 );
                emit model->activityHappening( true );
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
                    CommandHandle cmd = ptr->capability();
                    model->_parsers[ ptr ].commandMap[ cmd ] = Model::Task( Model::Task::CAPABILITY, 0 );
                    emit model->activityHappening( true );
                } else if ( model->_parsers[ ptr ].capabilities.contains( QLatin1String("LOGINDISABLED") ) ) {
                    qDebug() << "Can't login yet, trying STARTTLS";
                    // ... and we are forbidden from logging in, so we have to try the STARTTLS
                    CommandHandle cmd = ptr->startTls();
                    model->_parsers[ ptr ].commandMap[ cmd ] = Model::Task( Model::Task::STARTTLS, 0 );
                    emit model->activityHappening( true );
                } else {
                    // Apparently no need for STARTTLS and we are free to login
                    model->performAuthentication( ptr );
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
    return true;
#endif
}

}
}
