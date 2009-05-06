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
    m->authenticatedHandler->handleState( ptr, resp );
}

void SelectedHandler::handleNumberResponse( Imap::ParserPtr ptr, const Imap::Responses::NumberResponse* const resp )
{
    m->authenticatedHandler->handleNumberResponse( ptr, resp );

    if ( resp->kind == Imap::Responses::EXPUNGE ) {
        Model::ParserState& parser = m->_parsers[ ptr.get() ];
        Q_ASSERT( parser.currentMbox );
        parser.currentMbox->handleExpunge( m, *resp );
        parser.syncState.setExists( parser.syncState.exists() - 1 );
    } else if ( resp->kind == Imap::Responses::EXISTS ) {
        // EXISTS is already updated by AuthenticatedHandler
        Model::ParserState& parser = m->_parsers[ ptr.get() ];
        Q_ASSERT( parser.currentMbox );
        if ( parser.selectingAnother == 0 ) {
            parser.currentMbox->handleExistsSynced( m, ptr, *resp );
        }
    }
}

void SelectedHandler::handleList( Imap::ParserPtr ptr, const Imap::Responses::List* const resp )
{
    m->authenticatedHandler->handleList( ptr, resp );
}

void SelectedHandler::handleFlags( Imap::ParserPtr ptr, const Imap::Responses::Flags* const resp )
{
    // FIXME
}

void SelectedHandler::handleSearch( Imap::ParserPtr ptr, const Imap::Responses::Search* const resp )
{
    // FIXME
    throw UnexpectedResponseReceived( "SEARCH reply, wtf?", *resp );
}

void SelectedHandler::handleStatus( Imap::ParserPtr ptr, const Imap::Responses::Status* const resp )
{
    // FIXME: we should check state here -- this is not really important now
    // when we don't actually SELECT/EXAMINE any mailbox, but *HAS* to be
    // changed as soon as we do so
    m->authenticatedHandler->handleStatus( ptr, resp );
}

void SelectedHandler::handleFetch( Imap::ParserPtr ptr, const Imap::Responses::Fetch* const resp )
{
    TreeItemMailbox* mailbox = m->_parsers[ ptr.get() ].currentMbox;
    if ( ! mailbox )
        throw UnexpectedResponseReceived( "Received FETCH reply, but AFAIK we haven't selected any mailbox yet", *resp );

    emit m->layoutAboutToBeChanged();

    TreeItemPart* changedPart = 0;
    TreeItemMessage* changedMessage = 0;
    mailbox->handleFetchResponse( m, *resp, &changedPart, &changedMessage );
    emit m->layoutChanged();
    if ( changedPart ) {
        QModelIndex index = m->createIndex( changedPart->row(), 0, changedPart );
        emit m->dataChanged( index, index );
    }
    if ( changedMessage ) {
        QModelIndex index = m->createIndex( changedMessage->row(), 0, changedMessage );
        emit m->dataChanged( index, index );
    }
}

}
}

#include "SelectedHandler.moc"
