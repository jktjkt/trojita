/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#include "EmbeddedWebView.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QWebFrame>
#include <QWebHistory>
#include <QWindowsStyle>

#include <QDebug>

namespace Gui
{

EmbeddedWebView::EmbeddedWebView(QWidget *parent, QNetworkAccessManager *networkManager):
    QWebView(parent)
{
    page()->setNetworkAccessManager(networkManager);

    QWebSettings *s = settings();
    s->setAttribute(QWebSettings::JavascriptEnabled, false);
    s->setAttribute(QWebSettings::JavaEnabled, false);
    s->setAttribute(QWebSettings::PluginsEnabled, false);
    s->setAttribute(QWebSettings::PrivateBrowsingEnabled, true);
    s->setAttribute(QWebSettings::JavaEnabled, false);
    s->setAttribute(QWebSettings::OfflineStorageDatabaseEnabled, false);
    s->setAttribute(QWebSettings::OfflineWebApplicationCacheEnabled, false);
    s->setAttribute(QWebSettings::LocalStorageDatabaseEnabled, false);
    s->clearMemoryCaches();

    page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    connect(this, SIGNAL(linkClicked(QUrl)), this, SLOT(slotLinkClicked(QUrl)));
    connect(this, SIGNAL(loadFinished(bool)), this, SLOT(handlePageLoadFinished(bool)));

    // Scrolling is implemented on upper layers
    page()->mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
    page()->mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);

    // Setup shortcuts for standard actions
    QAction *copyAction = page()->action(QWebPage::Copy);
    copyAction->setShortcut(tr("Ctrl+C"));
    addAction(copyAction);

    // Redmine#3, the QWebView uses black text color when rendering stuff on dark background
    QPalette palette = QApplication::palette();
    if (palette.background().color().lightness() < 50) {
        qDebug() << "Setting the palette tweak";
        QWindowsStyle s;
        palette = s.standardPalette();
        setPalette(palette);
    }

    setContextMenuPolicy(Qt::NoContextMenu);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

void EmbeddedWebView::slotLinkClicked(const QUrl &url)
{
    // Only allow external http:// and https:// links for safety reasons
    if (url.scheme().toLower() == QLatin1String("http") || url.scheme().toLower() == QLatin1String("https")) {
        QDesktopServices::openUrl(url);
    } else if (url.scheme().toLower() == QLatin1String("mailto")) {
        // The mailto: scheme is registered by Gui::MainWindow and handled internally;
        // even if it wasn't, opening a third-party application in response to a
        // user-initiated click does not pose a security risk
        QUrl betterUrl(url);
        betterUrl.setScheme(url.scheme().toLower());
        QDesktopServices::openUrl(betterUrl);
    }
}

void EmbeddedWebView::handlePageLoadFinished(bool ok)
{
    Q_UNUSED(ok);
    setMinimumSize(page()->mainFrame()->contentsSize());

    // We've already set in in our constructor, but apparently it isn't enough (Qt 4.8.0 on X11).
    // Let's do it again here, it works.
    page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
}

}
