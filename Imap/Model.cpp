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

#include "Imap/Model.h"
#include "Imap/MailboxTree.h"
#include <QDebug>

namespace Imap {
namespace Mailbox {

Model::Model( QObject* parent, CachePtr cache, AuthenticatorPtr authenticator,
        ParserPtr parser ):
    // parent
    QAbstractItemModel( parent ),
    // our tools
    _cache(cache), _authenticator(authenticator), _parser(parser),
    _state( CONN_STATE_ESTABLISHED ), _capabilitiesFresh(false), _mailboxes(0)
{
    connect( _parser.get(), SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
    _mailboxes = new TreeItemMailbox( 0, QString::null );
}

void Model::responseReceived()
{
    while ( _parser->hasResponse() ) {
        std::tr1::shared_ptr<Imap::Responses::AbstractResponse> resp = _parser->getResponse();
        Q_ASSERT( resp );

        QTextStream s(stderr);
        s << "<<< " << *resp << "\r\n";
        s.flush();
        resp->plug( _parser, this );
    }
}

void Model::handleState( Imap::ParserPtr ptr, const Imap::Responses::State* const resp )
{
    // OK/NO/BAD/PREAUTH/BYE
    using namespace Imap::Responses;

    const QString& tag = resp->tag;

    // Check for common stuff like ALERT and CAPABILITIES update
    switch ( resp->respCode ) {
        case ALERT:
            {
                const RespData<QString>* const msg = dynamic_cast<const RespData<QString>* const>(
                        resp->respCodeData.get() );
                //alert( resp, msg ? msg->data : QString() );
                throw 42; // FIXME
            }
            break;
        case CAPABILITIES:
            {
                const RespData<QStringList>* const caps = dynamic_cast<const RespData<QStringList>* const>(
                        resp->respCodeData.get() );
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

    switch ( _state ) {
        case CONN_STATE_ESTABLISHED:
            if ( ! tag.isEmpty() )
                throw UnexpectedResponseReceived( "Received a tagged response when expecting server greeting", *resp );
            else
                handleStateInitial( resp );
            break;
        case CONN_STATE_NOT_AUTH:
            throw UnexpectedResponseReceived(
                    "Somehow we managed to get back to the "
                    "IMAP_STATE_NOT_AUTH, which is rather confusing",
                    *resp );
            break;
        case CONN_STATE_AUTH:
            handleStateAuthenticated( resp );
            break;
        case CONN_STATE_SELECTING:
            handleStateSelecting( resp );
            break;
        case CONN_STATE_SELECTED:
            handleStateSelected( resp );
            break;
        case CONN_STATE_LOGOUT:
            // hey, we're supposed to be logged out, how come that
            // *anything* made it here?
            throw UnexpectedResponseReceived(
                    "WTF, we're logged out, yet I just got this message", 
                    *resp );
            break;
    }
}

void Model::handleStateInitial( const Imap::Responses::State* const state )
{
    using namespace Imap::Responses;

    /*switch ( state->kind ) {
        case PREAUTH:
            _updateState( CONN_STATE_AUTH );
            break;
        case OK:
            _updateState( CONN_STATE_NOT_AUTH );
        case BYE:
            _updateState( CONN_STATE_LOGOUT );
        default:
            throw Imap::UnexpectedResponseReceived(
                    "Waiting for initial OK/BYE/PREAUTH, but got this instead",
                    *state );
    }

    switch ( state->respCode() ) {
        case ALERT:
        case CAPABILITIES:
            // already handled in handleState()
            break;
        default:
            _unknownResponseCode( state );
    }*/
    
    // FIXME
}

void Model::handleStateAuthenticated( const Imap::Responses::State* const state )
{
}

void Model::handleStateSelecting( const Imap::Responses::State* const state )
{
}

void Model::handleStateSelected( const Imap::Responses::State* const state )
{
}


void Model::handleCapability( Imap::ParserPtr ptr, const Imap::Responses::Capability* const resp )
{
}

void Model::handleNumberResponse( Imap::ParserPtr ptr, const Imap::Responses::NumberResponse* const resp )
{
}

void Model::handleList( Imap::ParserPtr ptr, const Imap::Responses::List* const resp )
{
    throw UnexpectedResponseReceived( "LIST reply, wtf?", *resp );
}

void Model::handleFlags( Imap::ParserPtr ptr, const Imap::Responses::Flags* const resp )
{
}

void Model::handleSearch( Imap::ParserPtr ptr, const Imap::Responses::Search* const resp )
{
    throw UnexpectedResponseReceived( "SEARCH reply, wtf?", *resp );
}

void Model::handleStatus( Imap::ParserPtr ptr, const Imap::Responses::Status* const resp )
{
    throw UnexpectedResponseReceived( "STATUS reply, wtf?", *resp );
}

void Model::handleFetch( Imap::ParserPtr ptr, const Imap::Responses::Fetch* const resp )
{
    throw UnexpectedResponseReceived( "FETCH reply, wtf?", *resp );
}



QVariant Model::data(const QModelIndex& index, int role ) const
{
    // FIXME
    //qDebug() << "Model::data" << index << role;
    switch ( role ) {
        case Qt::DisplayRole:
            return QVariant( "333666" );
        default:
            return QVariant();
    }
}

QModelIndex Model::index(int row, int column, const QModelIndex& parent ) const
{
    // FIXME
    qDebug() << "Model::index" << row << column << parent;
    return QAbstractItemModel::createIndex( row, column );
}

QModelIndex Model::parent(const QModelIndex& index ) const
{
    // FIXME
    qDebug() << "Model::parent" << index;
    return QModelIndex();
}

int Model::rowCount(const QModelIndex& index ) const
{
    qDebug() << "Model::rowCount" << index;

    TreeItem* node = static_cast<TreeItem*>( index.internalPointer() );
    if ( !node ) {
        node = _mailboxes;
    }
    Q_ASSERT(node);
    return node->rowCount( this );
}

int Model::columnCount(const QModelIndex& index ) const
{
    qDebug() << "Model::columnCount" << index;

    TreeItem* node = static_cast<TreeItem*>( index.internalPointer() );
    if ( !node ) {
        node = _mailboxes;
    }
    Q_ASSERT(node);
    return node->columnCount( this );
}


void Model::_askForChildrenOfMailbox( const QString& mailbox ) const
{
    qDebug() << "_askForChildrenOfMailbox() called!";
    _parser->list( "", mailbox );
}

}
}

#include "Model.moc"
