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

#include <QTextStream>
#include "MailboxTree.h"
#include "Model.h"
#include "Imap/Encoders.h"
#include <QtDebug>

namespace Imap {
namespace Mailbox {

TreeItem::TreeItem( TreeItem* parent ): _parent(parent), _fetchStatus(NONE)
{
}

TreeItem::~TreeItem()
{
    qDeleteAll( _children );
}

unsigned int TreeItem::childrenCount( Model* const model )
{
    fetch( model );
    return _children.size();
}

TreeItem* TreeItem::child( int offset, Model* const model )
{
    fetch( model );
    if ( offset >= 0 && offset < _children.size() )
        return _children[ offset ];
    else
        return 0;
}

int TreeItem::row() const
{
    return _parent ? _parent->_children.indexOf( const_cast<TreeItem*>(this) ) : 0;
}

QList<TreeItem*> TreeItem::setChildren( const QList<TreeItem*> items )
{
    QList<TreeItem*> res = _children;
    _children = items;
    _fetchStatus = DONE;
    return res;
}

 bool TreeItem::isUnavailable( Model* const model ) const
 {
     return _fetchStatus == UNAVAILABLE && model->networkPolicy() == Model::NETWORK_OFFLINE;
 }



TreeItemMailbox::TreeItemMailbox( TreeItem* parent ): TreeItem(parent)
{
    _children.prepend( new TreeItemMsgList( this ) );
}

TreeItemMailbox::TreeItemMailbox( TreeItem* parent, Responses::List response ):
    TreeItem(parent), _metadata( response.mailbox, response.separator, QStringList() )
{
    for ( QStringList::const_iterator it = response.flags.begin(); it != response.flags.end(); ++it )
        _metadata.flags.append( it->toUpper() );
    _children.prepend( new TreeItemMsgList( this ) );
}

TreeItemMailbox* TreeItemMailbox::fromMetadata( TreeItem* parent, const MailboxMetadata& metadata )
{
    TreeItemMailbox* res = new TreeItemMailbox( parent );
    res->_metadata = metadata;
    return res;
}

void TreeItemMailbox::fetch( Model* const model )
{
    if ( fetched() || isUnavailable( model ) )
        return;

    if ( ! loading() ) {
        _fetchStatus = LOADING;
        model->_askForChildrenOfMailbox( this );
    }
}

void TreeItemMailbox::rescanForChildMailboxes( Model* const model )
{
    // FIXME: fix duplicate requests (ie. don't allow more when some are on their way)
    // FIXME: gotta be fixed in the Model, or spontaneous replies from server can break us
    model->_cache->forgetChildMailboxes( mailbox() );
    _fetchStatus = NONE;
    fetch( model );
}

unsigned int TreeItemMailbox::rowCount( Model* const model )
{
    fetch( model );
    return _children.size();
}

QVariant TreeItemMailbox::data( Model* const model, int role )
{
    Q_UNUSED( model );
    if ( role != Qt::DisplayRole )
        return QVariant();

    if ( ! _parent )
        return QVariant();

    QString res = separator().isEmpty() ? mailbox() : mailbox().split( separator(), QString::SkipEmptyParts ).last();

    return loading() ? res + " [loading]" : res;
}

bool TreeItemMailbox::hasChildren( Model* const model )
{
    Q_UNUSED( model );
    return true; // we have that "messages" thing built in
}

bool TreeItemMailbox::hasChildMailboxes( Model* const model )
{
    QLatin1String noInferiors( "\\NOINFERIORS" );
    QLatin1String hasNoChildren( "\\HASNOCHILDREN" );
    QLatin1String hasChildren( "\\HASCHILDREN" );
    if ( fetched() || isUnavailable( model ) )
        return _children.size() > 1;
    else if ( _metadata.flags.contains( noInferiors ) ||
              ( _metadata.flags.contains( hasNoChildren ) &&
                ! _metadata.flags.contains( hasChildren ) ) )
        return false;
    else if ( _metadata.flags.contains( hasChildren ) &&
              ! _metadata.flags.contains( hasNoChildren ) )
        return true;
    else {
        fetch( model );
        return _children.size() > 1;
    }
}

TreeItem* TreeItemMailbox::child( const int offset, Model* const model )
{
    // accessing TreeItemMsgList doesn't need fetch()
    if ( offset == 0 )
        return _children[ 0 ];

    return TreeItem::child( offset, model );
}

QList<TreeItem*> TreeItemMailbox::setChildren( const QList<TreeItem*> items )
{
    // This function has to be special because we want to preserve _children[0]

    TreeItemMsgList* msgList = dynamic_cast<TreeItemMsgList*>( _children[0] );
    Q_ASSERT( msgList );
    _children.removeFirst();

    QList<TreeItem*> list = TreeItem::setChildren( items ); // this also adjusts _loading and _fetched

    _children.prepend( msgList );

    // FIXME: anything else required for \Noselect?
    if ( ! isSelectable() )
        msgList->_fetchStatus = DONE;

    return list;
}

void TreeItemMailbox::handleFetchResponse( Model* const model,
                                           const Responses::Fetch& response,
                                           TreeItemPart** changedPart,
                                           TreeItemMessage** changedMessage )
{
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( _children[0] );
    Q_ASSERT( list );
    if ( ! list->fetched() ) {
        QByteArray buf;
        QTextStream ss( &buf );
        ss << response;
        ss.flush();
        qDebug() << "Ignoring FETCH response to a mailbox that isn't synced yet:" << buf;
        return;
    }

    int number = response.number - 1;
    if ( number < 0 || number >= list->_children.size() )
        throw UnknownMessageIndex( "Got FETCH that is out of bounds", response );

    TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( list->child( number, model ) );
    Q_ASSERT( message ); // FIXME: this should be relaxed for allowing null pointers instead of "unfetched" TreeItemMessage

    // store UID earlier, for we need it for saving items
    if ( response.data.find( "UID" ) != response.data.end() )
        message->_uid = dynamic_cast<const Responses::RespData<uint>&>( *(response.data[ "UID" ]) ).data;

    bool savedBodyStructure = false;
    for ( Responses::Fetch::dataType::const_iterator it = response.data.begin(); it != response.data.end(); ++ it ) {
        if ( it.key() == "ENVELOPE" ) {
            message->_envelope = dynamic_cast<const Responses::RespData<Message::Envelope>&>( *(it.value()) ).data;
            message->_fetchStatus = DONE;
            if ( message->uid() )
                model->cache()->setMsgEnvelope( mailbox(), message->uid(), message->_envelope );
        } else if ( it.key() == "BODYSTRUCTURE" ) {
            if ( message->fetched() ) {
                // The message structure is already known, so we are free to ignore it
            } else {
                // We had no idea about the structure of the message
                QList<TreeItem*> newChildren = dynamic_cast<const Message::AbstractMessage&>( *(it.value()) ).createTreeItems( message );
                // FIXME: it would be nice to use more fine-grained signals here
                emit model->layoutAboutToBeChanged();
                QList<TreeItem*> oldChildren = message->setChildren( newChildren );
                Q_ASSERT( oldChildren.size() == 0 );
                emit model->layoutChanged();
                savedBodyStructure = true;
            }
        } else if ( it.key() == "x-trojita-bodystructure" ) {
            if ( savedBodyStructure )
                model->cache()->setMsgStructure( mailbox(), message->uid(),
                    dynamic_cast<const Responses::RespData<QByteArray>&>( *(it.value()) ).data );
        } else if ( it.key() == "RFC822.SIZE" ) {
            message->_size = dynamic_cast<const Responses::RespData<uint>&>( *(it.value()) ).data;
            if ( message->uid() )
                model->cache()->setMsgSize( mailbox(), message->uid(), message->_size );
        } else if ( it.key().startsWith( "BODY[" ) ) {
            if ( it.key()[ it.key().size() - 1 ] != ']' )
                throw UnknownMessageIndex( "Can't parse such BODY[]", response );
            QString partIdentification = it.key().mid( 5, it.key().size() - 6 );
            if ( partIdentification.endsWith( QLatin1String(".HEADER") ) )
                partIdentification = partIdentification.left( partIdentification.size() - QString::fromAscii(".HEADER").size() );
            TreeItemPart* part = partIdToPtr( model, response.number, partIdentification );
            if ( ! part )
                throw UnknownMessageIndex( "Got BODY[] fetch that is out of bounds", response );
            const QByteArray& data = dynamic_cast<const Responses::RespData<QByteArray>&>( *(it.value()) ).data;
            if ( part->encoding() == "quoted-printable" )
                part->_data = Imap::quotedPrintableDecode( data );
            else if ( part->encoding() == "base64" )
                part->_data = QByteArray::fromBase64( data );
            else if ( part->encoding() == "7bit" || part->encoding() == "8bit" || part->encoding() == "binary" )
                part->_data = data;
            else {
                qDebug() << "Warning: unknown encoding" << part->encoding();
                part->_data = data;
            }
            part->_fetchStatus = DONE;
            if ( message->uid() )
                model->cache()->setMsgPart( mailbox(), message->uid(), partIdentification, part->_data );
            if ( changedPart ) {
                *changedPart = part;
            }
        } else if ( it.key() == "UID" ) {
            message->_uid = dynamic_cast<const Responses::RespData<uint>&>( *(it.value()) ).data;
        } else if ( it.key() == "FLAGS" ) {
            bool wasSeen = message->isMarkedAsRead();
            message->_flags = dynamic_cast<const Responses::RespData<QStringList>&>( *(it.value()) ).data;
            if ( message->uid() )
                model->cache()->setMsgFlags( mailbox(), message->uid(), message->_flags );
            if ( list->_numberFetchingStatus == DONE ) {
                bool isSeen = message->isMarkedAsRead();
                if ( message->_flagsHandled ) {
                    if ( wasSeen && ! isSeen )
                        ++list->_unreadMessageCount;
                    else if ( ! wasSeen && isSeen )
                        --list->_unreadMessageCount;
                } else {
                    // it's a new message
                    message->_flagsHandled = true;
                    if ( ! isSeen ) {
                        ++list->_unreadMessageCount;
                    }
                }
            }
            if ( changedMessage )
                *changedMessage = message;
        } else {
            qDebug() << "TreeItemMailbox::handleFetchResponse: unknown FETCH identifier" << it.key();
        }
    }
}

void TreeItemMailbox::handleFetchWhileSyncing( Model* const model, Parser* ptr, const Responses::Fetch& response )
{
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( _children[0] );
    Q_ASSERT( list );

    QList<uint>& uidMap = model->_parsers[ ptr ].uidMap;
    QMap<uint,QStringList>& flagMap = model->_parsers[ ptr ].syncingFlags;

    int number = response.number - 1;
    if ( number < 0 || number >= uidMap.size() )
        throw UnknownMessageIndex( "FECTH response during mailbox sync referrs "
                                   "to a message that is out-of-bounds", response );

    uint uid = 0;
    QStringList flags;
    for ( Responses::Fetch::dataType::const_iterator it = response.data.begin(); it != response.data.end(); ++ it ) {
        if ( it.key() == "UID" ) {
            uid = dynamic_cast<const Responses::RespData<uint>&>( *(it.value()) ).data;
        } else if ( it.key() == "FLAGS" ) {
            flags = dynamic_cast<const Responses::RespData<QStringList>&>( *(it.value()) ).data;
        } else {
            qDebug() << "Ignoring FETCH field" << it.key() << "while syncing mailbox";
        }
    }
    if ( uid ) {
        uidMap[ number ] = uid;
        flagMap[ uid ] = flags;
    } else {
        qWarning() << "Warning: Got useless FETCH reply (didn't specify UID)";
    }
}

void TreeItemMailbox::handleExpunge( Model* const model, const Responses::NumberResponse& resp )
{
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( _children[ 0 ] );
    Q_ASSERT( list );
    if( ! list->fetched() ) {
        throw UnexpectedResponseReceived( "Got EXPUNGE before we fully synced", resp );
    }
    if ( resp.number > static_cast<uint>( list->_children.size() ) || resp.number == 0 ) {
        throw UnknownMessageIndex( "EXPUNGE references message number which is out-of-bounds" );
    }
    uint offset = resp.number - 1;

    model->beginRemoveRows( model->createIndex( 0, 0, list ), offset, offset );
    TreeItemMessage* message = static_cast<TreeItemMessage*>( list->_children.takeAt( offset ) );
    model->cache()->clearMessage( static_cast<TreeItemMailbox*>( list->parent() )->mailbox(), message->uid() );
    delete message;
    model->endRemoveRows();

    --list->_totalMessageCount;
    list->recalcUnreadMessageCount();
    model->saveUidMap( list );
    model->emitMessageCountChanged( this );
}

void TreeItemMailbox::handleExistsSynced( Model* const model, Parser* ptr, const Responses::NumberResponse& resp )
{
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( _children[ 0 ] );
    Q_ASSERT( list );
    if ( resp.number < static_cast<uint>( list->_children.size() ) ) {
        throw UnexpectedResponseReceived( "EXISTS response attempted to decrease number of messages", resp );
    } else if ( resp.number == static_cast<uint>( list->_children.size() ) ) {
        // remains unchanged...
        return;
    }
    uint firstNew = static_cast<uint>( list->_children.size() );
    uint diff = resp.number - firstNew;

    bool willLoad = diff <= Model::StructureFetchLimit && model->networkPolicy() == Model::NETWORK_ONLINE;

    model->beginInsertRows( model->createIndex( 0, 0, list ), list->_children.size(), resp.number - 1 );
    for ( uint i = 0; i < diff; ++i ) {
        TreeItemMessage* message = new TreeItemMessage( list );
        list->_children.append( message );
        if ( willLoad )
            message->_fetchStatus = TreeItem::LOADING;
    }
    model->endInsertRows();
    list->_totalMessageCount = list->_children.size();
    // we don't know the flags yet, so we can't update \seen count
    model->emitMessageCountChanged( this );
    QStringList items = willLoad ? model->_onlineMessageFetch : QStringList() << "UID" << "FLAGS" ;
    CommandHandle cmd = ptr->fetch( Sequence::startingAt( firstNew + 1 ), items );
    model->_parsers[ ptr ].commandMap[ cmd ] = Model::Task( Model::Task::FETCH, this );
}

void TreeItemMailbox::finalizeFetch( Model* const model, const Responses::Status& response )
{

}

TreeItemPart* TreeItemMailbox::partIdToPtr( Model* const model, const int msgNumber, const QString& msgId )
{
    TreeItem* item = _children[0]; // TreeItemMsgList
    Q_ASSERT( static_cast<TreeItemMsgList*>( item )->fetched() );
    item = item->child( msgNumber - 1, model ); // TreeItemMessage
    Q_ASSERT( item );
    QStringList separated = msgId.split( '.' );
    for ( QStringList::const_iterator it = separated.begin(); it != separated.end(); ++it ) {
        bool ok;
        uint number = it->toUInt( &ok );
        if ( !ok )
            throw UnknownMessageIndex( ( QString::fromAscii(
                            "Can't translate received offset of the message part to a number: " )
                        + msgId ).toAscii().constData() );

        TreeItemPart* part = dynamic_cast<TreeItemPart*>( item->child( 0, model ) );
        if ( part && part->isTopLevelMultiPart() )
            item = part;
        item = item->child( number - 1, model );
        if ( ! item ) {
            throw UnknownMessageIndex( ( QString::fromAscii(
                            "Offset of the message part not found: " )
                        + QString::number( number ) + QString::fromAscii(" of ") + msgId ).toAscii().constData() );}
    }
    TreeItemPart* part = dynamic_cast<TreeItemPart*>( item );
    if ( ! part )
        throw UnknownMessageIndex( ( QString::fromAscii(
                        "Offset of the message part doesn't point anywhere: " )
                    + msgId ).toAscii().constData() );
    return part;
}

bool TreeItemMailbox::isSelectable() const
{
    return ! _metadata.flags.contains( QLatin1String("\\NOSELECT") );
}



TreeItemMsgList::TreeItemMsgList( TreeItem* parent ):
        TreeItem(parent), _numberFetchingStatus(NONE), _totalMessageCount(-1),
        _unreadMessageCount(-1)
{
    if ( ! parent->parent() )
        _fetchStatus = DONE;
}

void TreeItemMsgList::fetch( Model* const model )
{
    if ( fetched() || isUnavailable( model ) )
        return;

    if ( ! loading() ) {
        _fetchStatus = LOADING;
        model->_askForMessagesInMailbox( this );
    }
}

void TreeItemMsgList::fetchNumbers( Model* const model )
{
    if ( _numberFetchingStatus == NONE ) {
        _numberFetchingStatus = LOADING;
        model->_askForNumberOfMessages( this );
    }
}

unsigned int TreeItemMsgList::rowCount( Model* const model )
{
    return childrenCount( model );
}

QVariant TreeItemMsgList::data( Model* const model, int role )
{
    if ( role != Qt::DisplayRole )
        return QVariant();

    if ( ! _parent )
        return QVariant();

    if ( loading() )
        return "[loading messages...]";

    if ( isUnavailable( model ) )
        return "[offline]";

    if ( fetched() )
        return hasChildren( model ) ? QString("[%1 messages]").arg( childrenCount( model ) ) : "[no messages]";

    return "[messages?]";
}

bool TreeItemMsgList::hasChildren( Model* const model )
{
    Q_UNUSED( model );
    return true; // we can easily wait here
}

int TreeItemMsgList::totalMessageCount( Model* const model )
{
    // yes, we really check the normal fetch status
    if ( ! fetched() )
        fetchNumbers( model );
    return _totalMessageCount;
}

int TreeItemMsgList::unreadMessageCount( Model* const model )
{
    // yes, we really check the normal fetch status
    if ( ! fetched() )
        fetchNumbers( model );
    return _unreadMessageCount;
}

void TreeItemMsgList::recalcUnreadMessageCount()
{
    _unreadMessageCount = 0;
    for ( int i = 0; i < _children.size(); ++i ) {
        TreeItemMessage* message = static_cast<TreeItemMessage*>( _children[i] );
        message->_flagsHandled = true;
        if ( ! message->isMarkedAsRead() )
            ++_unreadMessageCount;
    }
}

bool TreeItemMsgList::numbersFetched() const
{
    return fetched() || _numberFetchingStatus == DONE;
}



TreeItemMessage::TreeItemMessage( TreeItem* parent ):
        TreeItem(parent), _size(0), _uid(0), _flagsHandled(false)
{}

void TreeItemMessage::fetch( Model* const model )
{
    if ( fetched() || loading() || isUnavailable( model ) )
        return;

    _fetchStatus = LOADING;
    model->_askForMsgMetadata( this );
}

unsigned int TreeItemMessage::rowCount( Model* const model )
{
    fetch( model );
    return _children.size();
}

QVariant TreeItemMessage::data( Model* const model, int role )
{
    if ( ! _parent )
        return QVariant();

    fetch( model );

    switch ( role ) {
        case Qt::DisplayRole:
            if ( loading() )
                return "[loading...]";
            else if ( isUnavailable( model ) )
                return "[offline]";
            else
                return _envelope.subject;
        case Qt::ToolTipRole:
            if ( fetched() ) {
                QString buf;
                QTextStream stream( &buf );
                stream << _envelope;
                return buf;
            } else
                return QVariant();
        default:
            return QVariant();
    }
}

bool TreeItemMessage::isMarkedAsDeleted() const
{
    return _flags.contains( QLatin1String("\\Deleted"), Qt::CaseInsensitive );
}

bool TreeItemMessage::isMarkedAsRead() const
{
    return _flags.contains( QLatin1String("\\Seen"), Qt::CaseInsensitive );
}

bool TreeItemMessage::isMarkedAsReplied() const
{
    return _flags.contains( QLatin1String("\\Answered"), Qt::CaseInsensitive );
}

bool TreeItemMessage::isMarkedAsForwarded() const
{
    return _flags.contains( QLatin1String("$Forwarded"), Qt::CaseInsensitive );
}

bool TreeItemMessage::isMarkedAsRecent() const
{
    return _flags.contains( QLatin1String("\\Recent"), Qt::CaseInsensitive );
}

uint TreeItemMessage::uid() const
{
    return _uid;
}


Message::Envelope TreeItemMessage::envelope( Model* const model )
{
    fetch( model );
    return _envelope;
}

uint TreeItemMessage::size( Model* const model )
{
    fetch( model );
    return _size;
}


TreeItemPart::TreeItemPart( TreeItem* parent, const QString& mimeType ): TreeItem(parent), _mimeType(mimeType.toLower())
{
    if ( isTopLevelMultiPart() ) {
        // Note that top-level multipart messages are special, their immediate contents
        // can't be fetched. That's why we have to update the status here.
        _fetchStatus = DONE;
    }
}

unsigned int TreeItemPart::childrenCount( Model* const model )
{
    Q_UNUSED( model );
    return _children.size();
}

TreeItem* TreeItemPart::child( const int offset, Model* const model )
{
    Q_UNUSED( model );
    if ( offset >= 0 && offset < _children.size() )
        return _children[ offset ];
    else
        return 0;
}

QList<TreeItem*> TreeItemPart::setChildren( const QList<TreeItem*> items )
{
    FetchingState fetchStatus = _fetchStatus;
    QList<TreeItem*> res = TreeItem::setChildren( items );
    _fetchStatus = fetchStatus;
    return res;
}

void TreeItemPart::fetch(  Model* const model )
{
    if ( fetched() || loading() || isUnavailable( model ) )
        return;

    _fetchStatus = LOADING;
    model->_askForMsgPart( this );
}

void TreeItemPart::fetchFromCache( Model* const model )
{
    if ( fetched() || loading() || isUnavailable( model ) )
        return;

    model->_askForMsgPart( this, true );
}

unsigned int TreeItemPart::rowCount( Model* const model )
{
    // no call to fetch() required
    Q_UNUSED( model );
    return _children.size();
}

QVariant TreeItemPart::data( Model* const model, int role )
{
    if ( ! _parent )
        return QVariant();

    fetch( model );

    if ( loading() ) {
        if ( role == Qt::DisplayRole ) {
            return isTopLevelMultiPart() ?
                model->tr("[loading %1...]").arg( _mimeType ) :
                model->tr("[loading %1: %2...]").arg( partId() ).arg( _mimeType );
        } else {
            return QVariant();
        }
    }

    switch ( role ) {
        case Qt::DisplayRole:
            return isTopLevelMultiPart() ?
                QString("%1").arg( _mimeType ) :
                QString("%1: %2").arg( partId() ).arg( _mimeType );
        case Qt::ToolTipRole:
            return _data.size() > 10000 ? model->tr("%1 bytes of data").arg( _data.size() ) : _data;
        default:
            return QVariant();
    }
}

bool TreeItemPart::hasChildren( Model* const model )
{
    // no need to fetch() here
    Q_UNUSED( model );
    return ! _children.isEmpty();
}

/** @short Returns true if we're a multipart, top-level item in the body of a message */
bool TreeItemPart::isTopLevelMultiPart() const
{
    TreeItemMessage* msg = dynamic_cast<TreeItemMessage*>( parent() );
    TreeItemPart* part = dynamic_cast<TreeItemPart*>( parent() );
    return  _mimeType.startsWith( "multipart/" ) && ( msg || ( part && part->_mimeType.startsWith("message/")) );
}

QString TreeItemPart::partId() const
{
    if ( isTopLevelMultiPart() ) {
        TreeItemPart* part = dynamic_cast<TreeItemPart*>( parent() );
        if ( part )
            return part->partId();
        else
            return QString::null;
    } else if ( dynamic_cast<TreeItemMessage*>( parent() ) ) {
        return QString::number( row() + 1 );
    } else {
        QString parentId = dynamic_cast<TreeItemPart*>( parent() )->partId();
        if ( parentId.isNull() )
            return QString::number( row() + 1 );
        else
            return parentId + QChar('.') + QString::number( row() + 1 );
    }
}

QString TreeItemPart::pathToPart() const
{
    TreeItemPart* part = dynamic_cast<TreeItemPart*>( parent() );
    TreeItemMessage* msg = dynamic_cast<TreeItemMessage*>( parent() );
    if ( part )
        return part->pathToPart() + QLatin1Char('/') + QString::number( row() );
    else if ( msg )
        return QLatin1Char('/') + QString::number( row() );
    else {
        Q_ASSERT( false );
        return QString();
    }
}

TreeItemMessage* TreeItemPart::message() const
{
    const TreeItemPart* part = this;
    while ( part ) {
        TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( part->parent() );
        if ( message )
            return message;
        part = dynamic_cast<TreeItemPart*>( part->parent() );
    }
    return 0;
}

QByteArray* TreeItemPart::dataPtr()
{
    return &_data;
}

}
}
