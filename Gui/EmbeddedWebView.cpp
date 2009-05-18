#include "EmbeddedWebView.h"

#include <QWebFrame>
#include <QWebHistory>

namespace Gui {

EmbeddedWebView::EmbeddedWebView( QWidget* parent, QNetworkAccessManager* networkManager, QWebPluginFactory* pluginFactory ):
        QWebView(parent)
{
    page()->setNetworkAccessManager( networkManager );
    page()->setPluginFactory( pluginFactory );

    QWebSettings* s = settings();
    s->setAttribute( QWebSettings::JavascriptEnabled, false );
    s->setAttribute( QWebSettings::JavaEnabled, false );
    s->setAttribute( QWebSettings::PluginsEnabled, true );
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



ImapPartPluginFactory::ImapPartPluginFactory( QObject* parent, QNetworkAccessManager* networkManager ):
        QWebPluginFactory( parent ), _networkManager( networkManager )
{
}

QObject* ImapPartPluginFactory::create( const QString& mimeType, const QUrl& url,
                                        const QStringList& argumentNames,
                                        const QStringList& argumentValues ) const
{
    if ( mimeType != QLatin1String("application/x-trojita-imap-part") )
        return 0;

    EmbeddedWebView* webView = new EmbeddedWebView( 0, _networkManager,
        const_cast<ImapPartPluginFactory*>( this ) ); // FIXME: verify const_cast
    webView->load( url );
    return webView;
}

QList<QWebPluginFactory::Plugin> ImapPartPluginFactory::plugins() const
{
    Plugin res;
    res.name = QLatin1String("Trojita IMAP Part");
    MimeType mime;
    mime.name = QLatin1String("application/x-trojita-imap-part");
    res.mimeTypes = QList<MimeType>() << mime;
    return QList<Plugin>() << res;
}

}

#include "EmbeddedWebView.moc"
