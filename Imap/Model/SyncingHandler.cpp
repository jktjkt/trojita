#include <QAuthenticator>
#include "MailboxTree.h"
#include "SyncingHandler.h"

namespace Imap {
namespace Mailbox {

SyncingHandler::SyncingHandler( Model* _m ): ModelStateHandler(_m)
{
}


void SyncingHandler::handleState( Imap::ParserPtr ptr, const Imap::Responses::State* const resp )
{
    if ( resp->kind == Imap::Responses::BYE ) {
        // FIXME
        m->_parsers[ ptr.get() ].connState = Model::CONN_STATE_LOGOUT;
        m->_parsers[ ptr.get() ].responseHandler = 0;
    }
}

void SyncingHandler::handleCapability( Imap::ParserPtr ptr, const Imap::Responses::Capability* const resp )
{
    // FIXME
}

void SyncingHandler::handleNumberResponse( Imap::ParserPtr ptr, const Imap::Responses::NumberResponse* const resp )
{
    // FIXME
    switch ( resp->kind ) {
        case Imap::Responses::EXISTS:
            m->_parsers[ ptr.get() ].syncState.exists = resp->number;
            break;
        case Imap::Responses::EXPUNGE:
            {
                Model::ParserState& parser = m->_parsers[ ptr.get() ];
                Q_ASSERT( parser.handler );
                Q_ASSERT( parser.handler->fetched() );
                // FIXME: delete the message
                throw 42;
            }
            break;
        case Imap::Responses::RECENT:
            m->_parsers[ ptr.get() ].syncState.recent = resp->number;
            break;
        default:
            throw CantHappen( "Got a NumberResponse of invalid kind. This is supposed to be handled in its constructor!", *resp );
    }
}

void SyncingHandler::handleList( Imap::ParserPtr ptr, const Imap::Responses::List* const resp )
{
    m->authenticatedHandler->handleList( ptr, resp );
}

void SyncingHandler::handleFlags( Imap::ParserPtr ptr, const Imap::Responses::Flags* const resp )
{
    // FIXME
}

void SyncingHandler::handleSearch( Imap::ParserPtr ptr, const Imap::Responses::Search* const resp )
{
    throw UnexpectedResponseReceived( "SEARCH response while syncing the mailbox", *resp );
}

void SyncingHandler::handleStatus( Imap::ParserPtr ptr, const Imap::Responses::Status* const resp )
{
    // FIXME

    // FIXME: we should check state here -- this is not really important now
    // when we don't actually SELECT/EXAMINE any mailbox, but *HAS* to be
    // changed as soon as we do so
    m->_parsers[ ptr.get() ].statusResponses << *resp;

}

void SyncingHandler::handleFetch( Imap::ParserPtr ptr, const Imap::Responses::Fetch* const resp )
{
    throw UnexpectedResponseReceived( "FETCH response while syncing the mailbox", *resp );
}

void SyncingHandler::handleNamespace( Imap::ParserPtr ptr, const Imap::Responses::Namespace* resp )
{
    m->unauthHandler->handleNamespace( ptr, resp );
}

}
}

#include "SyncingHandler.moc"
