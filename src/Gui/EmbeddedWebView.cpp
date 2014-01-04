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
#include "EmbeddedWebView.h"
#include "MessageView.h"
#include "Gui/Util.h"

#include <QAbstractScrollArea>
#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QLayout>
#include <QNetworkReply>
#include <QStyle>
#include <QStyleFactory>
#include <QWebFrame>
#include <QWebHistory>

#include <QDebug>

namespace {

/** @short RAII pattern for counter manipulation */
class Incrementor {
    int *m_int;
public:
    Incrementor(int *what): m_int(what)
    {
        ++(*m_int);
    }
    ~Incrementor()
    {
        --(*m_int);
        Q_ASSERT(*m_int >= 0);
    }
};

}

namespace Gui
{

EmbeddedWebView::EmbeddedWebView(QWidget *parent, QNetworkAccessManager *networkManager):
    QWebView(parent), m_scrollParent(0L), m_resizeInProgress(0)
{
    // set to expanding, ie. "freely" - this is important so the widget will attempt to shrink below the sizehint!
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus); // not by the wheel
    setPage(new ErrorCheckingPage(this));
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
    connect(this, SIGNAL(loadFinished(bool)), this, SLOT(handlePageLoadFinished()));
    connect(page()->mainFrame(), SIGNAL(contentsSizeChanged(QSize)), this, SLOT(handlePageLoadFinished()));

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
        QStyle *style = QStyleFactory::create(QLatin1String("windows"));
        Q_ASSERT(style);
        palette = style->standardPalette();
        setPalette(palette);
    }

    setContextMenuPolicy(Qt::NoContextMenu);
    findScrollParent();
}

void EmbeddedWebView::constrainSize()
{
    Incrementor dummy(&m_resizeInProgress);

    if (!(m_scrollParent && page() && page()->mainFrame()))
        return; // should not happen but who knows

    // Prevent expensive operation where a resize triggers one extra resizing operation.
    // This is very visible on large attachments, and in fact could possibly lead to recursion as the
    // contentsSizeChanged signal is connected to handlePageLoadFinished.
    if (m_resizeInProgress > 1)
        return;

    // the m_scrollParentPadding measures the summed up horizontal paddings of this view compared to
    // its m_scrollParent
    setMinimumSize(0,0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    // resize so that the viewport has much vertical and wanted horizontal space
    resize(m_scrollParent->width() - m_scrollParentPadding, QWIDGETSIZE_MAX);
    // resize the PAGES viewport to this width and a minimum height
    page()->setViewportSize(QSize(m_scrollParent->width() - m_scrollParentPadding, 32));
    // now the page has an idea about it's demanded size
    const QSize bestSize = page()->mainFrame()->contentsSize();
    // set the viewport to that size! - Otherwise it'd still be our "suggestion"
    page()->setViewportSize(bestSize);
    // fix the widgets size so the layout doesn't have much choice
    setFixedSize(bestSize);
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

void EmbeddedWebView::handlePageLoadFinished()
{
    constrainSize();

    // We've already set in in our constructor, but apparently it isn't enough (Qt 4.8.0 on X11).
    // Let's do it again here, it works.
    Qt::ScrollBarPolicy policy = isWindow() ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
    page()->mainFrame()->setScrollBarPolicy(Qt::Horizontal, policy);
    page()->mainFrame()->setScrollBarPolicy(Qt::Vertical, policy);
    page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
}

void EmbeddedWebView::changeEvent(QEvent *e)
{
    QWebView::changeEvent(e);
    if (e->type() == QEvent::ParentChange)
        findScrollParent();
}

bool EmbeddedWebView::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Resize && o == m_scrollParent)
        constrainSize();
    return QWebView::eventFilter(o, e);
}

void EmbeddedWebView::findScrollParent()
{
    if (m_scrollParent)
        m_scrollParent->removeEventFilter(this);
    m_scrollParent = 0;
    m_scrollParentPadding = 4;
    QWidget *runner = this;
    int left, top, right, bottom;
    while (runner) {
        runner->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        runner->getContentsMargins(&left, &top, &right, &bottom);
        m_scrollParentPadding += left + right + 4;
        if (runner->layout()) {
            runner->layout()->getContentsMargins(&left, &top, &right, &bottom);
            m_scrollParentPadding += left + right;
        }
        QWidget *p = runner->parentWidget();
        if (p && qobject_cast<MessageView*>(runner) && // is this a MessageView?
            p->objectName() == "qt_scrollarea_viewport" && // in a viewport?
            qobject_cast<QAbstractScrollArea*>(p->parentWidget())) { // that is used?
            p->getContentsMargins(&left, &top, &right, &bottom);
            m_scrollParentPadding += left + right + 4;
            if (p->layout()) {
                p->layout()->getContentsMargins(&left, &top, &right, &bottom);
                m_scrollParentPadding += left + right;
            }
            m_scrollParent = p->parentWidget();
            break; // then we have our actual message view
        }
        runner = p;
    }
    m_scrollParentPadding += style()->pixelMetric(QStyle::PM_ScrollBarExtent, 0, m_scrollParent);
    if (m_scrollParent)
        m_scrollParent->installEventFilter(this);
}

void EmbeddedWebView::showEvent(QShowEvent *se)
{
    QWebView::showEvent(se);
    Qt::ScrollBarPolicy policy = isWindow() ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
    page()->mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAsNeeded);
    page()->mainFrame()->setScrollBarPolicy(Qt::Vertical, policy);
    if (isWindow()) {
        resize(640,480);
    } else if (!m_scrollParent) // it would be much easier if the parents were just passed with the constructor ;-)
        findScrollParent();
}

QSize EmbeddedWebView::sizeHint() const
{
    return QSize(32,32); // QWebView returns 800x600 what will lead to too wide pages for our implementation
}

ErrorCheckingPage::ErrorCheckingPage(QObject *parent): QWebPage(parent)
{
}

bool ErrorCheckingPage::supportsExtension(Extension extension) const
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 8, 0)
    if (extension == ErrorPageExtension)
        return true;
    else
#endif
        return false;
}

bool ErrorCheckingPage::extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output)
{
#if QT_VERSION < QT_VERSION_CHECK(4, 8, 0)
    return false;
#else
    if (extension != ErrorPageExtension)
        return false;

    const ErrorPageExtensionOption *input = static_cast<const ErrorPageExtensionOption *>(option);
    ErrorPageExtensionReturn *res = static_cast<ErrorPageExtensionReturn *>(output);
    if (input && res) {
        if (input->url.scheme() == QLatin1String("trojita-imap")) {
            if (input->domain == QtNetwork && input->error == QNetworkReply::TimeoutError) {
                res->content = tr("<img src='%2'/><span style='font-family: sans-serif; color: gray'>"
                                  "Uncached data not available when offline</span>")
                        .arg(Util::resizedImageAsDataUrl(QLatin1String(":/icons/network-offline.png"), 32)).toUtf8();
                return true;
            }
        }
        res->content = input->errorString.toUtf8();
        res->contentType = QLatin1String("text/plain");
    }
    return true;
#endif
}

}
