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
class MsgPartNetAccessManager;
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
    const Imap::Mailbox::Model* model;
    Imap::Network::MsgPartNetAccessManager* netAccess;
};

}

#endif // VIEW_MESSAGEVIEW_H
