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
#include <QApplication>
#include <QFileDialog>
#include <QNetworkReply>
#include <QWebFrame>

#include "SimplePartWidget.h"
#include "Common/Paths.h"
#include "Gui/MessageView.h" // so that the compiler knows that it's a QObject
#include "Gui/Util.h"
#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Network/FileDownloadManager.h"
#include "Imap/Model/Utils.h"
#include "UiUtils/Color.h"

namespace Gui
{

SimplePartWidget::SimplePartWidget(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager,
                                   const QModelIndex &partIndex, MessageView *messageView):
    EmbeddedWebView(parent, manager), m_partIndex(partIndex), m_messageView(messageView), m_netAccessManager(manager)
{
    Q_ASSERT(partIndex.isValid());

    if (m_messageView) {
        connect(this, SIGNAL(loadStarted()), m_messageView, SLOT(onWebViewLoadStarted()));
        connect(this, SIGNAL(loadFinished(bool)), m_messageView, SLOT(onWebViewLoadFinished()));
    }

    QUrl url;
    url.setScheme(QLatin1String("trojita-imap"));
    url.setHost(QLatin1String("msg"));
    url.setPath(partIndex.data(Imap::Mailbox::RolePartPathToPart).toString());
    if (partIndex.data(Imap::Mailbox::RolePartMimeType).toString() == QLatin1String("text/plain")
            && partIndex.data(Imap::Mailbox::RolePartOctets).toUInt() < 100 * 1024) {
        connect(this, SIGNAL(loadFinished(bool)), this, SLOT(slotMarkupPlainText()));
    }
    load(url);

    m_savePart = new QAction(tr("Save this message part..."), this);
    connect(m_savePart, SIGNAL(triggered()), this, SLOT(slotDownloadPart()));
    this->addAction(m_savePart);

    m_saveMessage = new QAction(tr("Save whole message..."), this);
    connect(m_saveMessage, SIGNAL(triggered()), this, SLOT(slotDownloadMessage()));
    this->addAction(m_saveMessage);

    m_findAction = new QAction(tr("Search..."), this);
    m_findAction->setShortcut(tr("Ctrl+F"));
    connect(m_findAction, SIGNAL(triggered()), this, SIGNAL(searchDialogRequested()));
    addAction(m_findAction);

    setContextMenuPolicy(Qt::CustomContextMenu);

    // It is actually OK to construct this widget without any connection to a messageView -- this is often used when
    // displaying message source or message headers. Let's silence the QObject::connect warning.
    if (m_messageView) {
        connect(this, SIGNAL(customContextMenuRequested(QPoint)), m_messageView, SLOT(partContextMenuRequested(QPoint)));
        connect(this, SIGNAL(searchDialogRequested()), m_messageView, SLOT(triggerSearchDialog()));
        // The targets expect the sender() of the signal to be a SimplePartWidget, not a QWebPage,
        // which means we have to do this indirection
        connect(page(), SIGNAL(linkHovered(QString,QString,QString)), this, SIGNAL(linkHovered(QString,QString,QString)));
        connect(this, SIGNAL(linkHovered(QString,QString,QString)),
                m_messageView, SLOT(partLinkHovered(QString,QString,QString)));

        installEventFilter(m_messageView);
    }
}

void SimplePartWidget::slotMarkupPlainText()
{
    // NOTICE "single shot", we get a recursion otherwise!
    disconnect(this, SIGNAL(loadFinished(bool)), this, SLOT(slotMarkupPlainText()));

    // If there's no data, don't try to "fix it up"
    if (!m_partIndex.isValid() || !m_partIndex.data(Imap::Mailbox::RoleIsFetched).toBool())
        return;

    QPalette palette = QApplication::palette();

    // and finally set the marked up page.
    page()->mainFrame()->setHtml(UiUtils::htmlizedTextPart(m_partIndex, Gui::Util::systemMonospaceFont(),
                                                           palette.base().color(), palette.text().color(),
                                                           palette.link().color(), palette.linkVisited().color()));
}

void SimplePartWidget::slotFileNameRequested(QString *fileName)
{
    *fileName = QFileDialog::getSaveFileName(this, tr("Save Attachment"),
                *fileName, QString(),
                0, QFileDialog::HideNameFilterDetails
                                            );
}

QString SimplePartWidget::quoteMe() const
{
    QString selection = selectedText();
    if (selection.isEmpty())
        return page()->mainFrame()->toPlainText();
    else
        return selection;
}

void SimplePartWidget::reloadContents()
{
    EmbeddedWebView::reload();
}

QList<QAction *> SimplePartWidget::contextMenuSpecificActions() const
{
    return QList<QAction*>() << m_savePart << m_saveMessage << m_findAction;
}

void SimplePartWidget::slotDownloadPart()
{
    Imap::Network::FileDownloadManager *manager = new Imap::Network::FileDownloadManager(this, m_netAccessManager, m_partIndex);
    connect(manager, SIGNAL(fileNameRequested(QString *)), this, SLOT(slotFileNameRequested(QString *)));
    connect(manager, SIGNAL(transferError(QString)), m_messageView, SIGNAL(transferError(QString)));
    connect(manager, SIGNAL(transferError(QString)), manager, SLOT(deleteLater()));
    connect(manager, SIGNAL(succeeded()), manager, SLOT(deleteLater()));
    manager->downloadPart();
}

void SimplePartWidget::slotDownloadMessage()
{
    QModelIndex index;
    if (m_partIndex.isValid()) {
        index = Imap::Mailbox::Model::findMessageForItem(Imap::deproxifiedIndex(m_partIndex));
    }

    Imap::Network::FileDownloadManager *manager = new Imap::Network::FileDownloadManager(this, m_netAccessManager, index);
    connect(manager, SIGNAL(fileNameRequested(QString *)), this, SLOT(slotFileNameRequested(QString *)));
    connect(manager, SIGNAL(transferError(QString)), m_messageView, SIGNAL(transferError(QString)));
    connect(manager, SIGNAL(transferError(QString)), manager, SLOT(deleteLater()));
    connect(manager, SIGNAL(succeeded()), manager, SLOT(deleteLater()));
    manager->downloadMessage();
}

}

