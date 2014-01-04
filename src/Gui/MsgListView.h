/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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
#ifndef MSGLISTVIEW_H
#define MSGLISTVIEW_H

#include <QHeaderView>
#include <QTreeView>

class QSignalMapper;

namespace Imap {
namespace Mailbox {
class PrettyMsgListModel;
}
}

namespace Gui {

/** @short A slightly tweaked QTreeView optimized for showing a list of messages in one mailbox

The optimizations (or rather modifications) include:
- automatically expanding a whole subtree when root item is expanded
- setting up reasonable size hints for all columns
*/
class MsgListView : public QTreeView
{
    Q_OBJECT
public:
    explicit MsgListView(QWidget *parent=0);
    virtual ~MsgListView() {}
    void setModel(QAbstractItemModel *model);
    void setAutoActivateAfterKeyNavigation(bool enabled);
    void updateActionsAfterRestoredState();
    virtual int sizeHintForColumn(int column) const;
    QHeaderView::ResizeMode resizeModeForColumn(const int column) const;
protected:
    void keyPressEvent(QKeyEvent *ke);
    void keyReleaseEvent(QKeyEvent *ke);
    virtual void startDrag(Qt::DropActions supportedActions);
    bool event(QEvent *event);
private slots:
    void slotFixSize();
    /** @short Expand all items below current root index */
    void slotExpandWholeSubtree(const QModelIndex &rootIndex);
    /** @short Update header actions for showing/hiding columns */
    void slotSectionCountChanged();
    /** @short Show/hide a corresponding column */
    void slotHeaderSectionVisibilityToggled(int section);
    /** @short Pick up the change of the sort critera */
    void slotHandleSortCriteriaChanged(int column, Qt::SortOrder order);
    /** @short conditionally emits activated(currentIndex()) for keyboard events */
    void slotCurrentActivated();
private:
    static Imap::Mailbox::PrettyMsgListModel *findPrettyMsgListModel(QAbstractItemModel *model);

    QSignalMapper *headerFieldsMapper;
    QTimer *m_naviActivationTimer;
    bool m_autoActivateAfterKeyNavigation;
    bool m_autoResizeSections;
};

}

#endif // MSGLISTVIEW_H
