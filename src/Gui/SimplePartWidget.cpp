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
#include <QWheelEvent>

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
#include "UiUtils/IconLoader.h"

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
        if (partIndex.data(Imap::Mailbox::RolePartOctets).toULongLong() < 100 * 1024) {
            connect(this, &QWebView::loadFinished, this, &SimplePartWidget::slotMarkupPlainText);
        } else {
            QFont font(QFontDatabase::systemFont(QFontDatabase::FixedFont));
            setStaticWidth(QFontMetrics(font).maxWidth()*90);
            addCustomStylesheet(QStringLiteral("pre{word-wrap:normal !important;white-space:pre !important;}"));
            QWebSettings *s = settings();
            s->setFontFamily(QWebSettings::StandardFont, font.family());
        }
    }
    load(url);

    m_savePart = new QAction(UiUtils::loadIcon(QStringLiteral("document-save")), tr("Save this message part..."), this);
    connect(m_savePart, &QAction::triggered, this, &SimplePartWidget::slotDownloadPart);
    this->addAction(m_savePart);

    m_saveMessage = new QAction(UiUtils::loadIcon(QStringLiteral("document-save-all")), tr("Save whole message..."), this);
    connect(m_saveMessage, &QAction::triggered, this, &SimplePartWidget::slotDownloadMessage);
    this->addAction(m_saveMessage);

    m_findAction = new QAction(UiUtils::loadIcon(QStringLiteral("edit-find")), tr("Search..."), this);
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

const auto zoomConstant = 1.1;

void SimplePartWidget::zoomIn()
{
    setZoomFactor(zoomFactor() * zoomConstant);
    constrainSize();
}

void SimplePartWidget::zoomOut()
{
    setZoomFactor(zoomFactor() / zoomConstant);
    constrainSize();
}

void SimplePartWidget::zoomOriginal()
{
    setZoomFactor(1);
    constrainSize();
}

void SimplePartWidget::buildContextMenu(const QPoint &point, QMenu &menu) const
{
    menu.addAction(m_findAction);
    auto a = pageAction(QWebPage::Copy);
    a->setIcon(UiUtils::loadIcon(QStringLiteral("edit-copy")));
    menu.addAction(a);
    a = pageAction(QWebPage::SelectAll);
    a->setIcon(UiUtils::loadIcon(QStringLiteral("edit-select-all")));
    menu.addAction(a);
    if (!page()->mainFrame()->hitTestContent(point).linkUrl().isEmpty()) {
        menu.addSeparator();
        a = pageAction(QWebPage::CopyLinkToClipboard);
        a->setIcon(UiUtils::loadIcon(QStringLiteral("edit-copy")));
        menu.addAction(a);
    }
    menu.addSeparator();
    menu.addAction(m_savePart);
    menu.addAction(m_saveMessage);
    if (!page()->mainFrame()->hitTestContent(point).imageUrl().isEmpty()) {
        a = pageAction(QWebPage::DownloadImageToDisk);
        a->setIcon(UiUtils::loadIcon(QStringLiteral("download")));
        menu.addAction(a);
    }
    menu.addSeparator();
    QMenu *colorSchemeMenu = menu.addMenu(UiUtils::loadIcon(QStringLiteral("colorneg")), tr("Color scheme"));
    QActionGroup *ag = new QActionGroup(colorSchemeMenu);
    for (auto item: supportedColorSchemes()) {
        QAction *a = colorSchemeMenu->addAction(item.second);
        connect(a, &QAction::triggered, this, [this, item](){
           const_cast<SimplePartWidget*>(this)->setColorScheme(item.first);
        });
        a->setCheckable(true);
        if (item.first == m_colorScheme) {
            a->setChecked(true);
        }
        a->setActionGroup(ag);
    }

    auto zoomMenu = menu.addMenu(UiUtils::loadIcon(QStringLiteral("zoom")), tr("Zoom"));
    if (m_messageView) {
        zoomMenu->addAction(m_messageView->m_zoomIn);
        zoomMenu->addAction(m_messageView->m_zoomOut);
        zoomMenu->addAction(m_messageView->m_zoomOriginal);
    } else {
        auto zoomIn = zoomMenu->addAction(UiUtils::loadIcon(QStringLiteral("zoom-in")), tr("Zoom In"));
        zoomIn->setShortcut(QKeySequence::ZoomIn);
        connect(zoomIn, &QAction::triggered, this, &SimplePartWidget::zoomIn);

        auto zoomOut = zoomMenu->addAction(UiUtils::loadIcon(QStringLiteral("zoom-out")), tr("Zoom Out"));
        zoomOut->setShortcut(QKeySequence::ZoomOut);
        connect(zoomOut, &QAction::triggered, this, &SimplePartWidget::zoomOut);

        auto zoomOriginal = zoomMenu->addAction(UiUtils::loadIcon(QStringLiteral("zoom-original")), tr("Original Size"));
        connect(zoomOriginal, &QAction::triggered, this, &SimplePartWidget::zoomOriginal);
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

