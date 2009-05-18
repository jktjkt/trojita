#include "EmbeddedWebView.h"

#include <QWebFrame>
#include <QWebHistory>

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

    // Scrolling is implemented on upper layers
    page()->mainFrame()->setScrollBarPolicy( Qt::Horizontal, Qt::ScrollBarAlwaysOff );
    page()->mainFrame()->setScrollBarPolicy( Qt::Vertical, Qt::ScrollBarAlwaysOff );

    connect( this, SIGNAL(loadFinished(bool)), this, SLOT(handlePageLoadFinished(bool)) );
}

void EmbeddedWebView::contextMenuEvent( QContextMenuEvent* event )
{
}

void EmbeddedWebView::handlePageLoadFinished( bool ok )
{
    Q_UNUSED( ok );
    setMinimumSize( page()->mainFrame()->contentsSize() );
}


}

#include "EmbeddedWebView.moc"
