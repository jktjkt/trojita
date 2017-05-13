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

#ifndef MSGITEMDELEGATE_H
#define MSGITEMDELEGATE_H

#include <QModelIndex>
#include <QPainter>
#include <QStyleOptionViewItem>
#include "Imap/Model/FavoriteTagsModel.h"
#include "ColoredItemDelegate.h"

namespace Gui {

/** @short Painting colorized messages in MsgListView
*/
class MsgItemDelegate : public ColoredItemDelegate
{
    Q_OBJECT
public:
    explicit MsgItemDelegate(QObject* parent, Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
private:
    QColor itemColor(const QModelIndex &index) const;

    Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel;
};

}

#endif	/* MSGITEMDELEGATE_H */
