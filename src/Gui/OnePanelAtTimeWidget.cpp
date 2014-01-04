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

#include "OnePanelAtTimeWidget.h"
#include <QAction>
#include <QMainWindow>
#include <QToolBar>
#include "CompleteMessageWidget.h"
#include "MailBoxTreeView.h"
#include "MessageListWidget.h"
#include "MsgListView.h"

namespace Gui {

OnePanelAtTimeWidget::OnePanelAtTimeWidget(QMainWindow *mainWindow, MailBoxTreeView *mboxTree, MessageListWidget *msgListWidget,
                                           CompleteMessageWidget *messageWidget, QToolBar *toolbar, QAction *actionGoBack):
    QStackedWidget(mainWindow), m_mainWindow(mainWindow), m_mboxTree(mboxTree), m_msgListWidget(msgListWidget),
    m_messageWidget(messageWidget), m_toolbar(toolbar), m_actionGoBack(actionGoBack)
{
    addWidget(m_mboxTree);
    addWidget(m_msgListWidget);
    addWidget(m_messageWidget);
    setCurrentWidget(m_mboxTree);

    connect(m_msgListWidget->tree, SIGNAL(clicked(QModelIndex)), this, SLOT(slotOneAtTimeGoDeeper()));
    connect(m_msgListWidget->tree, SIGNAL(activated(QModelIndex)), this, SLOT(slotOneAtTimeGoDeeper()));
    connect(m_mboxTree, SIGNAL(clicked(QModelIndex)), this, SLOT(slotOneAtTimeGoDeeper()));
    connect(m_mboxTree, SIGNAL(activated(QModelIndex)), this, SLOT(slotOneAtTimeGoDeeper()));
    connect(m_actionGoBack, SIGNAL(triggered()), this, SLOT(slotOneAtTimeGoBack()));

    // The list view is configured to auto-emit activated(QModelIndex) after a short while when the user has navigated
    // to an index through keyboard. Of course, this doesn't play terribly well with this layout.
    m_msgListWidget->tree->setAutoActivateAfterKeyNavigation(false);

    m_toolbar->addAction(m_actionGoBack);
}

OnePanelAtTimeWidget::~OnePanelAtTimeWidget()
{
    while (count()) {
        QWidget *w = widget(0);
        removeWidget(w);
        w->show();
    }
    m_msgListWidget->tree->setAutoActivateAfterKeyNavigation(false);

    m_toolbar->removeAction(m_actionGoBack);

    // The size of the widgets is still wrong. Let's fix this.
    if (m_mainWindow->isMaximized()) {
        m_mainWindow->showNormal();
        m_mainWindow->showMaximized();
    } else {
        m_mainWindow->resize(m_mainWindow->sizeHint());
    }
}

void OnePanelAtTimeWidget::slotOneAtTimeGoBack()
{
    if (currentIndex() > 0)
        setCurrentIndex(currentIndex() - 1);

    m_actionGoBack->setEnabled(currentIndex() > 0);
}

void OnePanelAtTimeWidget::slotOneAtTimeGoDeeper()
{
    QWidget *w = qobject_cast<QWidget*>(sender());
    Q_ASSERT(w);

    // Careful here: some of the events are, unfortunately, emitted twice (one for clicked(), another time for activated())
    if (!w->isVisible())
        return;

    if (currentIndex() < count() - 1)
        setCurrentIndex(currentIndex() + 1);

    m_actionGoBack->setEnabled(currentIndex() > 0);
}

}
