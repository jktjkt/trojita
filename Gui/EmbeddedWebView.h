#ifndef EMBEDDEDWEBVIEW_H
#define EMBEDDEDWEBVIEW_H

#include <QWebPluginFactory>
#include <QWebView>

namespace Gui {

class EmbeddedWebView: public QWebView {
    Q_OBJECT
public:
    EmbeddedWebView( QWidget* parent, QNetworkAccessManager* networkManager,
                     QWebPluginFactory* pluginFactory );
protected:
    void contextMenuEvent( QContextMenuEvent* event );
private slots:
    void handlePageLoadFinished( bool ok );
};

class ImapPartPluginFactory: public QWebPluginFactory {
    Q_OBJECT
public:
    ImapPartPluginFactory( QObject* parent, QNetworkAccessManager* networkManager );
    virtual QObject* create( const QString& mimeType, const QUrl& url,
                             const QStringList& argumentNames,
                             const QStringList& argumentValues ) const;
    virtual QList<Plugin> plugins() const;
private:
    QNetworkAccessManager* _networkManager;
};

}

#endif // EMBEDDEDWEBVIEW_H
