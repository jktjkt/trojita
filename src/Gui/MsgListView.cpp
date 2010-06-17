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
#include "MsgListView.h"

#include <QFontMetrics>
#include <QHeaderView>
#include "Imap/Model/MsgListModel.h"

namespace Gui {

MsgListView::MsgListView( QWidget* parent ): QTreeView(parent)
{
    connect( header(), SIGNAL(geometriesChanged()), this, SLOT(slotFixSize()) );
}

int MsgListView::sizeHintForColumn( int column ) const
{
    switch ( column ) {
        case Imap::Mailbox::MsgListModel::SUBJECT:
            return 200;
        case Imap::Mailbox::MsgListModel::SEEN:
            return 16;
        case Imap::Mailbox::MsgListModel::FROM:
        case Imap::Mailbox::MsgListModel::TO:
            return fontMetrics().size( Qt::TextSingleLine, QLatin1String("Blesmrt Trojita") ).width();
        case Imap::Mailbox::MsgListModel::DATE:
            return fontMetrics().size( Qt::TextSingleLine,
                                       QVariant( QDateTime::currentDateTime() ).toString()
                                       // because that's what the model's doing
                                       ).width();
        case Imap::Mailbox::MsgListModel::SIZE:
            return fontMetrics().size( Qt::TextSingleLine, QLatin1String("888.1 kB") ).width();
        default:
            return QTreeView::sizeHintForColumn( column );
    }
}

void MsgListView::slotFixSize()
{
    if ( header()->visualIndex( Imap::Mailbox::MsgListModel::SEEN ) == -1 ) {
        // calling setResizeMode() would assert()
        qDebug() << "Can't fix the header size of the icon, sorry";
        return;
    }
    header()->setStretchLastSection( false );
    header()->setResizeMode( Imap::Mailbox::MsgListModel::SUBJECT, QHeaderView::Stretch );
    header()->setResizeMode( Imap::Mailbox::MsgListModel::SEEN, QHeaderView::Fixed );
}

}


