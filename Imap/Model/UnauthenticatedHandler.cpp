#include <QAuthenticator>
#include "MailboxTree.h"
#include "UnauthenticatedHandler.h"

namespace Imap {
namespace Mailbox {

UnauthenticatedHandler::UnauthenticatedHandler( Model* _m ): ModelStateHandler(_m)
{
}

void UnauthenticatedHandler::handleResponseCode( Imap::ParserPtr ptr, const Imap::Responses::State* const resp )
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
                        resp->respCodeData.get() );
                if ( caps ) {
                    m->_parsers[ ptr.get() ].capabilities = caps->data;
                    m->_parsers[ ptr.get() ].capabilitiesFresh = true;
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

void UnauthenticatedHandler::handleState( Imap::ParserPtr ptr, const Imap::Responses::State* const resp )
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
            m->_parsers[ ptr.get() ].connState = Model::CONN_STATE_AUTH;
            m->_parsers[ ptr.get() ].responseHandler = m->authenticatedHandler;
            break;
        case OK:
            if ( !m->_startTls ) {
                QAuthenticator auth;
                emit m->authRequested( &auth );
                if ( auth.isNull() ) {
                    emit m->connectionError( tr("Can't login without user/password data") );
                } else {
                    CommandHandle cmd = ptr->login( auth.user(), auth.password() );
                    m->_parsers[ ptr.get() ].commandMap[ cmd ] = Model::Task( Model::Task::LOGIN, 0 );
                }
            }
            break;
        case BYE:
            m->_parsers[ ptr.get() ].connState = Model::CONN_STATE_LOGOUT;
            m->_parsers[ ptr.get() ].responseHandler = 0;
            break;
        default:
            throw Imap::UnexpectedResponseReceived(
                "Waiting for initial OK/BYE/PREAUTH, but got this instead",
                *resp );
    }
}

void UnauthenticatedHandler::handleCapability( Imap::ParserPtr ptr, const Imap::Responses::Capability* const resp )
{
    m->_parsers[ ptr.get() ].capabilities = resp->capabilities;
    m->_parsers[ ptr.get() ].capabilitiesFresh = true;
}

void UnauthenticatedHandler::handleNumberResponse( Imap::ParserPtr ptr, const Imap::Responses::NumberResponse* const resp )
{
    throw UnexpectedResponseReceived( "Numeric reply in unauthenticated state", *resp );
}

void UnauthenticatedHandler::handleList( Imap::ParserPtr ptr, const Imap::Responses::List* const resp )
{
    throw UnexpectedResponseReceived( "LIST reply in unauthenticated state", *resp );
}

void UnauthenticatedHandler::handleFlags( Imap::ParserPtr ptr, const Imap::Responses::Flags* const resp )
{
    throw UnexpectedResponseReceived( "FLAGS reply in unauthenticated state", *resp );
}

void UnauthenticatedHandler::handleSearch( Imap::ParserPtr ptr, const Imap::Responses::Search* const resp )
{
    throw UnexpectedResponseReceived( "SEARCH reply in unauthenticated state", *resp );
}

void UnauthenticatedHandler::handleStatus( Imap::ParserPtr ptr, const Imap::Responses::Status* const resp )
{
    throw UnexpectedResponseReceived( "STATUS reply in unauthenticated state", *resp );
}

void UnauthenticatedHandler::handleFetch( Imap::ParserPtr ptr, const Imap::Responses::Fetch* const resp )
{
    throw UnexpectedResponseReceived( "FETCH reply in unauthenticated state", *resp );
}

void UnauthenticatedHandler::handleNamespace( Imap::ParserPtr ptr, const Imap::Responses::Namespace* resp )
{
    throw UnexpectedResponseReceived( "NAMESPACE reply in unauthenticated state", *resp );
}

}
}

#include "UnauthenticatedHandler.moc"
