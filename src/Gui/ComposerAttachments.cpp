/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ComposerAttachments.h"
#include <QDragEnterEvent>
#include <QDebug>

ComposerAttachments::ComposerAttachments(QWidget *parent) : QListView(parent), m_dragging(false), m_dragInside(false)
{
    setMouseTracking( true );
    setAcceptDrops(true);
    setDragDropMode( DragDrop );
    setDragDropOverwriteMode( false );
    setDragEnabled(true);
    setDropIndicatorShown(false);
}


void ComposerAttachments::startDrag(Qt::DropActions da)
{
    m_dragging = true;
    m_dragInside = true;
    const QModelIndex idx = indexAt(mapFromGlobal(QCursor::pos()));
    if (!idx.isValid())
        setCurrentIndex(idx);
    QListView::startDrag(da);
    m_dragging = false;
    if (!m_dragInside) {
        emit itemDroppedOut();
    }
}


void ComposerAttachments::dragEnterEvent(QDragEnterEvent *de)
{
    if (m_dragging)
        m_dragInside = true;
    QListView::dragEnterEvent(de);
}

void ComposerAttachments::dragLeaveEvent(QDragLeaveEvent *de)
{
    if (m_dragging)
        m_dragInside = false;
    QListView::dragLeaveEvent(de);
}
