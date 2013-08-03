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

#include "ComposerAttachmentsList.h"
#include <QAction>
#include <QDragEnterEvent>
#include <QDebug>
#include "Composer/MessageComposer.h"

ComposerAttachmentsList::ComposerAttachmentsList(QWidget *parent):
    QListView(parent), m_dragging(false), m_dragInside(false), m_composer(0)
{
    setMouseTracking( true );
    setAcceptDrops(true);
    setDragDropMode( DragDrop );
    setDragDropOverwriteMode( false );
    setDragEnabled(true);
    setDropIndicatorShown(false);
    setContextMenuPolicy(Qt::ActionsContextMenu);

    m_actionRemoveAttachment = new QAction(tr("Remove"), this);
    connect(m_actionRemoveAttachment, SIGNAL(triggered()), this, SLOT(slotRemoveAttachment()));
    addAction(m_actionRemoveAttachment);

    connect(this, SIGNAL(itemDroppedOut()), this, SLOT(slotRemoveAttachment()));
}

void ComposerAttachmentsList::setComposer(Composer::MessageComposer *composer)
{
    // prevent double connections etc
    Q_ASSERT(!m_composer);

    m_composer = composer;
    setModel(m_composer);
    connect(model(), SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(onAttachmentNumberChanged()));
    connect(model(), SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(onAttachmentNumberChanged()));
    connect(model(), SIGNAL(layoutChanged()), this, SLOT(onAttachmentNumberChanged()));
    connect(model(), SIGNAL(modelReset()), this, SLOT(onAttachmentNumberChanged()));

    onAttachmentNumberChanged();
}

void ComposerAttachmentsList::startDrag(Qt::DropActions da)
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


void ComposerAttachmentsList::dragEnterEvent(QDragEnterEvent *de)
{
    if (m_dragging)
        m_dragInside = true;
    QListView::dragEnterEvent(de);
}

void ComposerAttachmentsList::dragLeaveEvent(QDragLeaveEvent *de)
{
    if (m_dragging)
        m_dragInside = false;
    QListView::dragLeaveEvent(de);
}

void ComposerAttachmentsList::slotRemoveAttachment()
{
    m_composer->removeAttachment(currentIndex());
}

void ComposerAttachmentsList::onAttachmentNumberChanged()
{
    Q_ASSERT(model());
    m_actionRemoveAttachment->setEnabled(model()->rowCount() > 0);
}
