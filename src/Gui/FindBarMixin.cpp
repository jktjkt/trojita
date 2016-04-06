/* Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

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

#include "Gui/FindBar.h"
#include "Gui/FindBarMixin.h"

namespace Gui {

FindBarMixin::FindBarMixin(QWidget *parent)
    : m_findBar(new FindBar(parent))
{
}

void FindBarMixin::searchRequestedBy(QWebView *webView)
{
    if (m_findBar->isVisible() || !webView) {
        // NOTICE: hide must go before resetting the AssociatedWebView
        // since it clears search results in the AssociatedWebView (otherise hightlights would stay)
        m_findBar->hide();
        m_findBar->setAssociatedWebView(nullptr);
    } else {
        m_findBar->setAssociatedWebView(webView);
        m_findBar->show();
    }
}

}
