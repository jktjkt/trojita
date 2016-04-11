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

#include "CompleteMessageWidget.h"
#include <QKeyEvent>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include "Gui/EmbeddedWebView.h"
#include "Gui/FindBar.h"
#include "Gui/MessageView.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

CompleteMessageWidget::CompleteMessageWidget(QWidget *parent, QSettings *settings, Plugins::PluginManager *pluginManager)
    : QWidget(parent)
    , FindBarMixin(this)
{
    setWindowIcon(UiUtils::loadIcon(QStringLiteral("mail-mark-read")));
    messageView = new MessageView(this, settings, pluginManager);
    area = new QScrollArea();
    area->setWidget(messageView);
    area->setWidgetResizable(true);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(area);
    animator = new QPropertyAnimation(area->verticalScrollBar(), "value", this);
    animator->setDuration(250); // the default, maybe play with values
    animator->setEasingCurve(QEasingCurve::InOutCubic); // InOutQuad?

    layout->addWidget(m_findBar);
    connect(messageView, &MessageView::searchRequestedBy,
            this, [this](EmbeddedWebView *w) {
        searchRequestedBy(w);
    });
    // because the FindBarMixin is not a QObject, we have to use lambda above, otherwise a cast
    // from FindBarMixin * to QObject * fails
}

void CompleteMessageWidget::keyPressEvent(QKeyEvent *ke)
{
    if (ke->key() == Qt::Key_Home) {
        animator->setEndValue(area->verticalScrollBar()->minimum());
        animator->start();
    } else if (ke->key() == Qt::Key_End) {
        animator->setEndValue(area->verticalScrollBar()->maximum());
        animator->start();
    } else if (ke->key() == Qt::Key_Space || ke->key() == Qt::Key_Backspace) {
        const int delta = area->verticalScrollBar()->pageStep() * (ke->key() == Qt::Key_Backspace ? -1 : 1);
        const int start = animator->state() == QAbstractAnimation::Running ? animator->endValue().toInt() : area->verticalScrollBar()->value();
        if (animator->state() == QAbstractAnimation::Running) {
            animator->stop();
        }
        animator->setEndValue(qMin(qMax(start + delta, area->verticalScrollBar()->minimum()), area->verticalScrollBar()->maximum()));
        animator->start();
    } else { // noop, but hey.
        QWidget::keyPressEvent(ke);
    }
}

}
