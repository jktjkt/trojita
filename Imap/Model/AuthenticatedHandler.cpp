#include <QAuthenticator>
#include "MailboxTree.h"
#include "AuthenticatedHandler.h"
#include "UnauthenticatedHandler.h"

namespace Imap {
namespace Mailbox {

AuthenticatedHandler::AuthenticatedHandler( Model* _m ): ModelStateHandler(_m)
{
}


void AuthenticatedHandler::handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    static_cast<UnauthenticatedHandler*>( m->unauthHandler )->handleResponseCode( ptr, resp );

    switch ( resp->respCode ) {
        case Responses::UNSEEN:
        {
            const Responses::RespData<uint>* const num = dynamic_cast<const Responses::RespData<uint>* const>( resp->respCodeData.data() );
            if ( num )
                m->_parsers[ ptr ].syncState.setUnSeen( num->data );
            else
                throw CantHappen( "State response has invalid UNSEEN respCodeData", *resp );
            break;
        }
        case Responses::PERMANENTFLAGS:
        {
            const Responses::RespData<QStringList>* const num = dynamic_cast<const Responses::RespData<QStringList>* const>( resp->respCodeData.data() );
            if ( num )
                m->_parsers[ ptr ].syncState.setPermanentFlags( num->data );
            else
                throw CantHappen( "State response has invalid PERMANENTFLAGS respCodeData", *resp );
            break;
        }
        case Responses::UIDNEXT:
        {
            const Responses::RespData<uint>* const num = dynamic_cast<const Responses::RespData<uint>* const>( resp->respCodeData.data() );
            if ( num )
                m->_parsers[ ptr ].syncState.setUidNext( num->data );
            else
                throw CantHappen( "State response has invalid UIDNEXT respCodeData", *resp );
            break;
        }
        case Responses::UIDVALIDITY:
        {
            const Responses::RespData<uint>* const num = dynamic_cast<const Responses::RespData<uint>* const>( resp->respCodeData.data() );
            if ( num )
                m->_parsers[ ptr ].syncState.setUidValidity( num->data );
            else
                throw CantHappen( "State response has invalid UIDVALIDITY respCodeData", *resp );
            break;
        }
            break;
        default:
            break;
    }
}

void AuthenticatedHandler::handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp )
{
    switch ( resp->kind ) {
        case Imap::Responses::EXISTS:
            m->_parsers[ ptr ].syncState.setExists( resp->number );
            break;
        case Imap::Responses::EXPUNGE:
            // must be handled elsewhere
            break;
        case Imap::Responses::RECENT:
            m->_parsers[ ptr ].syncState.setRecent( resp->number );
            break;
        default:
            throw CantHappen( "Got a NumberResponse of invalid kind. This is supposed to be handled in its constructor!", *resp );
    }
}

void AuthenticatedHandler::handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp )
{
    m->_parsers[ ptr ].listResponses << *resp;
}

void AuthenticatedHandler::handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp )
{
    m->_parsers[ ptr ].syncState.setFlags( resp->flags );
}

void AuthenticatedHandler::handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "SEARCH response in authenticated state", *resp );
}

void AuthenticatedHandler::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    Q_UNUSED( ptr );
    throw UnexpectedResponseReceived( "FETCH response in authenticated state", *resp );
}

}
}

#include "AuthenticatedHandler.moc"
