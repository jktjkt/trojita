#ifndef VIEW_MESSAGEVIEW_H
#define VIEW_MESSAGEVIEW_H

#include <QWidget>

class QWebView;
class QLayout;
class QModelIndex;

namespace Imap {
namespace Mailbox {
class TreeItemMessage;
class Model;
}
namespace Network {
class FormattingNetAccessManager;
}
}

namespace Gui {

class MessageView : public QWidget
{
Q_OBJECT
public:
    MessageView( QWidget* parent=0);
public slots:
    void setMessage( const QModelIndex& index );
private:
    QWebView* webView;
    QLayout* layout;
    Imap::Mailbox::TreeItemMessage* message;
    Imap::Mailbox::Model* model;
    Imap::Network::FormattingNetAccessManager* netAccess;
};

}

#endif // VIEW_MESSAGEVIEW_H
