#include <QAuthenticator>
#include "MailboxTree.h"
#include "SelectedHandler.h"

namespace Imap {
namespace Mailbox {

SelectedHandler::SelectedHandler( Model* _m ): ModelStateHandler(_m)
{
}


void SelectedHandler::handleState( Imap::ParserPtr ptr, const Imap::Responses::State* const resp )
{
    if ( resp->kind == Imap::Responses::BYE ) {
        // FIXME
        m->_parsers[ ptr.get() ].connState = Model::CONN_STATE_LOGOUT;
        m->_parsers[ ptr.get() ].responseHandler = 0;
    }
}

void SelectedHandler::handleCapability( Imap::ParserPtr ptr, const Imap::Responses::Capability* const resp )
{
    // FIXME
}

void SelectedHandler::handleNumberResponse( Imap::ParserPtr ptr, const Imap::Responses::NumberResponse* const resp )
{
    switch ( resp->kind ) {
        case Imap::Responses::EXISTS:
            // FIXME: exception
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
            // FIXME: exception
            m->_parsers[ ptr.get() ].syncState.recent = resp->number;
            break;
        default:
            throw CantHappen( "Got a NumberResponse of invalid kind. This is supposed to be handled in its constructor!", *resp );
    }
}

void SelectedHandler::handleList( Imap::ParserPtr ptr, const Imap::Responses::List* const resp )
{
    m->authenticatedHandler->handleList( ptr, resp );
}

void SelectedHandler::handleFlags( Imap::ParserPtr ptr, const Imap::Responses::Flags* const resp )
{
    // FIXME
    throw UnexpectedResponseReceived( "Got FLAGS reply after having synced", *resp );
}

void SelectedHandler::handleSearch( Imap::ParserPtr ptr, const Imap::Responses::Search* const resp )
{
    // FIXME
    throw UnexpectedResponseReceived( "SEARCH reply, wtf?", *resp );
}

void SelectedHandler::handleStatus( Imap::ParserPtr ptr, const Imap::Responses::Status* const resp )
{
    m->unauthHandler->handleStatus( ptr, resp );
}

void SelectedHandler::handleFetch( Imap::ParserPtr ptr, const Imap::Responses::Fetch* const resp )
{
    TreeItemMailbox* mailbox = m->_parsers[ ptr.get() ].handler;
    if ( ! mailbox )
        throw UnexpectedResponseReceived( "Received FETCH reply, but AFAIK we haven't selected any mailbox yet", *resp );

    emit m->layoutAboutToBeChanged();

    TreeItemPart* changedPart = 0;
    mailbox->handleFetchResponse( m, *resp, &changedPart );
    emit m->layoutChanged();
    if ( changedPart ) {
        QModelIndex index = m->createIndex( changedPart->row(), 0, changedPart );
        emit m->dataChanged( index, index );
    }
}

void SelectedHandler::handleNamespace( Imap::ParserPtr ptr, const Imap::Responses::Namespace* resp )
{
    m->unauthHandler->handleNamespace( ptr, resp );
}

}
}

#include "SelectedHandler.moc"
