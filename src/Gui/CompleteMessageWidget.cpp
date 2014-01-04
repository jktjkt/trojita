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

#include "CompleteMessageWidget.h"
#include <QKeyEvent>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include "Gui/FindBar.h"
#include "Gui/IconLoader.h"
#include "Gui/MessageView.h"

namespace Gui {

CompleteMessageWidget::CompleteMessageWidget(QWidget *parent, QSettings *settings): QWidget(parent)
{
    setWindowIcon(loadIcon(QLatin1String("mail-mark-read")));
    messageView = new MessageView(this, settings);
    area = new QScrollArea();
    area->setWidget(messageView);
    area->setWidgetResizable(true);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(area);
    animator = new QPropertyAnimation(area->verticalScrollBar(), "value", this);
    animator->setDuration(250); // the default, maybe play with values
    animator->setEasingCurve(QEasingCurve::InOutCubic); // InOutQuad?

    m_findBar = new FindBar(this);
    layout->addWidget(m_findBar);

    connect(messageView, SIGNAL(searchRequestedBy(QWebView*)), this, SLOT(searchRequestedBy(QWebView*)));
}

void CompleteMessageWidget::searchRequestedBy(QWebView *webView)
{
    if (m_findBar->isVisible() || !webView) {
        m_findBar->setAssociatedWebView(0);
        m_findBar->hide();
    } else {
        m_findBar->setAssociatedWebView(webView);
        m_findBar->show();
    }
}

void CompleteMessageWidget::keyPressEvent(QKeyEvent *ke)
{
    if (ke->key() == Qt::Key_Home) {
        animator->setEndValue(area->verticalScrollBar()->minimum());
        animator->start();
    } else if (ke->key() == Qt::Key_End) {
        animator->setEndValue(area->verticalScrollBar()->maximum());
        animator->start();
    } else { // noop, but hey.
        QWidget::keyPressEvent(ke);
    }
}

}
