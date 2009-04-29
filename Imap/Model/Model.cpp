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
#include "UnauthenticatedHandler.h"
#include "AuthenticatedHandler.h"
#include "SelectedHandler.h"
#include <QAuthenticator>
#include <QDebug>
#include <QTimer>

namespace Imap {
namespace Mailbox {

namespace {

bool MailboxNamesEqual( const TreeItem* const a, const TreeItem* const b )
{
    const TreeItemMailbox* const mailboxA = dynamic_cast<const TreeItemMailbox* const>(a);
    const TreeItemMailbox* const mailboxB = dynamic_cast<const TreeItemMailbox* const>(b);

    return mailboxA && mailboxB && mailboxA->mailbox() == mailboxB->mailbox();
}

bool MailboxNameComparator( const TreeItem* const a, const TreeItem* const b )
{
    const TreeItemMailbox* const mailboxA = dynamic_cast<const TreeItemMailbox* const>(a);
    const TreeItemMailbox* const mailboxB = dynamic_cast<const TreeItemMailbox* const>(b);

    if ( mailboxA->mailbox() == QLatin1String( "INBOX" ) )
        return true;
    if ( mailboxB->mailbox() == QLatin1String( "INBOX" ) )
        return false;
    return mailboxA->mailbox().compare( mailboxB->mailbox(), Qt::CaseInsensitive ) < 1;
}

}

ModelStateHandler::ModelStateHandler( Model* _m ): m(_m)
{
}


Model::Model( QObject* parent, CachePtr cache, SocketFactoryPtr socketFactory ):
    // parent
    QAbstractItemModel( parent ),
    // our tools
    _cache(cache), _socketFactory(socketFactory),
    _maxParsers(1), _mailboxes(0), _netPolicy( NETWORK_ONLINE )
{
    _startTls = _socketFactory->startTlsRequired();

    unauthHandler = new UnauthenticatedHandler( this );
    authenticatedHandler = new AuthenticatedHandler( this );
    selectedHandler = new SelectedHandler( this );

    ParserPtr parser( new Imap::Parser( this, _socketFactory ) );
    _parsers[ parser.get() ] = ParserState( parser, 0, ReadOnly, CONN_STATE_ESTABLISHED, unauthHandler );
    connect( parser.get(), SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
    connect( parser.get(), SIGNAL( disconnected( const QString ) ), this, SLOT( slotParserDisconnected( const QString ) ) );
    if ( _startTls ) {
        CommandHandle cmd = parser->startTls();
        _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::STARTTLS, 0 );
    }
    _mailboxes = new TreeItemMailbox( 0 );
    QTimer::singleShot( 0, this, SLOT( setNetworkOnline() ) );
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
        resp->plug( it.value().parser, this );
    }
}

void Model::handleState( Imap::ParserPtr ptr, const Imap::Responses::State* const resp )
{
    // OK/NO/BAD/PREAUTH/BYE
    using namespace Imap::Responses;

    const QString& tag = resp->tag;

    if ( ! tag.isEmpty() ) {
        QMap<CommandHandle, Task>::iterator command = _parsers[ ptr.get() ].commandMap.find( tag );
        if ( command == _parsers[ ptr.get() ].commandMap.end() ) {
            qDebug() << "This command is not valid anymore" << tag;
            return;
        }

        // FIXME: distinguish among OK/NO/BAD here
        switch ( command->kind ) {
            case Task::STARTTLS:
                _parsers[ ptr.get() ].capabilitiesFresh = false;
                if ( resp->kind == Responses::OK ) {
                    QAuthenticator auth;
                    emit authRequested( &auth );
                    if ( auth.isNull() ) {
                        emit connectionError( tr("Can't login without user/password data") );
                    } else {
                        CommandHandle cmd = ptr->login( auth.user(), auth.password() );
                        _parsers[ ptr.get() ].commandMap[ cmd ] = Task( Task::LOGIN, 0 );
                    }
                } else {
                    emit connectionError( tr("Can't establish a secure connection to the server (STARTTLS failed). Refusing to proceed.") );
                }
                break;
            case Task::LOGIN:
                if ( resp->kind == Responses::OK ) {
                    _parsers[ ptr.get() ].connState = CONN_STATE_AUTH;
                    _parsers[ ptr.get() ].responseHandler = authenticatedHandler;
                    completelyReset();
                } else {
                    // FIXME: handle this in a sane way
                    emit connectionError( tr("Login Failed") );
                }
                break;
            case Task::NONE:
                throw CantHappen( "Internal Error: command that is supposed to do nothing?", *resp );
                break;
            case Task::LIST:
                if ( resp->kind == Responses::OK ) {
                    _finalizeList( ptr, command );
                } else {
                    // FIXME
                }
                break;
            case Task::STATUS:
                _finalizeStatus( ptr, command );
                break;
            case Task::SELECT:
                if ( resp->kind == Responses::OK ) {
                    _finalizeSelect( ptr, command );
                } else {
                    if ( _parsers[ ptr.get() ].connState == CONN_STATE_SELECTED )
                        _parsers[ ptr.get() ].connState = CONN_STATE_AUTH;
                    // FIXME: error handling
                }
                break;
            case Task::FETCH:
                _finalizeFetch( ptr, command );
                break;
        }

        _parsers[ ptr.get() ].commandMap.erase( command );

    } else {
        // untagged response
        if ( _parsers[ ptr.get() ].responseHandler )
            _parsers[ ptr.get() ].responseHandler->handleState( ptr, resp );
    }
}

void Model::_finalizeList( ParserPtr parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( command->what );
    QList<TreeItem*> mailboxes;

    QList<Responses::List>& listResponses = _parsers[ parser.get() ].listResponses;
    for ( QList<Responses::List>::const_iterator it = listResponses.begin();
            it != listResponses.end(); ++it ) {
        if ( it->mailbox != mailboxPtr->mailbox() + mailboxPtr->separator() )
            mailboxes << new TreeItemMailbox( command->what, *it );
    }
    listResponses.clear();
    qSort( mailboxes.begin(), mailboxes.end(), MailboxNameComparator );

    // Remove duplicates; would be great if this could be done in a STLish way,
    // but unfortunately std::unique won't help here (the "duped" part of the
    // sequence contains undefined items)
    if ( mailboxes.size() > 1 ) {
        QList<TreeItem*>::iterator it = mailboxes.begin();
        ++it;
        while ( it != mailboxes.end() ) {
            if ( MailboxNamesEqual( it[-1], *it ) ) {
                delete *it;
                it = mailboxes.erase( it );
            } else {
                ++it;
            }
        }
    }

    QList<MailboxMetadata> metadataToCache;
    for ( QList<TreeItem*>::const_iterator it = mailboxes.begin(); it != mailboxes.end(); ++it ) {
        metadataToCache.append( dynamic_cast<TreeItemMailbox*>( *it )->mailboxMetadata() );
    }
    _cache->setChildMailboxes( mailboxPtr->mailbox(), metadataToCache );

    replaceChildMailboxes( parser, mailboxPtr, mailboxes );
}

void Model::replaceChildMailboxes( ParserPtr parser, TreeItemMailbox* mailboxPtr, const QList<TreeItem*> mailboxes )
{
    /* It would be nice to avoid calling layoutAboutToBeChanged(), but
       unfortunately it seems that it is neccessary for QTreeView to work
       correctly (at least in Qt 4.5).

       Additionally, the fine-frained signals aren't popagated via
       QSortFilterProxyModel, on which we rely for detailed models like
       MailboxModel.

       Some observations are at http://lists.trolltech.com/qt-interest/2006-01/thread00099-0.html

       Trolltech's QFileSystemModel works in the same way:
            rowsAboutToBeInserted( QModelIndex(0,0,0x9bbf290,QFileSystemModel(0x9bac530) )  0 16 )
            rowsInserted( QModelIndex(0,0,0x9bbf290,QFileSystemModel(0x9bac530) )  0 16 )
            layoutAboutToBeChanged()
            layoutChanged()

       This applies to other handlers in this file which update model layout as
       well.
    */

    QModelIndex parent = mailboxPtr == _mailboxes ? QModelIndex() : QAbstractItemModel::createIndex( mailboxPtr->row(), 0, mailboxPtr );

    if ( mailboxPtr->_children.size() != 1 ) {
        // There's something besides the TreeItemMsgList and we're going to
        // overwrite them, so we have to delete them right now
        int count = mailboxPtr->rowCount( this );
        beginRemoveRows( parent, 1, count - 1 );
        QList<TreeItem*> oldItems = mailboxPtr->setChildren( QList<TreeItem*>() );
        endRemoveRows();

        // FIXME: this should be less drastical (ie cancel only what is reqlly really required to be cancelled
        for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
            it->commandMap.clear();
        }
        qDeleteAll( oldItems );
    }

    if ( ! mailboxes.isEmpty() ) {
        emit layoutAboutToBeChanged();
        beginInsertRows( parent, 1, mailboxes.size() );
        QList<TreeItem*> dummy = mailboxPtr->setChildren( mailboxes );
        endInsertRows();
        emit layoutChanged();
        Q_ASSERT( dummy.isEmpty() );
    } else {
        QList<TreeItem*> dummy = mailboxPtr->setChildren( mailboxes );
        Q_ASSERT( dummy.isEmpty() );
    }
}

void Model::_finalizeStatus( ParserPtr parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    emit layoutAboutToBeChanged();
    QList<TreeItem*> messages;
    TreeItemMsgList* listPtr = dynamic_cast<TreeItemMsgList*>( command->what );

    uint sMessages = 0, sRecent = 0, sUidNext = 0, sUidValidity = 0, sUnSeen = 0;
    QList<Responses::Status>& statusResponses = _parsers[ parser.get() ].statusResponses;
    for ( QList<Responses::Status>::const_iterator it = statusResponses.begin();
            it != statusResponses.end(); ++it ) {

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
    statusResponses.clear();

    // FIXME: do something with more of these data...
    for ( uint i = 0; i < sMessages; ++i )
        messages.append( new TreeItemMessage( listPtr ) );

    // FIXME: emit signals to prevent nasty segfaults
    qDeleteAll( command->what->setChildren( messages ) );
    emit layoutChanged();
}

void Model::_finalizeSelect( ParserPtr parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    _parsers[ parser.get() ].handler = dynamic_cast<TreeItemMailbox*>( command->what );
    _parsers[ parser.get() ].responseHandler = selectedHandler;
}

void Model::_finalizeFetch( ParserPtr parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    TreeItemPart* part = dynamic_cast<TreeItemPart*>( command.value().what );
    if ( part && part->loading() ) {
        qDebug() << "Imap::Model::_finalizeFetch(): didn't receive anything about message" <<
            part->message()->row() << "part" << part->partId() << "in mailbox" <<
            _parsers[ parser.get() ].mailbox->mailbox();
        part->_fetchStatus = TreeItem::DONE;
    }
}

void Model::handleCapability( Imap::ParserPtr ptr, const Imap::Responses::Capability* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleCapability( ptr, resp );
}

void Model::handleNumberResponse( Imap::ParserPtr ptr, const Imap::Responses::NumberResponse* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleNumberResponse( ptr, resp );
}

void Model::handleList( Imap::ParserPtr ptr, const Imap::Responses::List* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleList( ptr, resp );
}

void Model::handleFlags( Imap::ParserPtr ptr, const Imap::Responses::Flags* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleFlags( ptr, resp );
}

void Model::handleSearch( Imap::ParserPtr ptr, const Imap::Responses::Search* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleSearch( ptr, resp );
}

void Model::handleStatus( Imap::ParserPtr ptr, const Imap::Responses::Status* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleStatus( ptr, resp );
}

void Model::handleFetch( Imap::ParserPtr ptr, const Imap::Responses::Fetch* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleFetch( ptr, resp );
}

void Model::handleNamespace( Imap::ParserPtr ptr, const Imap::Responses::Namespace* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleNamespace( ptr, resp );
}





TreeItem* Model::translatePtr( const QModelIndex& index ) const
{
    return index.internalPointer() ? static_cast<TreeItem*>( index.internalPointer() ) : _mailboxes;
}

QVariant Model::data(const QModelIndex& index, int role ) const
{
    return translatePtr( index )->data( const_cast<Model*>( this ), role );
}

QModelIndex Model::index(int row, int column, const QModelIndex& parent ) const
{
    TreeItem* parentItem = translatePtr( parent );

    if ( column != 0 )
        return QModelIndex();

    TreeItem* child = parentItem->child( row, const_cast<Model*>( this ) );

    return child ? QAbstractItemModel::createIndex( row, column, child ) : QModelIndex();
}

QModelIndex Model::parent(const QModelIndex& index ) const
{
    if ( !index.isValid() )
        return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parent();

    if ( ! parentItem || parentItem == _mailboxes )
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
    return node->rowCount( const_cast<Model*>( this ) );
}

int Model::columnCount(const QModelIndex& index ) const
{
    return 1;
}

bool Model::hasChildren( const QModelIndex& parent ) const
{
    TreeItem* node = translatePtr( parent );

    if ( node )
        return node->hasChildren( const_cast<Model*>( this ) );
    else
        return false;
}

void Model::_askForChildrenOfMailbox( TreeItemMailbox* item )
{
    QString mailbox = item->mailbox();

    if ( mailbox.isNull() )
        mailbox = "%";
    else
        mailbox = mailbox + item->separator() + QChar( '%' );

    if ( networkPolicy() != NETWORK_ONLINE && _cache->childMailboxesFresh( item->mailbox() ) ) {
        // We aren't online and the permanent cache contains relevant data
        QList<MailboxMetadata> metadata = _cache->childMailboxes( item->mailbox() );
        QList<TreeItem*> mailboxes;
        for ( QList<MailboxMetadata>::const_iterator it = metadata.begin(); it != metadata.end(); ++it ) {
            mailboxes << TreeItemMailbox::fromMetadata( item, *it );
        }
        ParserPtr parser = _getParser( 0, ReadOnly );
        TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item );
        Q_ASSERT( mailboxPtr );
        item->_fetchStatus = TreeItem::DONE;
        replaceChildMailboxes( parser, item, mailboxes );
    } else if ( networkPolicy() == NETWORK_OFFLINE ) {
        // No cached data, no network -> fail
        item->_fetchStatus = TreeItem::UNAVAILABLE;
    } else {
        // We have to go to the network
        ParserPtr parser = _getParser( 0, ReadOnly );
        CommandHandle cmd = parser->list( "", mailbox );
        _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::LIST, item );
    }
}

void Model::reloadMailboxList()
{
    _mailboxes->rescanForChildMailboxes( this );
}

void Model::_askForMessagesInMailbox( TreeItemMsgList* item )
{
    Q_ASSERT( item->parent() );
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item->parent() );
    Q_ASSERT( mailboxPtr );

    QString mailbox = mailboxPtr->mailbox();

    if ( networkPolicy() == NETWORK_OFFLINE ) {
        item->_fetchStatus = TreeItem::UNAVAILABLE;
    } else {
        ParserPtr parser = _getParser( 0, ReadOnly );
        CommandHandle cmd = parser->status( mailbox, QStringList() << "MESSAGES" /*<< "RECENT" << "UIDNEXT" << "UIDVALIDITY" << "UNSEEN"*/ );
        _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::STATUS, item );
    }
}

void Model::_askForMsgMetadata( TreeItemMessage* item )
{
    Q_ASSERT( item->parent() ); // TreeItemMsgList
    Q_ASSERT( item->parent()->parent() ); // TreeItemMailbox
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item->parent()->parent() );
    Q_ASSERT( mailboxPtr );

    int order = item->row();

    if ( networkPolicy() == NETWORK_OFFLINE ) {
        item->_fetchStatus = TreeItem::UNAVAILABLE;
    } else {
        ParserPtr parser = _getParser( mailboxPtr, ReadOnly );
        CommandHandle cmd = parser->fetch( Sequence( order + 1 ), QStringList() << "ENVELOPE" << "BODYSTRUCTURE" << "RFC822.SIZE" );
        _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::FETCH, item );
    }
}

void Model::_askForMsgPart( TreeItemPart* item )
{
    // FIXME: fetch parts in chunks, not at once
    Q_ASSERT( item->message() ); // TreeItemMessage
    Q_ASSERT( item->message()->parent() ); // TreeItemMsgList
    Q_ASSERT( item->message()->parent()->parent() ); // TreeItemMailbox
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item->message()->parent()->parent() );
    Q_ASSERT( mailboxPtr );

    if ( networkPolicy() == NETWORK_OFFLINE ) {
        item->_fetchStatus = TreeItem::UNAVAILABLE;
    } else {
        ParserPtr parser = _getParser( mailboxPtr, ReadOnly );
        CommandHandle cmd = parser->fetch( Sequence( item->message()->row() + 1 ),
                QStringList() << QString::fromAscii("BODY[%1]").arg( item->partId() ) );
        _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::FETCH, item );
    }
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
        ParserPtr parser( new Parser( const_cast<Model*>( this ), _socketFactory ) );
        _parsers[ parser.get() ] = ParserState( parser, mailbox, mode, CONN_STATE_ESTABLISHED, unauthHandler );
        connect( parser.get(), SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
        connect( parser.get(), SIGNAL( disconnected() ), this, SLOT( slotParserDisconnected() ) );
        CommandHandle cmd;
        if ( _startTls ) {
            cmd = parser->startTls();
            _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::STARTTLS, 0 );
        }
        if ( mode == ReadWrite )
            cmd = parser->select( mailbox->mailbox() );
        else
            cmd = parser->examine( mailbox->mailbox() );
        _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::SELECT, mailbox );
        return parser;
    }
}

void Model::setNetworkPolicy( const NetworkPolicy policy )
{
    switch ( policy ) {
        case NETWORK_OFFLINE:
            emit networkPolicyOffline();
            break;
        case NETWORK_EXPENSIVE:
            emit networkPolicyExpensive();
            break;
        case NETWORK_ONLINE:
            emit networkPolicyOnline();
            break;
    }
    _netPolicy = policy;
}

void Model::slotParserDisconnected( const QString msg )
{
    Parser* which = qobject_cast<Parser*>( sender() );
    if ( ! which )
        return;
    for ( QMap<CommandHandle, Task>::const_iterator it = _parsers[ which ].commandMap.begin();
            it != _parsers[ which ].commandMap.end(); ++it ) {
        // FIXME: fail the command, perform cleanup,...
    }
    // FIXME: tell the user (or not?)
    qDebug() << sender() << "disconnected:" << msg;
}

void Model::completelyReset()
{
    // FIXME: some replies might be already flying on their way to the parser, so we might receive duplicate data...
    delete _mailboxes;
    for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
        it->commandMap.clear();
        it->listResponses.clear();
        it->statusResponses.clear();
    }
    _mailboxes = new TreeItemMailbox( 0 );
    reset();
}

}
}

#include "Model.moc"
