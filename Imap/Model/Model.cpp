/* Copyright (C) 2007 - 2008 Jan Kundrát <jkt@gentoo.org>

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

#include "Model.h"
#include "MailboxTree.h"
#include <QDebug>

namespace Imap {
namespace Mailbox {

Model::Model( QObject* parent, CachePtr cache, AuthenticatorPtr authenticator,
        SocketFactoryPtr socketFactory ):
    // parent
    QAbstractItemModel( parent ),
    // our tools
    _cache(cache), _authenticator(authenticator), _socketFactory(socketFactory),
    _maxParsers(1), _capabilitiesFresh(false), _mailboxes(0)
{
    ParserPtr parser( new Imap::Parser( this, _socketFactory->create() ) );
    _parsers[ parser.get() ] = ParserState( parser, 0, ReadOnly, CONN_STATE_ESTABLISHED );
    connect( parser.get(), SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
    _mailboxes = new TreeItemMailbox( 0 );
}

Model::~Model()
{
    delete _mailboxes;
}

void Model::responseReceived()
{
    QMap<Parser*,ParserState>::iterator it = _parsers.find( qobject_cast<Imap::Parser*>( sender() ));
    Q_ASSERT( it != _parsers.end() );

    while ( it.value().parser->hasResponse() ) {
        std::tr1::shared_ptr<Imap::Responses::AbstractResponse> resp = it.value().parser->getResponse();
        Q_ASSERT( resp );

        /*QString buf;
        QTextStream s(&buf);
        s << "<<< " << *resp << "\r\n";
        s.flush();
        qDebug() << buf.left(100);*/
        resp->plug( it.value().parser, this );
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
                /*const RespData<QString>* const msg = dynamic_cast<const RespData<QString>* const>(
                        resp->respCodeData.get() );
                alert( resp, msg ? msg->data : QString() );*/
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

    if ( ! tag.isEmpty() ) {
        QMap<CommandHandle, Task>::iterator command = _parsers[ ptr.get() ].commandMap.find( tag );
        if ( command == _parsers[ ptr.get() ].commandMap.end() )
            throw UnexpectedResponseReceived( "Unknown tag in tagged response", *resp );

        switch ( command->kind ) {
            case Task::NONE:
                throw CantHappen( "Internal Error: command that is supposed to do nothing?", *resp );
                break;
            case Task::LIST:
                _finalizeList( command );
                break;
            case Task::STATUS:
                _finalizeStatus( command );
                break;
            case Task::SELECT:
                _finalizeSelect( ptr, command );
                break;
            case Task::FETCH:
                _finalizeFetch( ptr, command );
        }

        _parsers[ ptr.get() ].commandMap.erase( command );

    } else {
        // untagged response

        switch ( _parsers[ ptr.get() ].connState ) {
            case CONN_STATE_ESTABLISHED:
                using namespace Imap::Responses;
                switch ( resp->kind ) {
                    case PREAUTH:
                        _parsers[ ptr.get() ].connState = CONN_STATE_AUTH;
                        break;
                    case OK:
                        _parsers[ ptr.get() ].connState = CONN_STATE_NOT_AUTH;
                        break;
                    case BYE:
                        _parsers[ ptr.get() ].connState = CONN_STATE_LOGOUT;
                        break;
                    default:
                        throw Imap::UnexpectedResponseReceived(
                                "Waiting for initial OK/BYE/PREAUTH, but got this instead",
                                *resp );
                }
                break;
            case CONN_STATE_NOT_AUTH:
                throw UnexpectedResponseReceived(
                        "Somehow we managed to get back to the "
                        "IMAP_STATE_NOT_AUTH, which is rather confusing. "
                        "Internal error in trojita?",
                        *resp );
                break;
            case CONN_STATE_AUTH:
            case CONN_STATE_SELECTED:
                if ( _parsers[ ptr.get() ].handler ) {
                    // FIXME: pass it to mailbox
                } else {
                    // FIXME: somehow handle this one...
                }
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
}

void Model::_finalizeList( const QMap<CommandHandle, Task>::const_iterator command )
{
    // FIXME: more fine-grained resizing than layoutChanged()
    emit layoutAboutToBeChanged();
    QList<TreeItem*> mailboxes;
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( command->what );
    for ( QList<Responses::List>::const_iterator it = _listResponses.begin();
            it != _listResponses.end(); ++it ) {
        if ( it->mailbox != mailboxPtr->mailbox() + mailboxPtr->separator() )
            mailboxes << new TreeItemMailbox( command->what, *it );
    }
    _listResponses.clear();
    qSort( mailboxes.begin(), mailboxes.end(), SortMailboxes );
    command->what->setChildren( mailboxes );
    emit layoutChanged();

    qDebug() << "_finalizeList" << mailboxPtr->mailbox();
}

void Model::_finalizeStatus( const QMap<CommandHandle, Task>::const_iterator command )
{
    // FIXME: more fine-grained resizing than layoutChanged()
    emit layoutAboutToBeChanged();
    QList<TreeItem*> messages;
    TreeItemMsgList* listPtr = dynamic_cast<TreeItemMsgList*>( command->what );

    uint sMessages = 0, sRecent = 0, sUidNext = 0, sUidValidity = 0, sUnSeen = 0;
    for ( QList<Responses::Status>::const_iterator it = _statusResponses.begin();
            it != _statusResponses.end(); ++it ) {

        for ( Responses::Status::stateDataType::const_iterator item = it->states.begin();
                item != it->states.end(); ++item ) {
            switch ( item.key() ) {
                case Responses::Status::MESSAGES:
                    sMessages = item.value();
                    break;
                case Responses::Status::RECENT:
                    sRecent = item.value();
                    break;
                case Responses::Status::UIDNEXT:
                    sUidNext = item.value();
                    break;
                case Responses::Status::UIDVALIDITY:
                    sUidValidity = item.value();
                    break;
                case Responses::Status::UNSEEN:
                    sUnSeen = item.value();
                    break;
            }
        }
    }
    _statusResponses.clear();

    // FIXME: do something with more of these data...
    for ( uint i = 0; i < sMessages; ++i )
        messages.append( new TreeItemMessage( listPtr ) );

    command->what->setChildren( messages );
    emit layoutChanged();
    qDebug() << "_finalizeStatus" << dynamic_cast<TreeItemMailbox*>( listPtr->parent() )->mailbox();
}

void Model::_finalizeSelect( ParserPtr parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    _parsers[ parser.get() ].handler = dynamic_cast<TreeItemMailbox*>( command->what );
}

void Model::_finalizeFetch( ParserPtr parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    // FIXME
}

bool SortMailboxes( const TreeItem* const a, const TreeItem* const b )
{
    const TreeItemMailbox* const mailboxA = dynamic_cast<const TreeItemMailbox* const>(a);
    const TreeItemMailbox* const mailboxB = dynamic_cast<const TreeItemMailbox* const>(b);

    if ( mailboxA->mailbox() == QLatin1String( "INBOX" ) ) 
        return true;
    if ( mailboxB->mailbox() == QLatin1String( "INBOX" ) )
        return false;
    return mailboxA->mailbox().compare( mailboxB->mailbox(), Qt::CaseInsensitive ) < 1;
}

void Model::handleCapability( Imap::ParserPtr ptr, const Imap::Responses::Capability* const resp )
{
}

void Model::handleNumberResponse( Imap::ParserPtr ptr, const Imap::Responses::NumberResponse* const resp )
{
}

void Model::handleList( Imap::ParserPtr ptr, const Imap::Responses::List* const resp )
{
    _listResponses << *resp;
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
    // FIXME: we should check state here -- this is not really important now
    // when we don't actually SELECT/EXAMINE any mailbox, but *HAS* to be
    // changed as soon as we do so
    _statusResponses << *resp;
}

void Model::handleFetch( Imap::ParserPtr ptr, const Imap::Responses::Fetch* const resp )
{
    TreeItemMailbox* mailbox = _parsers[ ptr.get() ].handler;
    if ( ! mailbox )
        throw UnexpectedResponseReceived( "Received FETCH reply, but AFAIK we haven't selected any mailbox yet", *resp );

    emit layoutAboutToBeChanged();
    mailbox->handleFetchResponse( this, *resp );
    /*TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( mailbox->child( 0, this ) );
    TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( list->child( resp->number - 1, this ) );
    QModelIndex index = QAbstractItemModel::createIndex( resp->number - 1, 0, message );
    emit dataChanged( index, index ); */
    emit layoutChanged();
}

void Model::handleNamespace( Imap::ParserPtr ptr, const Imap::Responses::Namespace* const resp )
{
    throw UnexpectedResponseReceived( "NAMESPACE reply, wtf?", *resp );
}





TreeItem* Model::translatePtr( const QModelIndex& index ) const
{
    return index.internalPointer() ? static_cast<TreeItem*>( index.internalPointer() ) : _mailboxes;
}

QVariant Model::data(const QModelIndex& index, int role ) const
{
    return translatePtr( index )->data( this, role );
}

QModelIndex Model::index(int row, int column, const QModelIndex& parent ) const
{
    TreeItem* parentItem = translatePtr( parent );

    if ( column != 0 )
        return QModelIndex();

    TreeItem* child = parentItem->child( row, this );

    return child ? QAbstractItemModel::createIndex( row, column, child ) : QModelIndex();
}

QModelIndex Model::parent(const QModelIndex& index ) const
{
    if ( !index.isValid() )
        return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parent();

    if ( parentItem == _mailboxes )
        return QModelIndex();

    return QAbstractItemModel::createIndex( parentItem->row(), 0, parentItem );
}

int Model::rowCount(const QModelIndex& index ) const
{
    TreeItem* node = static_cast<TreeItem*>( index.internalPointer() );
    if ( !node ) {
        node = _mailboxes;
    }
    Q_ASSERT(node);
    return node->rowCount( this );
}

int Model::columnCount(const QModelIndex& index ) const
{
    TreeItem* node = static_cast<TreeItem*>( index.internalPointer() );
    if ( !node ) {
        node = _mailboxes;
    }
    Q_ASSERT(node);
    return node->columnCount( this );
}

bool Model::hasChildren( const QModelIndex& parent ) const
{
    TreeItem* node = translatePtr( parent );

    if ( node )
        return node->hasChildren( this );
    else
        return false;
}

void Model::_askForChildrenOfMailbox( TreeItemMailbox* item ) const
{
    QString mailbox = item->mailbox();

    if ( mailbox.isNull() )
        mailbox = "%";
    else
        mailbox = QString::fromLatin1("%1.%").arg( mailbox ); // FIXME: separator

    qDebug() << "_askForChildrenOfMailbox()" << mailbox;
    ParserPtr parser = _getParser( 0, ReadOnly );
    CommandHandle cmd = parser->list( "", mailbox );
    _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::LIST, item );
}

void Model::_askForMessagesInMailbox( TreeItemMsgList* item ) const
{
    Q_ASSERT( item->parent() );
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item->parent() );
    Q_ASSERT( mailboxPtr );

    QString mailbox = mailboxPtr->mailbox();

    qDebug() << "_askForMessagesInMailbox()" << mailbox;
    ParserPtr parser = _getParser( 0, ReadOnly );
    CommandHandle cmd = parser->status( mailbox, QStringList() << "MESSAGES" /*<< "RECENT" << "UIDNEXT" << "UIDVALIDITY" << "UNSEEN"*/ );
    _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::STATUS, item );
}

void Model::_askForMsgMetadata( TreeItemMessage* item ) const
{
    Q_ASSERT( item->parent() );
    Q_ASSERT( item->parent()->parent() );
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item->parent()->parent() );
    Q_ASSERT( mailboxPtr );

    int order = item->row();

    qDebug() << "_askForMsgEnvelope()" << mailboxPtr->mailbox() << order;
    ParserPtr parser = _getParser( mailboxPtr, ReadOnly );
    CommandHandle cmd = parser->fetch( Sequence( order + 1 ), QStringList() << "ENVELOPE" << "BODYSTRUCTURE" );
    _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::FETCH, item );
}

void Model::_askForMsgPart( TreeItemPart* item ) const
{
    // FIXME: fetch parts in chunks, not at once
    Q_ASSERT( item->message() );
    Q_ASSERT( item->message()->parent() );
    Q_ASSERT( item->message()->parent()->parent() );
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item->message()->parent()->parent() );
    Q_ASSERT( mailboxPtr );

    qDebug() << "fetching part" << item->partId() << "of msg#" << ( item->message()->row() + 1 );
    ParserPtr parser = _getParser( mailboxPtr, ReadOnly );
    CommandHandle cmd = parser->fetch( Sequence( item->message()->row() + 1 ),
            QStringList() << QString::fromAscii("BODY[%1]").arg( item->partId() ) );
    _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::FETCH, item );
    // FIXME: handle results somehow...
}

ParserPtr Model::_getParser( TreeItemMailbox* mailbox, const RWMode mode ) const
{
    if ( ! mailbox ) {
        return _parsers.begin().value().parser;
    } else {
        for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
            if ( it->mailbox == mailbox ) {
                if ( mode == ReadOnly || it->mode == mode ) {
                    return it->parser;
                } else {
                    it->mode = ReadWrite;
                    CommandHandle cmd = it->parser->select( mailbox->mailbox() );
                    _parsers[ it->parser.get() ].commandMap[ cmd ] = Task( Task::SELECT, mailbox );
                    return it->parser;
                }
            }
        }
    }
    if ( _parsers.size() >= _maxParsers ) {
        ParserState& parser = _parsers.begin().value();
        parser.mode = mode;
        parser.mailbox = mailbox;
        CommandHandle cmd;
        if ( mode == ReadWrite )
            cmd = parser.parser->select( mailbox->mailbox() );
        else
            cmd = parser.parser->examine( mailbox->mailbox() );
        _parsers[ parser.parser.get() ].commandMap[ cmd ] = Task( Task::SELECT, mailbox );
        return parser.parser;
    } else {
        // we can create one more
        ParserPtr parser( new Parser( const_cast<Model*>( this ), _socketFactory->create() ) );
        _parsers[ parser.get() ] = ParserState( parser, mailbox, mode, CONN_STATE_ESTABLISHED );
        connect( parser.get(), SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
        CommandHandle cmd;
        if ( mode == ReadWrite )
            cmd = parser->select( mailbox->mailbox() );
        else
            cmd = parser->examine( mailbox->mailbox() );
        _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::SELECT, mailbox );
        return parser;
    }
}

}
}

#include "Model.moc"
