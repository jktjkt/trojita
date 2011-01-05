/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

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
#include "PrettyMsgListModel.h"
#include "MailboxModel.h"
#include "ItemRoles.h"

#include <QFont>
#include <QIcon>

namespace Imap {

namespace Mailbox {

PrettyMsgListModel::PrettyMsgListModel( QObject *parent ): QSortFilterProxyModel( parent )
{
    setDynamicSortFilter( true );
}

QVariant PrettyMsgListModel::data( const QModelIndex& index, int role ) const
{
    if ( ! index.isValid() || index.model() != this )
        return QVariant();

    if ( index.column() < 0 || index.column() >= columnCount( index.parent() ) )
        return QVariant();

    if ( index.row() < 0 || index.row() >= rowCount( index.parent() ) )
        return QVariant();

    return QSortFilterProxyModel::data( index, role );
}

}

}
