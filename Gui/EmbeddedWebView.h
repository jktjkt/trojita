#ifndef EMBEDDEDWEBVIEW_H
#define EMBEDDEDWEBVIEW_H

#include <QWebPluginFactory>
#include <QWebView>

namespace Gui {


/** @short An embeddable QWebView which makes itself big enough to display all of the contents

  This class configures the QWebView in such a way that it will resize itself to
  show all required contents.  Note that one still has to provide a proper
  eventFilter in the parent widget (and register it for use).

  @see Gui::MessageView

  */
class EmbeddedWebView: public QWebView {
    Q_OBJECT
public:
    EmbeddedWebView( QWidget* parent, QNetworkAccessManager* networkManager );
private slots:
    void handlePageLoadFinished( bool ok );
};

}

#endif // EMBEDDEDWEBVIEW_H
