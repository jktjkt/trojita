/* Copyright (C) 2013  Ahmed Ibrahim Khalil <ahmedibrahimkhali@gmail.com>
   Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

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

#include "MessageSourceWidget.h"
#include <QAction>
#include <QModelIndex>
#include <QVBoxLayout>
#include <QWebView>
#include "Gui/FindBar.h"
#include "Gui/Spinner.h"
#include "Imap/Model/FullMessageCombiner.h"
#include "UiUtils/IconLoader.h"

namespace Gui
{

MessageSourceWidget::MessageSourceWidget(QWidget *parent, const QModelIndex &messageIndex)
    : QWidget(parent)
    , FindBarMixin(this)
    , m_combiner(nullptr)
    , m_loadingSpinner(nullptr)
    , m_widget(new QWebView(this))
{
    setWindowIcon(UiUtils::loadIcon(QStringLiteral("text-x-hex")));
    Q_ASSERT(messageIndex.isValid());
    m_widget->page()->setNetworkAccessManager(0);

    m_loadingSpinner = new Spinner(this);
    m_loadingSpinner->setText(tr("Fetching\nMessage"));
    m_loadingSpinner->setType(Spinner::Sun);
    m_loadingSpinner->start(250);

    m_combiner = new Imap::Mailbox::FullMessageCombiner(messageIndex, this);
    connect(m_combiner, &Imap::Mailbox::FullMessageCombiner::completed, this, &MessageSourceWidget::slotCompleted);
    connect(m_combiner, &Imap::Mailbox::FullMessageCombiner::failed, this, &MessageSourceWidget::slotError);
    m_combiner->load();

    auto find = new QAction(UiUtils::loadIcon(QStringLiteral("edit-find")), tr("Search..."), this);
    find->setShortcut(tr("Ctrl+F"));
    connect(find, &QAction::triggered, this, [this]() {
        searchRequestedBy(m_widget);
    });
    addAction(find);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_widget, 1);
    layout->addWidget(m_findBar);
    setLayout(layout);
}

void MessageSourceWidget::slotCompleted()
{
    m_loadingSpinner->stop();
    m_widget->setContent(m_combiner->data(), QStringLiteral("text/plain"));
}

void MessageSourceWidget::slotError(const QString &message)
{
    m_loadingSpinner->stop();
    m_widget->setContent(message.toUtf8(), QStringLiteral("text/plain; charset=utf-8"));
}

}
