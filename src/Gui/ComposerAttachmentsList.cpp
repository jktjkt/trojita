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

#include <QAction>
#include <QDragEnterEvent>
#include <QDebug>
#include <QInputDialog>
#include <QMenu>
#include "Composer/MessageComposer.h"
#include "Gui/ComposerAttachmentsList.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/IconLoader.h"

ComposerAttachmentsList::ComposerAttachmentsList(QWidget *parent):
    QListView(parent), m_dragging(false), m_dragInside(false), m_composer(0)
{
    setMouseTracking( true );
    setAcceptDrops(true);
    setDragDropMode( DragDrop );
    setDragDropOverwriteMode( false );
    setDragEnabled(true);
    setDropIndicatorShown(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &ComposerAttachmentsList::showContextMenu);

    m_actionSendInline = new QAction(UiUtils::loadIcon(QStringLiteral("insert-image")), tr("Send Inline"), this);
    m_actionSendInline->setCheckable(true);
    connect(m_actionSendInline, &QAction::triggered, this, &ComposerAttachmentsList::slotToggledContentDispositionInline);
    addAction(m_actionSendInline);

    m_actionRename = new QAction(UiUtils::loadIcon(QStringLiteral("edit-rename")), tr("Rename..."), this);
    connect(m_actionRename, &QAction::triggered, this, &ComposerAttachmentsList::slotRenameAttachment);
    addAction(m_actionRename);

    m_actionRemoveAttachment = new QAction(UiUtils::loadIcon(QStringLiteral("list-remove")), tr("Remove"), this);
    connect(m_actionRemoveAttachment, &QAction::triggered, this, &ComposerAttachmentsList::slotRemoveAttachment);
    addAction(m_actionRemoveAttachment);

    connect(this, &ComposerAttachmentsList::itemDroppedOut, this, &ComposerAttachmentsList::slotRemoveAttachment);
    connect(this, &QAbstractItemView::clicked, this, &ComposerAttachmentsList::onCurrentChanged);
    connect(this, &QAbstractItemView::activated, this, &ComposerAttachmentsList::onCurrentChanged);
}

void ComposerAttachmentsList::setComposer(Composer::MessageComposer *composer)
{
    // prevent double connections etc
    Q_ASSERT(!m_composer);

    m_composer = composer;
    setModel(m_composer);
    connect(model(), &QAbstractItemModel::rowsRemoved, this, &ComposerAttachmentsList::onAttachmentNumberChanged);
    connect(model(), &QAbstractItemModel::rowsInserted, this, &ComposerAttachmentsList::onAttachmentNumberChanged);
    connect(model(), &QAbstractItemModel::layoutChanged, this, &ComposerAttachmentsList::onAttachmentNumberChanged);
    connect(model(), &QAbstractItemModel::modelReset, this, &ComposerAttachmentsList::onAttachmentNumberChanged);

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

void ComposerAttachmentsList::slotToggledContentDispositionInline(bool checked)
{
    m_composer->setAttachmentContentDisposition(currentIndex(), checked ? Composer::CDN_INLINE : Composer::CDN_ATTACHMENT);
}

void ComposerAttachmentsList::slotRenameAttachment()
{
    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Attachment"), tr("New Name"), QLineEdit::Normal,
                                            currentIndex().data().toString(), &ok);
    if (ok) {
        m_composer->setAttachmentName(currentIndex(), newName);
    }
}

void ComposerAttachmentsList::onAttachmentNumberChanged()
{
    Q_ASSERT(model());
    bool current = currentIndex().isValid();
    m_actionRemoveAttachment->setEnabled(current);
    m_actionSendInline->setEnabled(current);
    m_actionRename->setEnabled(current);
    onCurrentChanged();
}

void ComposerAttachmentsList::onCurrentChanged()
{
    m_actionSendInline->setChecked(false);

    // This is needed because the QVariant::toInt returns zero when faced with null QVariant
    bool ok;
    int res = currentIndex().data(Imap::Mailbox::RoleAttachmentContentDispositionMode).toInt(&ok);
    if (!ok)
        return;

    switch (static_cast<Composer::ContentDisposition>(res)) {
    case Composer::CDN_INLINE:
        m_actionSendInline->setChecked(true);
        break;
    case Composer::CDN_ATTACHMENT:
        // nothing is needed here
        break;
    }
}

void ComposerAttachmentsList::showContextMenu(const QPoint &pos)
{
    // Sometimes currentChanged() is not enough -- we really want to to have these actions to reflect the current selection, if any
    onAttachmentNumberChanged();
    QMenu::exec(actions(), mapToGlobal(pos), 0, this);
}
