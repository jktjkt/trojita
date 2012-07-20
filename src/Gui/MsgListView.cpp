/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

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

#include <QAction>
#include <QApplication>
#include <QDesktopWidget>
#include <QDrag>
#include <QFontMetrics>
#include <QHeaderView>
#include <QPainter>
#include <QSignalMapper>
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/PrettyMsgListModel.h"

namespace Gui
{

MsgListView::MsgListView(QWidget *parent): QTreeView(parent)
{
    connect(header(), SIGNAL(geometriesChanged()), this, SLOT(slotFixSize()));
    connect(this, SIGNAL(expanded(QModelIndex)), this, SLOT(slotExpandWholeSubtree(QModelIndex)));
    connect(header(), SIGNAL(sectionCountChanged(int,int)), this, SLOT(slotSectionCountChanged()));
    header()->setContextMenuPolicy(Qt::ActionsContextMenu);
    headerFieldsMapper = new QSignalMapper(this);
    connect(headerFieldsMapper, SIGNAL(mapped(int)), this, SLOT(slotHeaderSectionVisibilityToggled(int)));

    setUniformRowHeights(true);
    setAllColumnsShowFocus(true);
    setSelectionMode(ExtendedSelection);
    setDragEnabled(true);
    setRootIsDecorated(false);

    setSortingEnabled(true);
    // By default, we don't do any sorting
    header()->setSortIndicator(-1, Qt::AscendingOrder);
}

int MsgListView::sizeHintForColumn(int column) const
{
    QFont boldFont = font();
    boldFont.setBold(true);
    QFontMetrics metric(boldFont);
    switch (column) {
    case Imap::Mailbox::MsgListModel::SEEN:
        return 16;
    case Imap::Mailbox::MsgListModel::SUBJECT:
        return metric.size(Qt::TextSingleLine, QLatin1String("Blesmrt Trojita Foo Bar Random Text")).width();
    case Imap::Mailbox::MsgListModel::FROM:
    case Imap::Mailbox::MsgListModel::TO:
    case Imap::Mailbox::MsgListModel::CC:
    case Imap::Mailbox::MsgListModel::BCC:
        return metric.size(Qt::TextSingleLine, QLatin1String("Blesmrt Trojita")).width();
    case Imap::Mailbox::MsgListModel::DATE:
    case Imap::Mailbox::MsgListModel::RECEIVED_DATE:
        return metric.size(Qt::TextSingleLine,
                                  tr("Mon 10:33")
                                  // Try to use a text which represents "recent mails" because these are likely to be relevant
                                  // in this context
                                 ).width();
    case Imap::Mailbox::MsgListModel::SIZE:
        return metric.size(Qt::TextSingleLine, QLatin1String("888.1 kB")).width();
    default:
        return QTreeView::sizeHintForColumn(column);
    }
}

/** @short Reimplemented to show custom pixmap during drag&drop

  Qt's model-view classes don't provide any means of interfering with the
  QDrag's pixmap so we just rip off QAbstractItemView::startDrag and provide
  our own QPixmap.
*/
void MsgListView::startDrag(Qt::DropActions supportedActions)
{
    // indexes for column 0, i.e. subject
    QModelIndexList baseIndexes;

    Q_FOREACH(const QModelIndex &index, selectedIndexes()) {
        if (!(model()->flags(index) & Qt::ItemIsDragEnabled))
            continue;
        if (index.column() == 0)
            baseIndexes << index;
    }

    if (!baseIndexes.isEmpty()) {
        QMimeData *data = model()->mimeData(baseIndexes);
        if (!data)
            return;

        // use screen width and itemDelegate()->sizeHint() to determine size of the pixmap
        int screenWidth = QApplication::desktop()->screenGeometry(this).width();
        int maxWidth = qMax(400, screenWidth / 4);
        QSize size(maxWidth, 0);

        QStyleOptionViewItem opt;
        opt.initFrom(this);
        opt.rect.setWidth(maxWidth);
        opt.rect.setHeight(itemDelegate()->sizeHint(opt, baseIndexes.at(0)).height());
        size.setHeight(baseIndexes.size() * opt.rect.height());
        // State_Selected provides for nice background of the items
        opt.state |= QStyle::State_Selected;

        // paint list of selected items using itemDelegate() to be consistent with style
        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);

        for (int i = 0; i < baseIndexes.size(); ++i) {
            opt.rect.moveTop(i * opt.rect.height());
            itemDelegate()->paint(&p, opt, baseIndexes.at(i));
        }

        QDrag *drag = new QDrag(this);
        drag->setPixmap(pixmap);
        drag->setMimeData(data);
        drag->setHotSpot(QPoint(0, 0));

        Qt::DropAction dropAction = Qt::IgnoreAction;
        if (defaultDropAction() != Qt::IgnoreAction && (supportedActions & defaultDropAction()))
            dropAction = defaultDropAction();
        else if (supportedActions & Qt::CopyAction && dragDropMode() != QAbstractItemView::InternalMove)
            dropAction = Qt::CopyAction;
        if (drag->exec(supportedActions, dropAction) == Qt::MoveAction) {
            // QAbstractItemView::startDrag calls d->clearOrRemove() here, so
            // this is a copy of QAbstractItemModelPrivate::clearOrRemove();
            const QItemSelection selection = selectionModel()->selection();
            QList<QItemSelectionRange>::const_iterator it = selection.constBegin();

            if (!dragDropOverwriteMode()) {
                for (; it != selection.constEnd(); ++it) {
                    QModelIndex parent = it->parent();
                    if (it->left() != 0)
                        continue;
                    if (it->right() != (model()->columnCount(parent) - 1))
                        continue;
                    int count = it->bottom() - it->top() + 1;
                    model()->removeRows(it->top(), count, parent);
                }
            } else {
                // we can't remove the rows so reset the items (i.e. the view is like a table)
                QModelIndexList list = selection.indexes();
                for (int i = 0; i < list.size(); ++i) {
                    QModelIndex index = list.at(i);
                    QMap<int, QVariant> roles = model()->itemData(index);
                    for (QMap<int, QVariant>::Iterator it = roles.begin(); it != roles.end(); ++it)
                        it.value() = QVariant();
                    model()->setItemData(index, roles);
                }
            }
        }
    }
}

void MsgListView::slotFixSize()
{
    if (header()->visualIndex(Imap::Mailbox::MsgListModel::SEEN) == -1) {
        // calling setResizeMode() would assert()
        return;
    }
    header()->setStretchLastSection(false);
    header()->setResizeMode(Imap::Mailbox::MsgListModel::SUBJECT, QHeaderView::Stretch);
    header()->setResizeMode(Imap::Mailbox::MsgListModel::SEEN, QHeaderView::Fixed);
}

void MsgListView::slotExpandWholeSubtree(const QModelIndex &rootIndex)
{
    if (rootIndex.parent().isValid())
        return;

    QVector<QModelIndex> queue(1, rootIndex);
    for (int i = 0; i < queue.size(); ++i) {
        const QModelIndex currentIndex = queue[i];
        // Append all children to the queue...
        for (int j = 0; j < currentIndex.model()->rowCount(currentIndex); ++j)
            queue.append(currentIndex.child(j, 0));
        // ...and expand the current index
        expand(currentIndex);
    }
}

void MsgListView::slotSectionCountChanged()
{
    Q_ASSERT(header());
    // At first, remove all actions
    QList<QAction *> actions = header()->actions();
    Q_FOREACH(QAction *action, actions) {
        header()->removeAction(action);
        headerFieldsMapper->removeMappings(action);
        action->deleteLater();
    }
    actions.clear();
    // Now add them again
    for (int i = 0; i < header()->count(); ++i) {
        QString message = header()->model() ? header()->model()->headerData(i, Qt::Horizontal).toString() : QString::number(i);
        QAction *action = new QAction(message, this);
        action->setCheckable(true);
        action->setChecked(true);
        connect(action, SIGNAL(toggled(bool)), headerFieldsMapper, SLOT(map()));
        headerFieldsMapper->setMapping(action, i);
        header()->addAction(action);

        // Next, add some special handling of certain columns
        switch (i) {
        case Imap::Mailbox::MsgListModel::SEEN:
            // This column doesn't have a textual description
            action->setText(tr("Seen status"));
            break;
        case Imap::Mailbox::MsgListModel::TO:
        case Imap::Mailbox::MsgListModel::CC:
        case Imap::Mailbox::MsgListModel::BCC:
        case Imap::Mailbox::MsgListModel::RECEIVED_DATE:
            // And these should be hidden by default
            action->toggle();
            break;
        default:
            break;
        }
    }

    // Make sure to kick the header again so that it shows reasonable sizing
    slotFixSize();
}

void MsgListView::slotHeaderSectionVisibilityToggled(int section)
{
    QList<QAction *> actions = header()->actions();
    if (section >= actions.size() || section < 0)
        return;
    bool hide = ! actions[section]->isChecked();

    if (hide && header()->hiddenSectionCount() == header()->count() - 1) {
        // This would hide the very last section, which would hide the whole header view
        actions[section]->setChecked(true);
    } else {
        header()->setSectionHidden(section, hide);
    }
}

/** @short Overriden from QTreeView::setModel

The whole point is that we have to listen for sortingPreferenceChanged to update your header view when sorting is requested
but cannot be fulfilled.
*/
void MsgListView::setModel(QAbstractItemModel *model)
{
    if (this->model()) {
        if (Imap::Mailbox::PrettyMsgListModel *prettyModel = findPrettyMsgListModel(this->model())) {
            disconnect(prettyModel, SIGNAL(sortingPreferenceChanged(int,Qt::SortOrder)),
                       this, SLOT(slotHandleSortCriteriaChanged(int,Qt::SortOrder)));
        }
    }
    QTreeView::setModel(model);
    if (Imap::Mailbox::PrettyMsgListModel *prettyModel = findPrettyMsgListModel(model)) {
        connect(prettyModel, SIGNAL(sortingPreferenceChanged(int,Qt::SortOrder)),
                this, SLOT(slotHandleSortCriteriaChanged(int,Qt::SortOrder)));
    }
}

void MsgListView::slotHandleSortCriteriaChanged(int column, Qt::SortOrder order)
{
    // The if-clause is needed to prevent infinite recursion
    if (header()->sortIndicatorSection() != column || header()->sortIndicatorOrder() != order) {
        header()->setSortIndicator(column, order);
    }
}

/** @short Walk the hierarchy of proxy models up until we stop at the PrettyMsgListModel or the first non-proxy model */
Imap::Mailbox::PrettyMsgListModel *MsgListView::findPrettyMsgListModel(QAbstractItemModel *model)
{
    while (QAbstractProxyModel *proxy = qobject_cast<QAbstractProxyModel*>(model)) {
        Imap::Mailbox::PrettyMsgListModel *prettyModel = qobject_cast<Imap::Mailbox::PrettyMsgListModel*>(proxy);
        if (prettyModel)
            return prettyModel;
        else
            model = proxy->sourceModel();
    }
    return 0;
}

}


