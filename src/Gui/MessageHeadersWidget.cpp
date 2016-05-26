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

#include "Gui/MessageHeadersWidget.h"
#include <QAction>
#include <QModelIndex>
#include <QVBoxLayout>
#include <QWebView>
#include "Gui/FindBar.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Network/MsgPartNetAccessManager.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

MessageHeadersWidget::MessageHeadersWidget(QWidget *parent, const QModelIndex &messageIndex)
    : QWidget(parent)
    , FindBarMixin(this)
    , m_widget(new QWebView(this))

{
    setWindowIcon(UiUtils::loadIcon(QStringLiteral("text-x-hex")));
    //: Translators: %1 is the UID of a message (a number) and %2 is the name of a mailbox.
    setWindowTitle(tr("Message headers of UID %1 in %2").arg(
                       QString::number(messageIndex.data(Imap::Mailbox::RoleMessageUid).toUInt()),
                       messageIndex.parent().parent().data(Imap::Mailbox::RoleMailboxName).toString()
                       ));
    Q_ASSERT(messageIndex.isValid());

    auto netAccess = new Imap::Network::MsgPartNetAccessManager(this);
    netAccess->setModelMessage(messageIndex);
    m_widget->page()->setNetworkAccessManager(netAccess);
    m_widget->setUrl(QUrl(QStringLiteral("trojita-imap://msg/HEADER")));

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

}
