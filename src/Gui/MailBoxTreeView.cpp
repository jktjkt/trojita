/* Copyright (C) 2012 Thomas Gahr <thomas.gahr@physik.uni-muenchen.de>

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

#include "MailBoxTreeView.h"

#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMenu>
#include "IconLoader.h"

namespace Gui {

MailBoxTreeView::MailBoxTreeView()
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
        QAction *moveAction = menu.addAction(loadIcon(QLatin1String("go-jump")), tr("Move here\tShift"));
        menu.addAction(loadIcon(QLatin1String("edit-copy")), tr("Copy here\tCtrl"));
        QAction *cancelAction = menu.addAction(loadIcon(QLatin1String("process-stop")), tr("Cancel"));

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

}
