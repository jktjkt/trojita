#ifndef VIEW_MESSAGEVIEW_H
#define VIEW_MESSAGEVIEW_H

#include <QWidget>

class QWebView;
class QLayout;

namespace Gui {

class MessageView : public QWidget
{
Q_OBJECT
public:
    MessageView( QWidget* parent=0);
private:
    QWebView* webView;
    QLayout* layout;
};

}

#endif // VIEW_MESSAGEVIEW_H
