#ifndef VIEW_MESSAGEVIEW_H
#define VIEW_MESSAGEVIEW_H

#include <QWidget>

class QLabel;
class QLayout;
class QModelIndex;
class QTimer;
class QWebView;

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
    void setEmpty();
    void handleMessageRemoved( void* msg );
private slots:
    void markAsRead();
    void pageLoadFinished();
private:
    bool eventFilter( QObject* object, QEvent* event );

    QWebView* webView;
    QLabel* header;
    QLayout* layout;
    Imap::Mailbox::TreeItemMessage* message;
    Imap::Mailbox::Model* model;
    Imap::Network::FormattingNetAccessManager* netAccess;
    QTimer* markAsReadTimer;
};

}

#endif // VIEW_MESSAGEVIEW_H
