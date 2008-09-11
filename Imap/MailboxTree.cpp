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

TreeItem::TreeItem( TreeItem* parent ): _parent(parent), _fetched(false)
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

TreeItem* TreeItem::child( unsigned int offset, const Model* const model )
{
    fetch( model );
    return _children[ offset ];
}

TreeItemMailbox::TreeItemMailbox( TreeItem* parent, const QString& mailbox ): 
    TreeItem(parent), _mailbox(mailbox)
{
}


void TreeItemMailbox::fetch( const Model* const model )
{
    if ( _fetched )
        return;

    model->_askForChildrenOfMailbox( _mailbox );
}

unsigned int TreeItemMailbox::columnCount( const Model* const model )
{
    Q_UNUSED( model );
    return 1;
}

unsigned int TreeItemMailbox::rowCount( const Model* const model )
{
    fetch( model );
    return _children.size();
}

}
}
