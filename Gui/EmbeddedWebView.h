#ifndef EMBEDDEDWEBVIEW_H
#define EMBEDDEDWEBVIEW_H

#include <QWebPluginFactory>
#include <QWebView>

namespace Gui {


/** @short An embeddable QWebView with some safety checks

  This class configures the QWebView in such a way that it will prevent certain
  dangerous (or unexpected, in the context of a MUA) features from being invoked.

  Note that you still have to provide a proper eventFilter in the parent widget
  (and register it for use).

  @see Gui::MessageView

  */
class EmbeddedWebView: public QWebView {
    Q_OBJECT
public:
    EmbeddedWebView( QWidget* parent, QNetworkAccessManager* networkManager );
};

}

#endif // EMBEDDEDWEBVIEW_H
