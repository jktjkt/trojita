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
#include <QtDebug>

namespace Imap {
namespace Mailbox {

TreeItem::TreeItem( TreeItem* parent ): _parent(parent), _fetched(false), _loading(false)
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
    _fetched = true;
    _loading = false;
    return res;
}



TreeItemMailbox::TreeItemMailbox( TreeItem* parent ):
    TreeItem(parent)
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
    if ( _fetched )
        return;

    if ( ! _loading ) {
        model->_askForChildrenOfMailbox( this );
        _loading = true;
    }
}

void TreeItemMailbox::rescanForChildMailboxes( Model* const model )
{
    model->_cache->forgetChildMailboxes( mailbox() );
    model->_askForChildrenOfMailbox( this );
    fetch( model );
}

unsigned int TreeItemMailbox::rowCount( Model* const model )
{
    fetch( model );
    return _children.size();
}

QVariant TreeItemMailbox::data( Model* const model, int role )
{
    if ( role != Qt::DisplayRole )
        return QVariant();

    if ( ! _parent )
        return QVariant();

    if ( _loading )
        return "[loading]";

    return separator().isEmpty() ? mailbox() : mailbox().split( separator(), QString::SkipEmptyParts ).last() ;
}
    
bool TreeItemMailbox::hasChildren( Model* const model )
{
    return true; // we have that "messages" thing built it
}

bool TreeItemMailbox::hasChildMailboxes( Model* const model )
{
    // FIXME: case sensitivity
    if ( _fetched )
        return _children.size() > 1;
    else if ( _metadata.flags.contains( "\\NOINFERIORS" ) || _metadata.flags.contains( "\\HASNOCHILDREN" ) )
        return false;
    else if ( _metadata.flags.contains( "\\HASCHILDREN" ) )
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
    if ( _metadata.flags.contains( "\\NOSELECT" ) )
        msgList->_fetched = true;

    return list;
}

void TreeItemMailbox::handleFetchResponse( Model* const model,
                                           const Responses::Fetch& response,
                                           TreeItemPart** changedPart )
{
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( _children[0] );
    Q_ASSERT( list );
    
    if ( ! list->_fetched ) {
        // this is bad -- we got a reply about a mailbox' state before we had
        // consistent information :(
        // FIXME: this needs more work (we might not have to throw the
        // exception, simple return *might* work)
        throw UnexpectedResponseReceived( "Received a FETCH response before we synced the mailbox state "
                "(TreeItemMsgList not up-to-speed yet)", response );
    }

    int number = response.number - 1;
    if ( number < 0 || number >= list->_children.size() )
        throw UnknownMessageIndex( "Got FETCH that is out of bounds", response );

    TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( list->child( number, model ) );
    Q_ASSERT( message ); // FIXME: this should be relaxed for allowing null pointers instead of "unfetched" TreeItemMessage

    for ( Responses::Fetch::dataType::const_iterator it = response.data.begin(); it != response.data.end(); ++ it ) {
        if ( it.key() == "ENVELOPE" ) {
            message->_envelope = dynamic_cast<const Responses::RespData<Message::Envelope>&>( *(it.value()) ).data;
            message->_fetched = true;
            message->_loading = false;
        } else if ( it.key() == "BODYSTRUCTURE" ) {
            if ( message->_fetched ) {
                // The message structure is already known, so we are free to ignore it
            } else {
                // We had no idea about the structure of the message
                QList<TreeItem*> newChildren = dynamic_cast<const Message::AbstractMessage&>( *(it.value()) ).createTreeItems( message );
                // FIXME: it would be nice to use more fine-grained signals here
                QList<TreeItem*> oldChildren = message->setChildren( newChildren );
                Q_ASSERT( oldChildren.size() == 0 );
            }
        } else if ( it.key().startsWith( "BODY[" ) ) {
            if ( it.key()[ it.key().size() - 1 ] != ']' )
                throw UnknownMessageIndex( "Can't parse such BODY[]", response );
            TreeItemPart* part = partIdToPtr( model, response.number, it.key().mid( 5, it.key().size() - 6 ) );
            if ( ! part )
                throw UnknownMessageIndex( "Got BODY[] fetch that is out of bounds", response );
            part->_data = dynamic_cast<const Responses::RespData<QByteArray>&>( *(it.value()) ).data;
            part->_fetched = true;
            part->_loading = false;
            if ( changedPart ) {
                *changedPart = part;
            }
        } else {
            qDebug() << "TreeItemMailbox::handleFetchResponse: unknown FETCH identifier" << it.key();
        }
    }
}

void TreeItemMailbox::finalizeFetch( Model* const model, const Responses::Status& response )
{

}

TreeItemPart* TreeItemMailbox::partIdToPtr( Model* const model, const int msgNumber, const QString& msgId )
{
    TreeItem* item = _children[0]; // TreeItemMsgList
    Q_ASSERT( static_cast<TreeItemMsgList*>( item )->_fetched );
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


TreeItemMsgList::TreeItemMsgList( TreeItem* parent ): TreeItem(parent)
{
    if ( ! parent->parent() )
        _fetched = true;
}

void TreeItemMsgList::fetch( Model* const model )
{
    if ( _fetched )
        return;

    if ( ! _loading ) {
        model->_askForMessagesInMailbox( this );
        _loading = true;
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

    if ( _loading )
        return "[loading messages...]";

    if ( _fetched )
        return hasChildren( model ) ? QString("[%1 messages]").arg( childrenCount( model ) ) : "[no messages]";
    
    return "[messages?]";
}

bool TreeItemMsgList::hasChildren( Model* const model )
{
    return true; // we can easily wait here
    // return childrenCount( model ) > 0;
}



TreeItemMessage::TreeItemMessage( TreeItem* parent ): TreeItem(parent)
{}

void TreeItemMessage::fetch( Model* const model )
{
    if ( _fetched || _loading )
        return;

    model->_askForMsgMetadata( this );
    _loading = true;
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
            if ( _loading )
                return "[loading...]";
            else
                return _envelope.subject;
        case Qt::ToolTipRole:
            if ( ! _loading ) {
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



TreeItemPart::TreeItemPart( TreeItem* parent, const QString& mimeType ): TreeItem(parent), _mimeType(mimeType.toLower())
{
    if ( isTopLevelMultiPart() ) {
        // Note that top-level multipart messages are special, their immediate contents
        // can't be fetched. That's why we have to update the status here.
        _fetched = true;
    }
}

unsigned int TreeItemPart::childrenCount( Model* const model )
{
    return _children.size();
}

TreeItem* TreeItemPart::child( const int offset, Model* const model )
{
    if ( offset >= 0 && offset < _children.size() )
        return _children[ offset ];
    else
        return 0;
}

QList<TreeItem*> TreeItemPart::setChildren( const QList<TreeItem*> items )
{
    bool fetched = _fetched;
    bool loading = _loading;
    QList<TreeItem*> res = TreeItem::setChildren( items );
    _fetched = fetched;
    _loading = loading;
    return res;
}

void TreeItemPart::fetch(  Model* const model )
{
    if ( _fetched || _loading )
        return;

    model->_askForMsgPart( this );
    _loading = true;
}

unsigned int TreeItemPart::rowCount( Model* const model )
{
    // no call to fetch() required
    return _children.size();
}

QVariant TreeItemPart::data( Model* const model, int role )
{
    if ( ! _parent )
        return QVariant();

    fetch( model );

    if ( _loading )
        return isTopLevelMultiPart() ?
            QString("[loading %1...]").arg( _mimeType ) : 
            QString("[loading %1: %2...]").arg( partId() ).arg( _mimeType );

    switch ( role ) {
        case Qt::DisplayRole:
            return isTopLevelMultiPart() ?
                QString("%1").arg( _mimeType ) : 
                QString("%1: %2").arg( partId() ).arg( _mimeType );
        case Qt::ToolTipRole:
            return _data.size() > 10000 ? QString::number(_data.size()) + QString(" bytes of data") : _data;
        default:
            return QVariant();
    }
}

bool TreeItemPart::hasChildren( Model* const model )
{
    // no need to fetch() here
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
