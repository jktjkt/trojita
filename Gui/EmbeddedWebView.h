#ifndef EMBEDDEDWEBVIEW_H
#define EMBEDDEDWEBVIEW_H

#include <QWebView>

namespace Gui {

class EmbeddedWebView: public QWebView {
    Q_OBJECT
public:
    EmbeddedWebView( QWidget* parent, QNetworkAccessManager* networkManager );
protected:
    void contextMenuEvent( QContextMenuEvent* event );
private slots:
    void handlePageLoadFinished( bool ok );
};

}

#endif // EMBEDDEDWEBVIEW_H
