#include "MailBoxTreeView.h"

#include <QCursor>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMenu>

namespace Gui {

MailBoxTreeView::MailBoxTreeView() :
    QTreeView(), dropMightBeAccepted(false)
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

/** reimplemented to keep track if QTreeView accepted the event
*/
void MailBoxTreeView::dragEnterEvent(QDragEnterEvent *event)
{
    QTreeView::dragEnterEvent(event);
    dropMightBeAccepted = event->isAccepted();
}

/** reimplemented to keep track if QTreeView accepted the event
*/
void MailBoxTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    QTreeView::dragMoveEvent(event);
    dropMightBeAccepted = event->isAccepted();
}

/** Present the user with a menu to choose between copy or move.
 The menu is only shown if neither dragEnterEvent nor dragMoveEvent ignored the
 event, i.e. the drop is not allowed at the mouse pointer's position
*/
void MailBoxTreeView::dropEvent(QDropEvent *event)
{
    if(!dropMightBeAccepted) {
        // if neither enter nor move event accepted the event, the mouse is
        // not hovering over an allowed item - ignore right away
        event->ignore();
        return;
    }
    // no matter what happens, reset the state
    dropMightBeAccepted = false;

    QMenu menu;
    // TODO: provide icons for copy and move actions
    QAction *moveAction = menu.addAction(tr("Move here"));
    menu.addAction(tr("Copy here"));
    QAction *cancelAction = menu.addAction(style()->standardIcon(QStyle::SP_DialogCancelButton), tr("Cancel"));

    QAction *selectedAction = menu.exec(QCursor::pos());

    // if user closes the menu or selects cancel, ignore the event
    if(!selectedAction || selectedAction == cancelAction) {
        event->ignore();
        return;
    }

    event->setDropAction(selectedAction == moveAction ? Qt::MoveAction : Qt::CopyAction);

    QTreeView::dropEvent(event);
}

}
