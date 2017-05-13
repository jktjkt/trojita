/* Copyright (C) Roland Pallai <dap78@magex.hu>

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

#include "MsgItemDelegate.h"
#include <QTreeView>
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MsgListModel.h"
#include "MsgListView.h"

namespace Gui
{

MsgItemDelegate::MsgItemDelegate(QObject *parent, Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel) :
    ColoredItemDelegate(parent), m_favoriteTagsModel(m_favoriteTagsModel)
{
}

QColor MsgItemDelegate::itemColor(const QModelIndex &index) const
{
    auto view = static_cast<MsgListView*>(parent());

    Q_ASSERT(index.isValid());
    QModelIndex sindex = index.sibling(index.row(), Imap::Mailbox::MsgListModel::SUBJECT);
    QVariant flags;
    if (!sindex.model()->hasChildren(sindex) || view->isExpanded(sindex)) {
        flags = index.data(Imap::Mailbox::RoleMessageFlags);
    } else {
        flags = index.data(Imap::Mailbox::RoleThreadAggregatedFlags);
    }

    return m_favoriteTagsModel->findBestColorForTags(flags.toStringList());
}

void MsgItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem viewOption(option);
    auto foregroundColor = itemColor(index);
    if (foregroundColor.isValid())
        viewOption.palette.setColor(QPalette::Text, foregroundColor);
    ColoredItemDelegate::paintWithForeground(painter, viewOption, index, foregroundColor);
}

}
