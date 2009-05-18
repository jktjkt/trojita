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
class MsgPartNetAccessManager;
}
}

namespace Gui {

class PartWidgetFactory;

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
private:
    bool eventFilter( QObject* object, QEvent* event );
    QString headerText();

    QWidget* viewer;
    QLabel* header;
    QLayout* layout;
    Imap::Mailbox::TreeItemMessage* message;
    Imap::Mailbox::Model* model;
    Imap::Network::MsgPartNetAccessManager* netAccess;
    QTimer* markAsReadTimer;
    QWebView* emptyView;
    PartWidgetFactory* factory;
};

}

#endif // VIEW_MESSAGEVIEW_H
