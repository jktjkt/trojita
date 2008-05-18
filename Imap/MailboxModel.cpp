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
        AuthenticatorPtr authenticator, ParserPtr parser,
        const QString& mailbox, const bool readWrite,
        const ThreadAlgorithm sorting ):
    // parent
    QAbstractItemModel( parent ),
    // our tools
    _cache(cache), _authenticator(authenticator), _parser(parser),
    // parameters
    _mailbox(mailbox), _threadSorting(sorting), _readWrite(readWrite),
    // internal state
    _state(IMAP_STATE_CONN_ESTABLISHED),
    // mailbox metadata
    _exists(0), _recent(0), _unSeen(0), _uidNext(0), _uidValidity(0),
    // freshness
    _existsDone(false), _recentDone(false), _unSeenDone(false),
    _uidNextDone(false), _uidValidityDone(false), _flagsDone(false),
    _permanentFlagsDone(false),
    _capabilitiesFresh(false),
    // internal state again
    _waitingForSelect(false)
{
    connect( _parser.get(), SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
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
        QTextStream s(stderr);
        s << "<<< " << *resp << "\r\n";
        s.flush();
        resp->plug( this ); // make use of RTTI instead of messing with dynamic_cast<> by hand
    }
}

void MailboxModel::handleFetch(Imap::Responses::Fetch const* resp )
{
    // FIXME
    throw UnexpectedResponseReceived( "FETCH reply, wtf?", *resp );

}

void MailboxModel::handleStatus(Imap::Responses::Status const* resp )
{
    // FIXME
    throw UnexpectedResponseReceived( "STATUS reply, wtf?", *resp );
}

void MailboxModel::handleSearch(Imap::Responses::Search const* resp )
{
    // FIXME
    throw UnexpectedResponseReceived( "SEARCH reply, wtf?", *resp );
}

void MailboxModel::handleFlags(Imap::Responses::Flags const* resp )
{
    switch ( _state ) {
        case IMAP_STATE_SELECTING:
            _flags = resp->flags;
            _flagsDone = true;
            break;
        default:
            throw UnexpectedResponseReceived( "FLAGS reply, wtf?", *resp );
    }
}

void MailboxModel::handleList(Imap::Responses::List const* resp )
{
    // FIXME
    throw UnexpectedResponseReceived( "LIST reply, wtf?", *resp );
}

void MailboxModel::handleNumberResponse(Imap::Responses::NumberResponse const* resp )
{
    using namespace Imap::Responses;

    switch ( _state ) {
        case IMAP_STATE_SELECTING:
            switch ( resp->kind ) {
                case EXISTS:
                    _exists = resp->number;
                    _existsDone = true;
                    break;
                case RECENT:
                    _recent = resp->number;
                    _recentDone = true;
                    break;
                default:
                    throw UnexpectedResponseReceived( 
                            "NUMBER reply of weird kind when SELECTing", 
                            *resp );
            }
            break;
        default:
            throw UnexpectedResponseReceived( "NUMBER reply, wtf?", *resp );
    }
}

void MailboxModel::handleCapability(Imap::Responses::Capability const* r_capability )
{
    _capabilities = r_capability->capabilities;
    _capabilitiesFresh = true;
}

void MailboxModel::handleState(Imap::Responses::State const* r_state )
{
    using namespace Imap::Responses;

    // Check for common stuff like ALERT and CAPABILITIES update
    switch ( r_state->respCode ) {
        case ALERT:
            {
                const RespData<QString>* const msg = dynamic_cast<const RespData<QString>* const>(
                        r_state->respCodeData.get() );
                alert( r_state, msg ? msg->data : QString() );
            }
            break;
        case CAPABILITIES:
            {
                const RespData<QStringList>* const caps = dynamic_cast<const RespData<QStringList>* const>(
                        r_state->respCodeData.get() );
                if ( caps ) {
                    _capabilities = caps->data;
                    _capabilitiesFresh = true;
                }
            }
            break;
        default:
            // do nothing here, it must be handled later
            break;
    }

    QString tag = r_state->tag;

    switch ( _state ) {
        case IMAP_STATE_CONN_ESTABLISHED:
            if ( ! tag.isEmpty() )
                throw UnexpectedResponseReceived( 
                        "Received a tagged response when expecting server greeting", 
                        *r_state );
            else
                handleInitial( r_state );
            break;
        case IMAP_STATE_NOT_AUTH:
            // either I'm terribly mistaken or this is really guarded by the
            // authenticate() function, so that it can't happen :)
            throw UnexpectedResponseReceived(
                    "Somehow we managed to get back to the "
                    "IMAP_STATE_NOT_AUTH, which is rather confusing", 
                    *r_state );
            break;
        case IMAP_STATE_AUTH:
            handleStateAuthenticated( r_state );
            break;
        case IMAP_STATE_SELECTING:
            handleStateSelecting( r_state );
            break;
        case IMAP_STATE_SELECTED:
            handleStateSelected( r_state );
            break;
        case IMAP_STATE_LOGOUT:
            // hey, we're supposed to be logged out, how come that
            // *anything* made it here?
            throw UnexpectedResponseReceived(
                    "WTF, we're logged out, yet I just got this message", 
                    *r_state );
            break;
    }
}

void MailboxModel::handleStateAuthenticated( const Imap::Responses::State* const state )
{
    // FIXME
    throw UnexpectedResponseReceived( "AUTHENTICATED state, tagged reply, wtf?", *state );
}

void MailboxModel::handleStateSelecting( const Imap::Responses::State* const state )
{
    using namespace Imap::Responses;

    switch ( state->kind ) {
        case BAD:
            throw UnexpectedResponseReceived(
                    "Got a BAD result when trying to SELECT/EXAMINE a mailbox",
                    *state );
            break;
        case NO:
            updateState( IMAP_STATE_AUTH );
            // FIXME: throw an exception?
            break;
        case OK:
            if ( state->tag.isEmpty() ) {
                switch ( state->respCode ) {
                    case UNSEEN:
                        _unSeen = dynamic_cast<const RespData<uint>* const>( state->respCodeData.get() )->data;
                        _unSeenDone = true;
                        break;
                    case PERMANENTFLAGS:
                        _permanentFlags = dynamic_cast<const RespData<QStringList>* const>( state->respCodeData.get() )->data;
                        _permanentFlagsDone = true;
                        break;
                    case UIDNEXT:
                        _uidNext = dynamic_cast<const RespData<uint>* const>( state->respCodeData.get() )->data;
                        _uidNextDone = true;
                        break;
                    case UIDVALIDITY:
                        _uidValidity = dynamic_cast<const RespData<uint>* const>( state->respCodeData.get() )->data;
                        _uidValidityDone = true;
                        break;
                    default:
                        unknownResponseCode( state );
                }
            } else {
                if ( ! ( _existsDone && _recentDone && _flagsDone ) )
                    throw ServerError( "Server didn't provide all required fields for mailbox status",
                            *state );
                if ( ! ( _unSeenDone && _uidNextDone && _uidValidityDone && _permanentFlagsDone ) ) {
                    /*throw ServerError( "Server is conforming to an old standard only, it didn't provide us"
                            " with all required information of mailbox status as per RFC3501. We should've"
                            " handled this, but it isn't implemented yet.", *state );*/
                    // well, it's an allowed behavior :)
                }
                // FIXME: cache re-sync, RO/RW update,...
                updateState( IMAP_STATE_SELECTED );
            }
            break;
        case BYE:
            // FIXME: throw an exception, inform all clients that we are going
            // to die
            updateState( IMAP_STATE_LOGOUT );
            break;
        default:
            throw UnexpectedResponseReceived(
                    "Got strange STATE response when awaiting SELECT/EXAMINE command completion",
                    *state );
            break;
    }
}

void MailboxModel::handleStateSelected( const Imap::Responses::State* const state )
{
    // FIXME
    throw UnexpectedResponseReceived( "SELECTED state, tagged reply, wtf?", *state );
}

void MailboxModel::updateState( const ImapState state )
{
    _state = state;
    // FIXME: emit state change signal
    QTextStream s(stderr);
    s << "Updating state to " << state << "\r\n";
    s.flush();
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

    switch ( state->respCode ) {
        case ALERT:
        case CAPABILITIES:
            // already handled in the responseReceived()
            break;
        default:
            // nothing else is expected here
            unknownResponseCode( state );
    }

    switch ( _state ) {
        case IMAP_STATE_AUTH:
            select();
            break;
        case IMAP_STATE_NOT_AUTH:
            authenticate();
            select();
            break;
        default:
            throw Imap::UnexpectedResponseReceived( "It seems that IMAP server won't let us go in", *state );
            break;
    }
}

void MailboxModel::authenticate()
{
    // FIXME: real authentication here; *die* if server doesn't let us in
    qDebug() << "MailboxModel::authenticate()";
}

void MailboxModel::select()
{
    _existsDone = _recentDone = _unSeenDone = _uidNextDone = _uidValidityDone =
        _flagsDone = _permanentFlagsDone = false;
    _selectTag = _readWrite ? _parser->select( _mailbox ) : _parser->examine( _mailbox );
    _waitingForSelect = true;
    updateState( IMAP_STATE_SELECTING );
}

void MailboxModel::alert( const Imap::Responses::AbstractResponse* const resp, const QString& message )
{
    // FIXME
    qDebug() << "ALERT: " << message;
}

void MailboxModel::unknownResponseCode( const Imap::Responses::AbstractResponse* const resp )
{
    // FIXME
}

QTextStream& operator<<( QTextStream& s, const MailboxModel::ImapState state )
{
    switch (state) {
        case MailboxModel::IMAP_STATE_CONN_ESTABLISHED:
            s << "IMAP_STATE_CONN_ESTABLISHED";
            break;
        case MailboxModel::IMAP_STATE_NOT_AUTH:
            s << "IMAP_STATE_NOT_AUTH";
            break;
        case MailboxModel::IMAP_STATE_AUTH:
            s << "IMAP_STATE_AUTH";
            break;
        case MailboxModel::IMAP_STATE_SELECTING:
            s << "IMAP_STATE_SELECTING";
            break;
        case MailboxModel::IMAP_STATE_SELECTED:
            s << "IMAP_STATE_SELECTED";
            break;
        case MailboxModel::IMAP_STATE_LOGOUT:
            s << "IMAP_STATE_LOGOUT";
            break;
    }
    return s;
}

}
}

#include "MailboxModel.moc"
