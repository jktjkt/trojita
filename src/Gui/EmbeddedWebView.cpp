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
#include <QDesktopWidget>
#include <QLayout>
#include <QMouseEvent>
#include <QNetworkReply>
#include <QScrollBar>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>
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

EmbeddedWebView::EmbeddedWebView(QWidget *parent, QNetworkAccessManager *networkManager)
    : QWebView(parent)
    , m_scrollParent(nullptr)
    , m_resizeInProgress(0)
    , m_staticWidth(0)
    , m_colorScheme(ColorScheme::System)
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
    connect(this, &QWebView::linkClicked, this, &EmbeddedWebView::slotLinkClicked);
    connect(this, &QWebView::loadFinished, this, &EmbeddedWebView::handlePageLoadFinished);
    connect(page()->mainFrame(), &QWebFrame::contentsSizeChanged, this, &EmbeddedWebView::handlePageLoadFinished);

    // Scrolling is implemented on upper layers
    page()->mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
    page()->mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);

    // Setup shortcuts for standard actions
    QAction *copyAction = page()->action(QWebPage::Copy);
    copyAction->setShortcut(tr("Ctrl+C"));
    addAction(copyAction);

    m_autoScrollTimer = new QTimer(this);
    m_autoScrollTimer->setInterval(50);
    connect(m_autoScrollTimer, &QTimer::timeout, this, &EmbeddedWebView::autoScroll);

    m_sizeContrainTimer = new QTimer(this);
    m_sizeContrainTimer->setInterval(50);
    m_sizeContrainTimer->setSingleShot(true);
    connect(m_sizeContrainTimer, &QTimer::timeout, this, &EmbeddedWebView::constrainSize);

    setContextMenuPolicy(Qt::NoContextMenu);
    findScrollParent();

    addCustomStylesheet(QString());
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
    if (m_staticWidth) {
        resize(m_staticWidth, QWIDGETSIZE_MAX - 1);
        page()->setViewportSize(QSize(m_staticWidth, 32));
    } else {
        // resize so that the viewport has much vertical and wanted horizontal space
        resize(m_scrollParent->width() - m_scrollParentPadding, QWIDGETSIZE_MAX);
        // resize the PAGES viewport to this width and a minimum height
        page()->setViewportSize(QSize(m_scrollParent->width() - m_scrollParentPadding, 32));
    }
    // now the page has an idea about it's demanded size
    const QSize bestSize = page()->mainFrame()->contentsSize();
    // set the viewport to that size! - Otherwise it'd still be our "suggestion"
    page()->setViewportSize(bestSize);
    // fix the widgets size so the layout doesn't have much choice
    setFixedSize(bestSize);
    m_sizeContrainTimer->stop(); // we caused spurious resize events
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
    if (o == m_scrollParent) {
        if (e->type() == QEvent::Resize) {
            if (!m_staticWidth)
                m_sizeContrainTimer->start();
        } else if (e->type() == QEvent::Enter) {
            m_autoScrollPixels = 0;
            m_autoScrollTimer->stop();
        }
    }
    return QWebView::eventFilter(o, e);
}

void EmbeddedWebView::autoScroll()
{
    if (!(m_scrollParent && m_autoScrollPixels)) {
        m_autoScrollPixels = 0;
        m_autoScrollTimer->stop();
        return;
    }
    if (QScrollBar *bar = static_cast<QAbstractScrollArea*>(m_scrollParent)->verticalScrollBar()) {
        bar->setValue(bar->value() + m_autoScrollPixels);
    }
}

void EmbeddedWebView::mouseMoveEvent(QMouseEvent *e)
{
    if ((e->buttons() & Qt::LeftButton) && m_scrollParent) {
        m_autoScrollPixels = 0;
        const QPoint pos = mapTo(m_scrollParent, e->pos());
        if (pos.y() < 0)
            m_autoScrollPixels = pos.y();
        else if (pos.y() > m_scrollParent->rect().height())
            m_autoScrollPixels = pos.y() - m_scrollParent->rect().height();
        autoScroll();
        m_autoScrollTimer->start();
    }
    QWebView::mouseMoveEvent(e);
}

void EmbeddedWebView::mouseReleaseEvent(QMouseEvent *e)
{
    if (!(e->buttons() & Qt::LeftButton)) {
        m_autoScrollPixels = 0;
        m_autoScrollTimer->stop();
    }
    QWebView::mouseReleaseEvent(e);
}

void EmbeddedWebView::findScrollParent()
{
    if (m_scrollParent)
        m_scrollParent->removeEventFilter(this);
    m_scrollParent = 0;
    const int frameWidth = 2*style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    m_scrollParentPadding = frameWidth;
    QWidget *runner = this;
    int left, top, right, bottom;
    while (runner) {
        runner->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        runner->getContentsMargins(&left, &top, &right, &bottom);
        m_scrollParentPadding += left + right + frameWidth;
        if (runner->layout()) {
            runner->layout()->getContentsMargins(&left, &top, &right, &bottom);
            m_scrollParentPadding += left + right;
        }
        QWidget *p = runner->parentWidget();
        if (p && qobject_cast<MessageView*>(runner) && // is this a MessageView?
            p->objectName() == QLatin1String("qt_scrollarea_viewport") && // in a viewport?
            qobject_cast<QAbstractScrollArea*>(p->parentWidget())) { // that is used?
            p->getContentsMargins(&left, &top, &right, &bottom);
            m_scrollParentPadding += left + right + frameWidth;
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

QWidget *EmbeddedWebView::scrollParent() const
{
    return m_scrollParent;
}

void EmbeddedWebView::setStaticWidth(int staticWidth)
{
    m_staticWidth = staticWidth;
}

int EmbeddedWebView::staticWidth() const
{
    return m_staticWidth;
}

ErrorCheckingPage::ErrorCheckingPage(QObject *parent): QWebPage(parent)
{
}

bool ErrorCheckingPage::supportsExtension(Extension extension) const
{
    if (extension == ErrorPageExtension)
        return true;
    else
        return false;
}

bool ErrorCheckingPage::extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output)
{
    if (extension != ErrorPageExtension)
        return false;

    const ErrorPageExtensionOption *input = static_cast<const ErrorPageExtensionOption *>(option);
    ErrorPageExtensionReturn *res = static_cast<ErrorPageExtensionReturn *>(output);
    if (input && res) {
        if (input->url.scheme() == QLatin1String("trojita-imap")) {
            if (input->domain == QtNetwork && input->error == QNetworkReply::TimeoutError) {
                res->content = tr("<img src=\"%2\"/><span style=\"font-family: sans-serif; color: gray\">"
                                  "Uncached data not available when offline</span>")
                        .arg(Util::resizedImageAsDataUrl(QStringLiteral(":/icons/network-offline.svg"), 32)).toUtf8();
                return true;
            }
        }
        res->content = input->errorString.toUtf8();
        res->contentType = QStringLiteral("text/plain");
    }
    return true;
}

std::map<EmbeddedWebView::ColorScheme, QString> EmbeddedWebView::supportedColorSchemes() const
{
    std::map<EmbeddedWebView::ColorScheme, QString> map;
    map[ColorScheme::System] = tr("System colors");
    map[ColorScheme::AdjustedSystem] = tr("System theme adjusted for better contrast");
    map[ColorScheme::BlackOnWhite] = tr("Black on white, forced");
    return map;
}

void EmbeddedWebView::setColorScheme(const ColorScheme colorScheme)
{
    m_colorScheme = colorScheme;
    addCustomStylesheet(m_customCss);
}

void EmbeddedWebView::addCustomStylesheet(const QString &css)
{
    m_customCss = css;

    QWebSettings *s = settings();
    QString bgName, fgName;
    QColor bg = palette().color(QPalette::Active, QPalette::Base),
           fg = palette().color(QPalette::Active, QPalette::Text);

    switch (m_colorScheme) {
    case ColorScheme::BlackOnWhite:
        bgName = QStringLiteral("white !important");
        fgName = QStringLiteral("black !important");
        break;
    case ColorScheme::AdjustedSystem:
    {
        // This is HTML, and the authors of that markup are free to specify only the background colors, or only the foreground colors.
        // No matter what we pass from outside, there will always be some color which will result in unreadable text, and we can do
        // nothing except adding !important everywhere to fix this.
        // This code attempts to create a color which will try to produce exactly ugly results for both dark-on-bright and
        // bright-on-dark segments of text. However, it's pure alchemy and only a limited heuristics. If you do not like this, please
        // submit patches (or talk to the HTML producers, hehehe).
        const int v = bg.value();
        if (v < 96 && fg.value() > 128 + v/2) {
            int h,s,vv,a;
            fg.getHsv(&h, &s, &vv, &a) ;
            fg.setHsv(h, s, 128+v/2, a);
        }
        bgName = bg.name();
        fgName = fg.name();
        break;
    }
    case ColorScheme::System:
        bgName = bg.name();
        fgName = fg.name();
        break;
    }


    const QString urlPrefix(QStringLiteral("data:text/css;charset=utf-8;base64,"));
    const QString myColors(QStringLiteral("body { background-color: %1; color: %2; }\n").arg(bgName, fgName));
    s->setUserStyleSheetUrl(QString::fromUtf8(urlPrefix.toUtf8() + (myColors + m_customCss).toUtf8().toBase64()));
}

}
