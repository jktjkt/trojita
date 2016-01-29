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
#include <QFontDatabase>
#include <QMenu>
#include <QNetworkReply>
#include <QWebFrame>

#include "SimplePartWidget.h"
#include "Common/MetaTypes.h"
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
        connect(this, &QWebView::loadStarted, m_messageView, &MessageView::onWebViewLoadStarted);
        connect(this, &QWebView::loadFinished, m_messageView, &MessageView::onWebViewLoadFinished);
    }

    QUrl url;
    url.setScheme(QStringLiteral("trojita-imap"));
    url.setHost(QStringLiteral("msg"));
    url.setPath(partIndex.data(Imap::Mailbox::RolePartPathToPart).toString());
    if (partIndex.data(Imap::Mailbox::RolePartMimeType).toString() == QLatin1String("text/plain")) {
        if (partIndex.data(Imap::Mailbox::RolePartOctets).toUInt() < 100 * 1024) {
            connect(this, &QWebView::loadFinished, this, &SimplePartWidget::slotMarkupPlainText);
        } else {
            QFont font(QFontDatabase::systemFont(QFontDatabase::FixedFont));
            setStaticWidth(QFontMetrics(font).maxWidth()*90);
            QWebSettings *s = settings();
            // TODO wtf does this not work?
            const QString css(QLatin1String("data:text/css;charset=utf-8;base64,") +
                              QLatin1String(QByteArray("pre{word-wrap:normal !important;white-space:pre !important;}").toBase64()));
            s->setUserStyleSheetUrl(css);
            s->setFontFamily(QWebSettings::StandardFont, font.family());
        }
    }
    load(url);

    m_savePart = new QAction(tr("Save this message part..."), this);
    connect(m_savePart, &QAction::triggered, this, &SimplePartWidget::slotDownloadPart);
    this->addAction(m_savePart);

    m_saveMessage = new QAction(tr("Save whole message..."), this);
    connect(m_saveMessage, &QAction::triggered, this, &SimplePartWidget::slotDownloadMessage);
    this->addAction(m_saveMessage);

    m_findAction = new QAction(tr("Search..."), this);
    m_findAction->setShortcut(tr("Ctrl+F"));
    connect(m_findAction, &QAction::triggered, this, &SimplePartWidget::searchDialogRequested);
    addAction(m_findAction);

    setContextMenuPolicy(Qt::CustomContextMenu);

    // It is actually OK to construct this widget without any connection to a messageView -- this is often used when
    // displaying message source or message headers. Let's silence the QObject::connect warning.
    if (m_messageView) {
        connect(this, &QWidget::customContextMenuRequested, m_messageView, &MessageView::partContextMenuRequested);
        connect(this, &SimplePartWidget::searchDialogRequested, m_messageView, &MessageView::triggerSearchDialog);
        // The targets expect the sender() of the signal to be a SimplePartWidget, not a QWebPage,
        // which means we have to do this indirection
        connect(page(), &QWebPage::linkHovered, this, &SimplePartWidget::linkHovered);
        connect(this, &SimplePartWidget::linkHovered, m_messageView, &MessageView::partLinkHovered);
        connect(page(), &QWebPage::downloadRequested, this, &SimplePartWidget::slotDownloadImage);

        installEventFilter(m_messageView);
    }
}

void SimplePartWidget::slotMarkupPlainText()
{
    // NOTICE "single shot", we get a recursion otherwise!
    disconnect(this, &QWebView::loadFinished, this, &SimplePartWidget::slotMarkupPlainText);

    // If there's no data, don't try to "fix it up"
    if (!m_partIndex.isValid() || !m_partIndex.data(Imap::Mailbox::RoleIsFetched).toBool())
        return;

    QPalette palette = QApplication::palette();

    // and finally set the marked up page.
    page()->mainFrame()->setHtml(UiUtils::htmlizedTextPart(m_partIndex, QFontDatabase::systemFont(QFontDatabase::FixedFont),
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

void SimplePartWidget::buildContextMenu(const QPoint &point, QMenu &menu) const
{
    menu.addAction(m_findAction);
    menu.addAction(pageAction(QWebPage::Copy));
    menu.addAction(pageAction(QWebPage::SelectAll));
    if (!page()->mainFrame()->hitTestContent(point).linkUrl().isEmpty()) {
        menu.addSeparator();
        menu.addAction(pageAction(QWebPage::CopyLinkToClipboard));
    }
    menu.addSeparator();
    menu.addAction(m_savePart);
    menu.addAction(m_saveMessage);
    if (!page()->mainFrame()->hitTestContent(point).imageUrl().isEmpty()) {
        menu.addAction(pageAction(QWebPage::DownloadImageToDisk));
    }
}

void SimplePartWidget::slotDownloadPart()
{
    Imap::Network::FileDownloadManager *manager = new Imap::Network::FileDownloadManager(this, m_netAccessManager, m_partIndex);
    connect(manager, &Imap::Network::FileDownloadManager::fileNameRequested, this, &SimplePartWidget::slotFileNameRequested);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, m_messageView, &MessageView::transferError);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, manager, &QObject::deleteLater);
    connect(manager, &Imap::Network::FileDownloadManager::succeeded, manager, &QObject::deleteLater);
    manager->downloadPart();
}

void SimplePartWidget::slotDownloadMessage()
{
    QModelIndex index = m_partIndex.data(Imap::Mailbox::RolePartMessageIndex).toModelIndex();
    Imap::Network::FileDownloadManager *manager = new Imap::Network::FileDownloadManager(this, m_netAccessManager, index);
    connect(manager, &Imap::Network::FileDownloadManager::fileNameRequested, this, &SimplePartWidget::slotFileNameRequested);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, m_messageView, &MessageView::transferError);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, manager, &QObject::deleteLater);
    connect(manager, &Imap::Network::FileDownloadManager::succeeded, manager, &QObject::deleteLater);
    manager->downloadMessage();
}

void SimplePartWidget::slotDownloadImage(const QNetworkRequest &req)
{
    Imap::Network::FileDownloadManager *manager = new Imap::Network::FileDownloadManager(this, m_netAccessManager, req.url(), m_partIndex.parent());
    connect(manager, &Imap::Network::FileDownloadManager::fileNameRequested, this, &SimplePartWidget::slotFileNameRequested);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, m_messageView, &MessageView::transferError);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, manager, &QObject::deleteLater);
    connect(manager, &Imap::Network::FileDownloadManager::succeeded, manager, &QObject::deleteLater);
    manager->downloadPart();
}

}

