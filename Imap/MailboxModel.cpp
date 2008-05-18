/* Copyright (C) 2007 Jan Kundrát <jkt@gentoo.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "Imap/MailboxModel.h"
#include <QDebug>

namespace Imap {
namespace Mailbox {

MailboxModel::MailboxModel( QObject* parent, CachePtr cache,
        ParserPtr parser, const QString& mailbox, const bool readWrite,
        const ThreadAlgorithm sorting ):
    QAbstractItemModel( parent ), _cache(cache), _parser(parser), 
    _mailbox(mailbox), _threadSorting(sorting), _readWrite(readWrite),
    _state(IMAP_STATE_CONN_ESTABLISHED), _capabilitiesFresh(false)
{
    connect( _parser.get(), SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
    // FIXME :)
    // setup up parser, connect to correct mailbox,...
    // start listening for events, react to them, make signals,... (another thread?)
}

Imap::ThreadAlgorithm MailboxModel::threadSorting()
{
    return _threadSorting;
}

void MailboxModel::setThreadSorting( const ThreadAlgorithm value )
{
    _threadSorting = value;
}

void MailboxModel::responseReceived()
{
    while ( _parser->hasResponse() ) {
        std::tr1::shared_ptr<Imap::Responses::AbstractResponse> resp = _parser->getResponse();
        if ( !resp ) {
            // this shouldn't happen, but it's better to be safe anyway
            continue;
        }
        responseReceived( resp );
    }
}

void MailboxModel::responseReceived( std::tr1::shared_ptr<Imap::Responses::AbstractResponse> resp )
{
    using namespace Imap::Responses;
    const Capability* capability = dynamic_cast<const Capability*>( resp.get() );
    const State* state = dynamic_cast<const State*>( resp.get() );

    if ( capability ) {
        _capabilities = capability->capabilities;
        _capabilitiesFresh = true;
    }

    if ( state ) {
        // handle most of the work here :(
        QString tag = state->tag;
        if ( ! tag.isEmpty() ) {
            switch ( _state ) {
                case IMAP_STATE_CONN_ESTABLISHED:
                    // this is certainly unexpected here
                    break;
            }
        } else {
            switch ( _state ) {
                case IMAP_STATE_CONN_ESTABLISHED:
                    // server's initial greeting
                    handleInitial( state );
                    break;
            }
        }
    }
}

void MailboxModel::updateState( const ImapState state )
{
    _state = state;
    // FIXME: emit state change
}

void MailboxModel::handleInitial( const Imap::Responses::State* const state )
{
    using namespace Imap::Responses;

    switch ( state->kind ) {
        case PREAUTH:
            updateState( IMAP_STATE_AUTH );
            break;
        case OK:
            updateState( IMAP_STATE_NOT_AUTH );
            break;
        case BYE:
            updateState( IMAP_STATE_LOGOUT );
            break;
        default:
            throw Imap::UnexpectedResponseReceived(
                    "Waiting for initial OK/BYE/PREAUTH, but got this instead",
                    *state );
    }

    _selectTag = _readWrite ? _parser->select( _mailbox ) : _parser->examine( _mailbox );
}

}
}

#include "MailboxModel.moc"
