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
#include "MsgListModel.h"
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

    QModelIndex translated = mapToSource( index );

    switch ( role ) {

    case Qt::TextAlignmentRole:
        switch ( index.column() ) {
        case MsgListModel::SIZE:
            return Qt::AlignRight;
        default:
            return QVariant();
        }

    case Qt::DecorationRole:
        switch ( index.column() ) {
        case MsgListModel::SUBJECT:
            {
            if ( ! translated.data( RoleIsFetched ).toBool() )
                return QVariant();

            bool isForwarded = translated.data( RoleMessageIsMarkedForwarded ).toBool();
            bool isReplied = translated.data( RoleMessageIsMarkedReplied ).toBool();

            if ( translated.data( RoleMessageIsMarkedDeleted ).toBool() )
                return QIcon::fromTheme( QLatin1String("mail-deleted"),
                                         QIcon( QLatin1String(":/icons/mail-deleted.png") ) );
            else if ( isForwarded && isReplied )
                return QIcon::fromTheme( QLatin1String("mail-replied-forw"),
                                         QIcon( QLatin1String(":/icons/mail-replied-forw.png") ) );
            else if ( isReplied )
                return QIcon::fromTheme( QLatin1String("mail-replied"),
                                         QIcon( QLatin1String(":/icons/mail-replied.png") ) );
            else if ( isForwarded )
                return QIcon::fromTheme( QLatin1String("mail-forwarded"),
                                         QIcon( QLatin1String(":/icons/mail-forwarded.png") ) );
            else if ( translated.data( RoleMessageIsMarkedRecent ).toBool() )
                return QIcon::fromTheme( QLatin1String("mail-recent"),
                                         QIcon( QLatin1String(":/icons/mail-recent.png") ) );
            else
                return QIcon( QLatin1String(":/icons/transparent.png") );
            }
        case MsgListModel::SEEN:
            if ( ! translated.data( RoleIsFetched ).toBool() )
                return QVariant();
            if ( ! translated.data( RoleMessageIsMarkedRead ).toBool() )
                return QIcon( QLatin1String(":/icons/mail-unread.png") );
            else
                return QIcon( QLatin1String(":/icons/mail-read.png") );
        default:
            return QVariant();
        }

    case Qt::FontRole:
        {
            if ( ! translated.data( RoleIsFetched ).toBool() )
                return QVariant();

            QFont font;
            if ( translated.data( RoleMessageIsMarkedDeleted ).toBool() )
                font.setStrikeOut( true );
            if ( ! translated.data( RoleMessageIsMarkedRead ).toBool() )
                font.setBold( true );
            return font;
        }
    }

    return QSortFilterProxyModel::data( index, role );
}

}

}
