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
    m->unauthHandler->handleState( ptr, resp );
}

void AuthenticatedHandler::handleCapability( Imap::ParserPtr ptr, const Imap::Responses::Capability* const resp )
{
    m->unauthHandler->handleCapability( ptr, resp );
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
    // FIXME: deprecate STATUS replies altogether?
    m->_parsers[ ptr.get() ].statusResponses << *resp;
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
