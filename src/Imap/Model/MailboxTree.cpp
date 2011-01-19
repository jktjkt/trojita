/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <QTextStream>
#include "MailboxTree.h"
#include "Model.h"
#include "Imap/Encoders.h"
#include "KeepMailboxOpenTask.h"
#include "ItemRoles.h"
#include <QtDebug>

namespace {

void decodeMessagePartTransportEncoding( const QByteArray& rawData, const QByteArray& encoding, QByteArray* outputData )
{
    Q_ASSERT( outputData );
    if ( encoding == "quoted-printable" ) {
        *outputData = Imap::quotedPrintableDecode( rawData );
    } else if ( encoding == "base64" ) {
        *outputData = QByteArray::fromBase64( rawData );
    } else if ( encoding.isEmpty() || encoding == "7bit" || encoding == "8bit" || encoding == "binary" ) {
        *outputData = rawData;
    } else {
        qDebug() << "Warning: unknown encoding" << encoding;
        *outputData = rawData;
    }
}

QVariantList addresListToQVariant( const QList<Imap::Message::MailAddress>& addressList )
{
    QVariantList res;
    foreach( const Imap::Message::MailAddress& address, addressList ) {
        res.append( QVariant( QStringList() << address.name << address.adl << address.mailbox << address.host ) );
    }
    return res;
}

}



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

unsigned int TreeItem::columnCount()
{
    return 1;
}

TreeItem* TreeItem::specialColumnPtr( int row, int column ) const
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    return 0;
}


TreeItemMailbox::TreeItemMailbox( TreeItem* parent ): TreeItem(parent), maintainingTask(0)
{
    _children.prepend( new TreeItemMsgList( this ) );
}

TreeItemMailbox::TreeItemMailbox( TreeItem* parent, Responses::List response ):
    TreeItem(parent), _metadata( response.mailbox, response.separator, QStringList() ), maintainingTask(0)
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

    if ( hasNoChildMaliboxesAlreadyKnown() ) {
        _fetchStatus = DONE;
        return;
    }

    if ( ! loading() ) {
        _fetchStatus = LOADING;
        model->_askForChildrenOfMailbox( this );
    }
}

void TreeItemMailbox::rescanForChildMailboxes( Model* const model )
{
    // FIXME: fix duplicate requests (ie. don't allow more when some are on their way)
    // FIXME: gotta be fixed in the Model, or spontaneous replies from server can break us
    model->cache()->forgetChildMailboxes( mailbox() );
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
    if ( ! _parent )
        return QVariant();

    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( _children[0] );
    Q_ASSERT( list );

    switch ( role ) {
    case Qt::DisplayRole:
    {
        // this one is used only for a dumb view attached to the Model
        QString res = separator().isEmpty() ? mailbox() : mailbox().split( separator(), QString::SkipEmptyParts ).last();
        return loading() ? res + " [loading]" : res;
    }
    case RoleIsFetched:
        return fetched();
    case RoleShortMailboxName:
        return separator().isEmpty() ? mailbox() : mailbox().split( separator(), QString::SkipEmptyParts ).last();
    case RoleMailboxName:
        return mailbox();
    case RoleMailboxSeparator:
        return separator();
    case RoleMailboxHasChildmailboxes:
        return list->hasChildren( 0 );
    case RoleMailboxIsINBOX:
        return mailbox().toUpper() == QLatin1String("INBOX");
    case RoleMailboxIsSelectable:
        return isSelectable();
    case RoleMailboxNumbersFetched:
        return list->numbersFetched();
    case RoleTotalMessageCount:
    {
        if ( ! isSelectable() )
            return QVariant();
        // At first, register that request for count
        int res = list->totalMessageCount( model );
        // ...and now that it's been sent, display a number if it's available
        return list->numbersFetched() ? QVariant(res) : QVariant();
    }
    case RoleUnreadMessageCount:
    {
        if ( ! isSelectable() )
            return QVariant();
        // This one is similar to the case of RoleTotalMessageCount
        int res = list->unreadMessageCount( model );
        return list->numbersFetched() ? QVariant(res): QVariant();
    }
    case RoleMailboxItemsAreLoading:
        return list->loading() || ! list->numbersFetched();
    case RoleMailboxUidValidity:
    {
        list->fetch( model );
        return list->fetched() ? QVariant(syncState.uidValidity()) : QVariant();
    }
    default:
        return QVariant();
    }
}

bool TreeItemMailbox::hasChildren( Model* const model )
{
    Q_UNUSED( model );
    return true; // we have that "messages" thing built in
}

QLatin1String TreeItemMailbox::_noInferiors( "\\NOINFERIORS" );
QLatin1String TreeItemMailbox::_hasNoChildren( "\\HASNOCHILDREN" );
QLatin1String TreeItemMailbox::_hasChildren( "\\HASCHILDREN" );

bool TreeItemMailbox::hasNoChildMaliboxesAlreadyKnown()
{
    if ( _metadata.flags.contains( _noInferiors ) ||
                  ( _metadata.flags.contains( _hasNoChildren ) &&
                    ! _metadata.flags.contains( _hasChildren ) ) )
            return true;
    else
            return false;
}

bool TreeItemMailbox::hasChildMailboxes( Model* const model )
{
    if ( fetched() || isUnavailable( model ) )
        return _children.size() > 1;
    else if ( hasNoChildMaliboxesAlreadyKnown() )
        return false;
    else if ( _metadata.flags.contains( _hasChildren ) &&
              ! _metadata.flags.contains( _hasNoChildren ) )
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
                                           QList<TreeItemPart*> &changedParts,
                                           TreeItemMessage* &changedMessage )
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

    // This should stay, unless we ever abandon the UID SEARCH ALL for UID discovery.
    // I believe it's guaranteed by the check for list->fetched() above.
    Q_ASSERT(message->_uid);

    bool savedBodyStructure = false;
    bool gotEnvelope = false;
    bool gotSize = false;
    bool gotFlags = false;

    for ( Responses::Fetch::dataType::const_iterator it = response.data.begin(); it != response.data.end(); ++ it ) {
        if (  it.key() == "UID" ) {
            // established above
            Q_ASSERT( dynamic_cast<const Responses::RespData<uint>&>( *(it.value()) ).data == message->uid() );
        } else if ( it.key() == "ENVELOPE" ) {
            message->_envelope = dynamic_cast<const Responses::RespData<Message::Envelope>&>( *(it.value()) ).data;
            message->_fetchStatus = DONE;
            gotEnvelope = true;
            changedMessage = message;
        } else if ( it.key() == "BODYSTRUCTURE" ) {
            if ( message->fetched() ) {
                // The message structure is already known, so we are free to ignore it
            } else {
                // We had no idea about the structure of the message
                QList<TreeItem*> newChildren = dynamic_cast<const Message::AbstractMessage&>( *(it.value()) ).createTreeItems( message );
                if ( ! message->_children.isEmpty() ) {
                    QModelIndex messageIdx = model->createIndex( message->row(), 0, message );
                    model->beginRemoveRows( messageIdx, 0, message->_children.size() - 1 );
                    QList<TreeItem*> oldChildren = message->setChildren( newChildren );
                    model->endRemoveRows();
                    qDeleteAll( oldChildren );
                } else {
                    QList<TreeItem*> oldChildren = message->setChildren( newChildren );
                    Q_ASSERT( oldChildren.size() == 0 );
                }
                savedBodyStructure = true;
            }
        } else if ( it.key() == "x-trojita-bodystructure" ) {
            // do nothing
        } else if ( it.key() == "RFC822.SIZE" ) {
            message->_size = dynamic_cast<const Responses::RespData<uint>&>( *(it.value()) ).data;
            gotSize = true;
        } else if ( it.key().startsWith( "BODY[" ) ) {
            if ( it.key()[ it.key().size() - 1 ] != ']' )
                throw UnknownMessageIndex( "Can't parse such BODY[]", response );
            TreeItemPart* part = partIdToPtr( model, message, it.key() );
            if ( ! part )
                throw UnknownMessageIndex( "Got BODY[] fetch that did not resolve to any known part", response );
            const QByteArray& data = dynamic_cast<const Responses::RespData<QByteArray>&>( *(it.value()) ).data;
            decodeMessagePartTransportEncoding( data, part->encoding(), part->dataPtr() );
            part->_fetchStatus = DONE;
            if ( message->uid() )
                model->cache()->setMsgPart( mailbox(), message->uid(), it.key(), part->_data );
            changedParts.append( part );
        } else if ( it.key() == "FLAGS" ) {
            bool wasSeen = message->isMarkedAsRead();
            message->_flags = dynamic_cast<const Responses::RespData<QStringList>&>( *(it.value()) ).data;
            gotFlags = true;
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
            changedMessage = message;
        } else {
            qDebug() << "TreeItemMailbox::handleFetchResponse: unknown FETCH identifier" << it.key();
        }
    }
    if ( message->uid() ) {
        if ( gotEnvelope && gotSize && savedBodyStructure ) {
            Imap::Mailbox::AbstractCache::MessageDataBundle dataForCache;
            dataForCache.envelope = message->_envelope;
            dataForCache.serializedBodyStructure = dynamic_cast<const Responses::RespData<QByteArray>&>( *(response.data[ "x-trojita-bodystructure" ]) ).data;
            dataForCache.size = message->_size;
            dataForCache.uid = message->uid();
            model->cache()->setMessageMetadata( mailbox(), message->uid(), dataForCache );
        }
        if ( gotFlags ) {
            model->cache()->setMsgFlags( mailbox(), message->uid(), message->_flags );
        }
    }
}

void TreeItemMailbox::handleFetchWhileSyncing( Model* const model, Parser* ptr, const Responses::Fetch& response )
{
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( _children[0] );
    Q_ASSERT( list );

    int number = response.number - 1;
    if ( number < 0 || number >= list->_children.size() )
        throw UnknownMessageIndex( "FECTH response during mailbox sync referrs "
                                   "to a message that is out-of-bounds", response );

    for ( Responses::Fetch::dataType::const_iterator it = response.data.begin(); it != response.data.end(); ++ it ) {
        if ( it.key() == "UID" ) {
            qDebug() << "Specifying UID while syncing flags in a mailbox is not too useful";
        } else if ( it.key() == "FLAGS" ) {
            TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( list->_children[ number ] );
            Q_ASSERT( message );
            Q_ASSERT( message->uid() );
            TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( list->parent() );
            Q_ASSERT( mailbox );
            message->_flags = dynamic_cast<const Responses::RespData<QStringList>&>( *(it.value()) ).data;
            model->cache()->setMsgFlags( mailbox->mailbox(), message->uid(), message->_flags );
        } else {
            qDebug() << "Ignoring FETCH field" << it.key() << "while syncing mailbox";
        }
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
    for ( int i = offset; i < list->_children.size(); ++i ) {
        --static_cast<TreeItemMessage*>( list->_children[i] )->_offset;
    }
    model->endRemoveRows();

    --list->_totalMessageCount;
    list->recalcUnreadMessageCount();
    model->saveUidMap( list );
    model->emitMessageCountChanged( this );
}

TreeItemPart* TreeItemMailbox::partIdToPtr( Model* const model, TreeItemMessage* message, const QString& msgId )
{
    QString partIdentification;
    if ( msgId.startsWith(QLatin1String("BODY[")) ) {
        partIdentification = msgId.mid( 5, msgId.size() - 6 );
    } else if ( msgId.startsWith(QLatin1String("BODY.PEEK[")) ) {
        partIdentification = msgId.mid( 10, msgId.size() - 11 );
    } else {
        throw UnknownMessageIndex( QString::fromAscii("Fetch identifier doesn't start with reasonable prefix: %1" ).arg(msgId).toAscii().constData() );
    }

    TreeItem* item = message;
    Q_ASSERT( item );
    Q_ASSERT( item->parent()->fetched() ); // TreeItemMsgList
    QStringList separated = partIdentification.split( '.' );
    for ( QStringList::const_iterator it = separated.begin(); it != separated.end(); ++it ) {
        bool ok;
        uint number = it->toUInt( &ok );
        if ( !ok ) {
            // It isn't a number, so let's check for that special modifiers
            if ( it + 1 != separated.end() ) {
                // If it isn't at the very end, it's an error
                throw UnknownMessageIndex(
                        QString::fromAscii("Part offset contains non-numeric identifiers in the middle: %1" )
                        .arg( msgId ).toAscii().constData() );
            }
            // Recognize the valid modifiers
            if ( *it == QString::fromAscii("HEADER") )
                item = item->specialColumnPtr( 0, OFFSET_HEADER );
            else if ( *it == QString::fromAscii("TEXT") )
                item = item->specialColumnPtr( 0, OFFSET_TEXT );
            else if ( *it == QString::fromAscii("MIME") )
                item = item->specialColumnPtr( 0, OFFSET_MIME );
            else
                throw UnknownMessageIndex( QString::fromAscii(
                            "Can't translate received offset of the message part to a number: %1" )
                            .arg( msgId ).toAscii().constData() );
            break;
        }

        // Normal path: descending down and finding the correct part
        TreeItemPart* part = dynamic_cast<TreeItemPart*>( item->child( 0, model ) );
        if ( part && part->isTopLevelMultiPart() )
            item = part;
        item = item->child( number - 1, model );
        if ( ! item ) {
            throw UnknownMessageIndex( QString::fromAscii(
                    "Offset of the message part not found: message %1 (UID %2), current number %3, full identification %4" )
                                       .arg( QString::number(message->row()), QString::number( message->uid() ),
                                             QString::number(number), msgId ).toAscii().constData() );
        }
    }
    TreeItemPart* part = dynamic_cast<TreeItemPart*>( item );
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
    if ( role == RoleIsFetched )
        return fetched();

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
    // The goal here is to allow periodic updates of the numbers, that's why we don't check just the numbersFetched().
    // That said, it would require other pieces to support refresh of these numbers.
    if ( ! fetched() )
        fetchNumbers( model );
    return _totalMessageCount;
}

int TreeItemMsgList::unreadMessageCount( Model* const model )
{
    // See totalMessageCount()
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
        TreeItem(parent), _size(0), _uid(0), _flagsHandled(false), _offset(-1)
{
    _partHeader = new TreeItemModifiedPart( this, OFFSET_HEADER );
    _partText = new TreeItemModifiedPart( this, OFFSET_TEXT );
}

TreeItemMessage::~TreeItemMessage()
{
    delete _partHeader;
    delete _partText;
}

void TreeItemMessage::fetch( Model* const model )
{
    if ( fetched() || loading() || isUnavailable( model ) )
        return;

    model->_askForMsgMetadata( this );
}

unsigned int TreeItemMessage::rowCount( Model* const model )
{
    fetch( model );
    return _children.size();
}

unsigned int TreeItemMessage::columnCount()
{
    return 3;
}

TreeItem* TreeItemMessage::specialColumnPtr( int row, int column ) const
{
    // This is a nasty one -- we have an irregular shape...

    // No extra columns on other rows
    if ( row != 0 )
        return 0;

    switch ( column ) {
    case OFFSET_TEXT:
        return _partText;
    case OFFSET_HEADER:
        return _partHeader;
    default:
        return 0;
    }
}

int TreeItemMessage::row() const
{
    Q_ASSERT( _offset != -1 );
    return _offset;
}

QVariant TreeItemMessage::data( Model* const model, int role )
{
    if ( ! _parent )
        return QVariant();

    // This one is special, UID doesn't depend on fetch() and should not trigger it, either
    if ( role == RoleMessageUid )
        return _uid ? QVariant(_uid) : QVariant();

    // The same for RoleIsFetched
    if ( role == RoleIsFetched )
        return fetched();

    // FLAGS shouldn't trigger message fetching, either
    if ( role == RoleMessageFlags )
        return _flags;

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
            } else {
                return QVariant();
            }
        default:
            // fall through
            ;
    }

    if ( ! fetched() )
        return QVariant();

    switch ( role ) {
        case RoleMessageIsMarkedDeleted:
            return isMarkedAsDeleted();
        case RoleMessageIsMarkedRead:
            return isMarkedAsRead();
        case RoleMessageIsMarkedForwarded:
            return isMarkedAsForwarded();
        case RoleMessageIsMarkedReplied:
            return isMarkedAsReplied();
        case RoleMessageIsMarkedRecent:
            return isMarkedAsRecent();
        case RoleMessageDate:
            return envelope( model ).date;
        case RoleMessageFrom:
            return addresListToQVariant( envelope( model ).from );
        case RoleMessageTo:
            return addresListToQVariant( envelope( model ).to );
        case RoleMessageCc:
            return addresListToQVariant( envelope( model ).cc );
        case RoleMessageBcc:
            return addresListToQVariant( envelope( model ).bcc );
        case RoleMessageSender:
            return addresListToQVariant( envelope( model ).sender );
        case RoleMessageReplyTo:
            return addresListToQVariant( envelope( model ).replyTo );
        case RoleMessageInReplyTo:
            return envelope( model ).inReplyTo;
        case RoleMessageMessageId:
            return envelope( model ).messageId;
        case RoleMessageSubject:
            return envelope( model ).subject;
        case RoleMessageSize:
            return _size;
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
    _partHeader = new TreeItemModifiedPart( this, OFFSET_HEADER );
    _partMime = new TreeItemModifiedPart( this, OFFSET_MIME );
    _partText = new TreeItemModifiedPart( this, OFFSET_TEXT );
}

TreeItemPart::TreeItemPart(TreeItem *parent):
        TreeItem(parent), _mimeType(QString::fromAscii("text/plain")), _partHeader(0), _partText(0), _partMime(0)
{
}

TreeItemPart::~TreeItemPart()
{
    delete _partHeader;
    delete _partMime;
    delete _partText;
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

    // these data are available immediately
    switch ( role ) {
    case RoleIsFetched:
        return fetched();
    case RolePartMimeType:
        return _mimeType;
    case RolePartCharset:
        return _charset;
    case RolePartEncoding:
        return _encoding;
    case RolePartBodyFldId:
        return _bodyFldId;
    case RolePartBodyDisposition:
        return _bodyDisposition;
    case RolePartFileName:
        return _fileName;
    case RolePartOctets:
        return _octets;
    }


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
        case RolePartData:
            return _data;
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

QString TreeItemPart::partIdForFetch() const
{
    if ( mimeType() == QLatin1String("message/rfc822") )
        return QString::fromAscii("BODY.PEEK[%1.HEADER]").arg( partId() );
    else
        return QString::fromAscii("BODY.PEEK[%1]").arg( partId() );
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

unsigned int TreeItemPart::columnCount()
{
    return 4; // This one includes the OFFSET_MIME
}

TreeItem* TreeItemPart::specialColumnPtr( int row, int column ) const
{
    // No extra columns on other rows
    if ( row != 0 )
        return 0;

    switch ( column ) {
    case OFFSET_HEADER:
        return _partHeader;
    case OFFSET_TEXT:
        return _partText;
    case OFFSET_MIME:
        return _partMime;
    default:
        return 0;
    }
}

void TreeItemPart::silentlyReleaseMemoryRecursive()
{
    Q_FOREACH( TreeItem *item, _children ) {
        TreeItemPart *part = dynamic_cast<TreeItemPart*>( item );
        Q_ASSERT(part);
        part->silentlyReleaseMemoryRecursive();
    }
    _data.clear();
    _fetchStatus = NONE;
}



TreeItemModifiedPart::TreeItemModifiedPart(TreeItem *parent, const PartModifier kind):
        TreeItemPart( parent ), _modifier(kind)
{
}

int TreeItemModifiedPart::row() const
{
    // we're always at the very top
    return 0;
}

TreeItem* TreeItemModifiedPart::specialColumnPtr(int row, int column) const
{
    Q_UNUSED( row );
    Q_UNUSED( column );
    // no special children below the current special one
    return 0;
}

bool TreeItemModifiedPart::isTopLevelMultiPart() const
{
    // we're special enough not to ever act like a "top-level multipart"
    return false;
}

unsigned int TreeItemModifiedPart::columnCount()
{
    // no child items, either
    return 0;
}

QString TreeItemModifiedPart::partId() const
{
    QString parentId;

    if ( TreeItemPart* part = dynamic_cast<TreeItemPart*>( parent() ) )
        parentId = part->partId() + QChar::fromAscii('.');

    return parentId + modifierToString();
}

TreeItem::PartModifier TreeItemModifiedPart::kind() const
{
    return _modifier;
}

QString TreeItemModifiedPart::modifierToString() const
{
    switch ( _modifier ) {
    case OFFSET_HEADER:
        return QString::fromAscii("HEADER");
    case OFFSET_TEXT:
        return QString::fromAscii("TEXT");
    case OFFSET_MIME:
        return QString::fromAscii("MIME");
    default:
        Q_ASSERT(false);
        return QString();
    }
}

QString TreeItemModifiedPart::pathToPart() const
{
    TreeItemPart* parentPart = dynamic_cast<TreeItemPart*>( parent() );
    TreeItemMessage* parentMessage = dynamic_cast<TreeItemMessage*>( parent() );
    Q_ASSERT( parentPart || parentMessage );
    if ( parentPart ) {
        return QString::fromAscii( "%1/%2" ).arg( parentPart->pathToPart(), modifierToString() );
    } else {
        Q_ASSERT( parentMessage );
        return QString::fromAscii( "/%1" ).arg( modifierToString() );
    }
}

}
}
