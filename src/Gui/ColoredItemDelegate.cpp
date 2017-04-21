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

#include "ColoredItemDelegate.h"

namespace Gui
{

ColoredItemDelegate::ColoredItemDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

void ColoredItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem viewOption(option);
    QColor itemForegroundColor = index.data(Qt::ForegroundRole).value<QColor>();
    if (itemForegroundColor.isValid()) {
        // Thunderbird works like this and we like it
        viewOption.palette.setColor(QPalette::ColorGroup::Active, QPalette::Highlight, itemForegroundColor);

        // We have to make sure that the text remains readable with the new Highlight color.
        // This covers most of the cases out there if not all. Feel free to extend if you are using a very
        // special theme.
        auto fgLightnessF = viewOption.palette.color(QPalette::ColorGroup::Active, QPalette::HighlightedText).lightnessF();
        auto bgLightnessF = viewOption.palette.color(QPalette::ColorGroup::Active, QPalette::Highlight).lightnessF();
        if (fgLightnessF > 0.75 && bgLightnessF > 0.75) {
            viewOption.palette.setColor(QPalette::ColorGroup::Active, QPalette::HighlightedText, Qt::black);
        } else
        if (fgLightnessF < 0.25 && bgLightnessF < 0.25) {
            viewOption.palette.setColor(QPalette::ColorGroup::Active, QPalette::HighlightedText, Qt::white);
        }

        viewOption.palette.setColor(QPalette::ColorGroup::Inactive, QPalette::HighlightedText, itemForegroundColor);
    }
    QStyledItemDelegate::paint(painter, viewOption, index);
}

}
