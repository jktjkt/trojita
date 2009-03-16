#include <QVBoxLayout>
#include <QtWebKit/QWebView>

#include "MessageView.h"

namespace Gui {

MessageView::MessageView( QWidget* parent ): QWidget(parent)
{
    layout = new QVBoxLayout( this );
    webView = new QWebView( this );
    layout->addWidget( webView );
    layout->setContentsMargins( 0, 0, 0, 0 );
    webView->setUrl( QUrl("http://mozek.cz/info/blesmrt") );
}

}

#include "MessageView.moc"
