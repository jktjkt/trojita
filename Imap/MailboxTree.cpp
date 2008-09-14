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

#include "MailboxTree.h"
#include "Model.h"

namespace Imap {
namespace Mailbox {

TreeItem::TreeItem( TreeItem* parent ): _parent(parent), _fetched(false), _loading(false)
{
}

TreeItem::~TreeItem()
{
    qDeleteAll( _children );
}

unsigned int TreeItem::childrenCount( const Model* const model )
{
    fetch( model );
    return _children.size();
}

TreeItem* TreeItem::child( int offset, const Model* const model )
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

void TreeItem::setChildren( const QList<TreeItem*> items )
{
    qDeleteAll( _children );
    _children = items;
    _fetched = true;
    _loading = false;
}



TreeItemMailbox::TreeItemMailbox( TreeItem* parent ):
    TreeItem(parent)
{
}

TreeItemMailbox::TreeItemMailbox( TreeItem* parent, Responses::List response ):
    TreeItem(parent), _mailbox(response.mailbox),
    _separator(response.separator)
{
    for ( QStringList::const_iterator it = response.flags.begin(); it != response.flags.end(); ++it )
        _flags.append( it->toUpper() );
}

void TreeItemMailbox::fetch( const Model* const model )
{
    if ( _fetched )
        return;

    if ( ! _loading ) {
        model->_askForChildrenOfMailbox( this );
        _loading = true;
    }
}

unsigned int TreeItemMailbox::rowCount( const Model* const model )
{
    fetch( model );
    return _children.size();
}

QVariant TreeItemMailbox::data( const Model* const model, int role )
{
    if ( role != Qt::DisplayRole )
        return QVariant();

    if ( ! _parent )
        return QVariant();

    if ( _loading )
        return "[loading]";

    return separator().isEmpty() ? mailbox() : mailbox().split( separator(), QString::SkipEmptyParts ).last() ;
}
    
bool TreeItemMailbox::hasChildren( const Model* const model )
{
    return true; // we have that "messages" thing built it

    // FIXME: case sensitivity
    if ( _fetched )
        return ! _children.isEmpty();
    else if ( _flags.contains( "\\NOINFERIORS" ) || _flags.contains( "\\HASNOCHILDRen" ) )
        return false;
    else if ( _flags.contains( "\\HASCHILDREN" ) )
        return true;
    else {
        fetch( model );
        return ! _children.isEmpty();
    }
}

void TreeItemMailbox::setChildren( const QList<TreeItem*> items )
{
    TreeItem::setChildren( items );
    _children.prepend( new TreeItemMsgList( this ) );

    // FIXME: anything else required for \Noselect?
    if ( _flags.contains( "\\NOSELECT" ) )
        static_cast<TreeItemMsgList*>( _children[0] )->_fetched = true;
}


TreeItemMsgList::TreeItemMsgList( TreeItem* parent ): TreeItem(parent)
{
    if ( ! parent->parent() )
        _fetched = true;
}

void TreeItemMsgList::fetch( const Model* const model )
{
    if ( _fetched )
        return;

    if ( ! _loading ) {
        model->_askForMessagesInMailbox( this );
        _loading = true;
    }
}

unsigned int TreeItemMsgList::rowCount( const Model* const model )
{
    return childrenCount( model );
}

QVariant TreeItemMsgList::data( const Model* const model, int role )
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

bool TreeItemMsgList::hasChildren( const Model* const model )
{
    return true; // we can easily wait here
    // return childrenCount( model ) > 0;
}



TreeItemMessage::TreeItemMessage( TreeItem* parent ): TreeItem(parent)
{}

void TreeItemMessage::fetch( const Model* const model )
{
    // FIXME
}

unsigned int TreeItemMessage::rowCount( const Model* const model )
{
    // FIXME
    return 0;
}

QVariant TreeItemMessage::data( const Model* const model, int role )
{
    if ( role != Qt::DisplayRole )
        return QVariant();

    if ( ! _parent )
        return QVariant();

    if ( _loading )
        return "[loading...]";

    return "[message]";
}

}
}
