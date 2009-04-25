#include <QAuthenticator>
#include "MailboxTree.h"
#include "AuthenticatedHandler.h"

namespace Imap {
namespace Mailbox {

AuthenticatedHandler::AuthenticatedHandler( Model* _m ): ModelStateHandler(_m)
{
}


void AuthenticatedHandler::handleState( Imap::ParserPtr ptr, const Imap::Responses::State* const resp )
{
    if ( resp->kind == Imap::Responses::BYE ) {
        // FIXME
        m->_parsers[ ptr.get() ].connState = Model::CONN_STATE_LOGOUT;
        m->_parsers[ ptr.get() ].responseHandler = 0;
    }
}

void AuthenticatedHandler::handleCapability( Imap::ParserPtr ptr, const Imap::Responses::Capability* const resp )
{
    // FIXME
}

void AuthenticatedHandler::handleNumberResponse( Imap::ParserPtr ptr, const Imap::Responses::NumberResponse* const resp )
{
    throw UnexpectedResponseReceived( "Number response in authenticated state", *resp );
}

void AuthenticatedHandler::handleList( Imap::ParserPtr ptr, const Imap::Responses::List* const resp )
{
    m->_parsers[ ptr.get() ].listResponses << *resp;
}

void AuthenticatedHandler::handleFlags( Imap::ParserPtr ptr, const Imap::Responses::Flags* const resp )
{
    throw UnexpectedResponseReceived( "FLAGS response in authenticated state", *resp );
}

void AuthenticatedHandler::handleSearch( Imap::ParserPtr ptr, const Imap::Responses::Search* const resp )
{
    throw UnexpectedResponseReceived( "SEARCH response in authenticated state", *resp );
}

void AuthenticatedHandler::handleStatus( Imap::ParserPtr ptr, const Imap::Responses::Status* const resp )
{
    m->unauthHandler->handleStatus( ptr, resp );
}

void AuthenticatedHandler::handleFetch( Imap::ParserPtr ptr, const Imap::Responses::Fetch* const resp )
{
    throw UnexpectedResponseReceived( "FETCH response in authenticated state", *resp );
}

void AuthenticatedHandler::handleNamespace( Imap::ParserPtr ptr, const Imap::Responses::Namespace* resp )
{
    throw UnexpectedResponseReceived( "NAMESPACE response in authenticated state", *resp );
}

}
}

#include "AuthenticatedHandler.moc"
