#include <QAuthenticator>
#include "MailboxTree.h"
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
            m->_parsers[ ptr ].connState = Model::CONN_STATE_AUTH;
            m->_parsers[ ptr ].responseHandler = m->authenticatedHandler;
            if ( ! m->_parsers[ ptr ].capabilitiesFresh ) {
                CommandHandle cmd = ptr->capability();
                m->_parsers[ ptr ].commandMap[ cmd ] = Model::Task( Model::Task::CAPABILITY, 0 );
            }
            CommandHandle cmd = ptr->namespaceCommand();
            m->_parsers[ ptr ].commandMap[ cmd ] = Model::Task( Model::Task::NAMESPACE, 0 );
            break;
        }
        case OK:
            if ( !m->_startTls ) {
                // FIXME: obey LOGINDISABLED
                QAuthenticator auth;
                emit m->authRequested( &auth );
                if ( auth.isNull() ) {
                    emit m->connectionError( tr("Can't login without user/password data") );
                } else {
                    CommandHandle cmd = ptr->login( auth.user(), auth.password() );
                    m->_parsers[ ptr ].commandMap[ cmd ] = Model::Task( Model::Task::LOGIN, 0 );
                }
            }
            break;
        case BYE:
            m->_parsers[ ptr ].connState = Model::CONN_STATE_LOGOUT;
            m->_parsers[ ptr ].responseHandler = 0;
            break;
        default:
            throw Imap::UnexpectedResponseReceived(
                "Waiting for initial OK/BYE/PREAUTH, but got this instead",
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

void UnauthenticatedHandler::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "FETCH reply in unauthenticated state", *resp );
}

}
}

#include "UnauthenticatedHandler.moc"
