#include "EmbeddedWebView.h"

#include <QAction>
#include <QDesktopServices>
#include <QWebFrame>
#include <QWebHistory>

#include <QDebug>

namespace Gui {

EmbeddedWebView::EmbeddedWebView( QWidget* parent, QNetworkAccessManager* networkManager ):
        QWebView(parent)
{
    page()->setNetworkAccessManager( networkManager );

    QWebSettings* s = settings();
    s->setAttribute( QWebSettings::JavascriptEnabled, false );
    s->setAttribute( QWebSettings::JavaEnabled, false );
    s->setAttribute( QWebSettings::PluginsEnabled, false );
    s->setAttribute( QWebSettings::PrivateBrowsingEnabled, true );
    s->setAttribute( QWebSettings::JavaEnabled, false );
    s->setAttribute( QWebSettings::OfflineStorageDatabaseEnabled, false );
    s->setAttribute( QWebSettings::OfflineWebApplicationCacheEnabled, false );
    s->setAttribute( QWebSettings::LocalStorageDatabaseEnabled, false );
    s->clearMemoryCaches();

    page()->setLinkDelegationPolicy( QWebPage::DelegateAllLinks );
    connect(this, SIGNAL(linkClicked(QUrl)), this, SLOT(slotLinkClicked(QUrl)));

    // Scrolling is implemented on upper layers
    page()->mainFrame()->setScrollBarPolicy( Qt::Horizontal, Qt::ScrollBarAlwaysOff );
    page()->mainFrame()->setScrollBarPolicy( Qt::Vertical, Qt::ScrollBarAlwaysOff );

    // Setup shortcuts for standard actions
    QAction* copyAction = page()->action( QWebPage::Copy );
    copyAction->setShortcut( tr("Ctrl+C") );
    addAction( copyAction );

    setContextMenuPolicy( Qt::NoContextMenu );
}

void EmbeddedWebView::slotLinkClicked(const QUrl &url)
{
    // Only allow external http:// links for safety reasons
    if ( url.scheme().toLower() == QLatin1String("http") || url.scheme().toLower() == QLatin1String("mailto") ) {
        QDesktopServices::openUrl( url );
    }
}

}
