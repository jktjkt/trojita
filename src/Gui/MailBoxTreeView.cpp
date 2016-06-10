/* Copyright (C) 2012 Thomas Gahr <thomas.gahr@physik.uni-muenchen.de>
   Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

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

#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMenu>
#include "Gui/MailBoxTreeView.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxFinder.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

MailBoxTreeView::MailBoxTreeView(QWidget *parent)
    : QTreeView(parent)
    , m_mailboxFinder(nullptr)
{
    setUniformRowHeights(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setAllColumnsShowFocus(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setHeaderHidden(true);
    // I wonder what's the best value to use here. Unfortunately, the default is to disable auto expanding.
    setAutoExpandDelay(800);

    // Track expansion/collapsing so that we remember this state despite the asynchronous nature of mailbox loading
    connect(this, &QTreeView::expanded, this, [this](const QModelIndex &what) {
        auto name = what.data(Imap::Mailbox::RoleMailboxName).toString();
        if (!m_desiredExpansionState.contains(name)) {
            m_desiredExpansionState.insert(name);
            emit mailboxExpansionChanged(m_desiredExpansionState.toList());
        }
    });
    connect(this, &QTreeView::collapsed, this, [this](const QModelIndex &what) {
        auto name = what.data(Imap::Mailbox::RoleMailboxName).toString();
        if (m_desiredExpansionState.remove(name)) {
            emit mailboxExpansionChanged(m_desiredExpansionState.toList());
        }
    });
}

/** \reimp

The MailboxFinder has to be kept up-to-speed about these changes.
*/
void MailBoxTreeView::setModel(QAbstractItemModel *model)
{
    delete m_mailboxFinder;
    m_mailboxFinder = new Imap::Mailbox::MailboxFinder(this, model);
    connect(m_mailboxFinder, &Imap::Mailbox::MailboxFinder::mailboxFound,
            this, [this](const QString &, const QModelIndex &index) {
        expand(index);
    });
    connect(model, &QAbstractItemModel::layoutChanged, this, &MailBoxTreeView::resetWatchedMailboxes);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &MailBoxTreeView::resetWatchedMailboxes);
    connect(model, &QAbstractItemModel::modelReset, this, &MailBoxTreeView::resetWatchedMailboxes);
    QTreeView::setModel(model);
    resetWatchedMailboxes();
}

/** @short Reimplemented for more consistent handling of modifiers

  Qt's default behaviour is odd here:
  If you selected the move-action by pressing shift and you release the shift
  key while moving the mouse, the action does not change back to copy. But if you
  then move the mouse over a widget border - i.e. cause dragLeaveEvent, the action
  WILL change back to copy.
  This implementation immitates KDE's behaviour: react to a change in modifiers
  immediately.
*/
void MailBoxTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    QTreeView::dragMoveEvent(event);
    if (!event->isAccepted())
        return;
    if (event->keyboardModifiers() == Qt::ShiftModifier)
        event->setDropAction(Qt::MoveAction);
    else
        event->setDropAction(Qt::CopyAction);
}

/** @short Reimplemented to present the user with a menu to choose between copy or move.

  Does not show the menu if either ctrl (copy messages) or shift (move messages)
  is pressed
*/
void MailBoxTreeView::dropEvent(QDropEvent *event)
{
    if (event->keyboardModifiers() == Qt::ControlModifier) {
        event->setDropAction(Qt::CopyAction);
    } else if (event->keyboardModifiers() == Qt::ShiftModifier) {
        event->setDropAction(Qt::MoveAction);
    } else {
        QMenu menu;
        QAction *moveAction = menu.addAction(UiUtils::loadIcon(QStringLiteral("go-jump")), tr("Move here\tShift"));
        menu.addAction(UiUtils::loadIcon(QStringLiteral("edit-copy")), tr("Copy here\tCtrl"));
        QAction *cancelAction = menu.addAction(UiUtils::loadIcon(QStringLiteral("process-stop")), tr("Cancel"));

        QAction *selectedAction = menu.exec(QCursor::pos());

        // if user closes the menu or selects cancel, ignore the event
        if (!selectedAction || selectedAction == cancelAction) {
            event->ignore();
            return;
        }

        event->setDropAction(selectedAction == moveAction ? Qt::MoveAction : Qt::CopyAction);
    }

    QTreeView::dropEvent(event);
}

/** @short Specify which mailboxes should be expanded

The mailboxes might appear and disappear at any time, so let's make sure that
they are properly expanded/collapsed once they pop in.
*/
void MailBoxTreeView::setDesiredExpansion(const QStringList &mailboxNames)
{
    m_desiredExpansionState = mailboxNames.toSet();
    resetWatchedMailboxes();
}

/** @short Ensure that we watch stuff that we need to watch */
void MailBoxTreeView::resetWatchedMailboxes()
{
    if (m_mailboxFinder) {
        for (const auto &mailbox: m_desiredExpansionState) {
            m_mailboxFinder->addMailbox(mailbox);
        }
    }
}

}
