#include <QAuthenticator>
#include "MailboxTree.h"
#include "AuthenticatedHandler.h"
#include "UnauthenticatedHandler.h"

namespace Imap {
namespace Mailbox {

AuthenticatedHandler::AuthenticatedHandler( Model* _m ): ModelStateHandler(_m)
{
}


void AuthenticatedHandler::handleState( Imap::ParserPtr ptr, const Imap::Responses::State* const resp )
{
    static_cast<UnauthenticatedHandler*>( m->unauthHandler )->handleResponseCode( ptr, resp );

    switch ( resp->respCode ) {
        case Responses::UNSEEN:
        {
            const Responses::RespData<uint>* const num = dynamic_cast<const Responses::RespData<uint>* const>( resp->respCodeData.get() );
            if ( num )
                m->_parsers[ ptr.get() ].syncState.setUnSeen( num->data );
            else
                throw CantHappen( "State response has invalid UNSEEN respCodeData", *resp );
            break;
        }
        case Responses::PERMANENTFLAGS:
        {
            const Responses::RespData<QStringList>* const num = dynamic_cast<const Responses::RespData<QStringList>* const>( resp->respCodeData.get() );
            if ( num )
                m->_parsers[ ptr.get() ].syncState.setPermanentFlags( num->data );
            else
                throw CantHappen( "State response has invalid PERMANENTFLAGS respCodeData", *resp );
            break;
        }
        case Responses::UIDNEXT:
        {
            const Responses::RespData<uint>* const num = dynamic_cast<const Responses::RespData<uint>* const>( resp->respCodeData.get() );
            if ( num )
                m->_parsers[ ptr.get() ].syncState.setUidNext( num->data );
            else
                throw CantHappen( "State response has invalid UIDNEXT respCodeData", *resp );
            break;
        }
        case Responses::UIDVALIDITY:
        {
            const Responses::RespData<uint>* const num = dynamic_cast<const Responses::RespData<uint>* const>( resp->respCodeData.get() );
            if ( num )
                m->_parsers[ ptr.get() ].syncState.setUidValidity( num->data );
            else
                throw CantHappen( "State response has invalid UIDVALIDITY respCodeData", *resp );
            break;
        }
            break;
        default:
            break;
    }
}

void AuthenticatedHandler::handleCapability( Imap::ParserPtr ptr, const Imap::Responses::Capability* const resp )
{
    m->unauthHandler->handleCapability( ptr, resp );
}

void AuthenticatedHandler::handleNumberResponse( Imap::ParserPtr ptr, const Imap::Responses::NumberResponse* const resp )
{
    switch ( resp->kind ) {
        case Imap::Responses::EXISTS:
            m->_parsers[ ptr.get() ].syncState.setExists( resp->number );
            break;
        case Imap::Responses::EXPUNGE:
            // must be handled elsewhere
            break;
        case Imap::Responses::RECENT:
            m->_parsers[ ptr.get() ].syncState.setRecent( resp->number );
            break;
        default:
            throw CantHappen( "Got a NumberResponse of invalid kind. This is supposed to be handled in its constructor!", *resp );
    }
}

void AuthenticatedHandler::handleList( Imap::ParserPtr ptr, const Imap::Responses::List* const resp )
{
    m->_parsers[ ptr.get() ].listResponses << *resp;
}

void AuthenticatedHandler::handleFlags( Imap::ParserPtr ptr, const Imap::Responses::Flags* const resp )
{
    m->_parsers[ ptr.get() ].syncState.setFlags( resp->flags );
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
