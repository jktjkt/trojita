#ifndef VIEW_MESSAGEVIEW_H
#define VIEW_MESSAGEVIEW_H

#include <QWidget>

class QLabel;
class QLayout;
class QModelIndex;
class QTimer;
class QUrl;
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

class MainWindow;
class PartWidgetFactory;
class ExternalElementsWidget;


/** @short Widget for displaying complete e-mail messages as available from the IMAP server

  Widget which can render a regular message as exported by the Imap::Mailbox::Model model.
  Notably, this class will not render message/rfc822 MIME parts.
*/
class MessageView : public QWidget
{
Q_OBJECT
public:
    MessageView( QWidget* parent=0);
    ~MessageView();
    enum ReplyMode { REPLY_SENDER_ONLY, /**< @short Reply to sender(s) only */
                     REPLY_ALL /**< @short Reply to all recipients */ };
    void reply( MainWindow* mainWindow, ReplyMode mode );
public slots:
    void setMessage( const QModelIndex& index );
    void setEmpty();
    void handleMessageRemoved( void* msg );
private slots:
    void markAsRead();
    void externalsRequested( const QUrl& url );
    void externalsEnabled();
signals:
    void messageChanged();
private:
    bool eventFilter( QObject* object, QEvent* event );
    QString headerText();
    QString quoteText() const;
    static QString replySubject( const QString& subject );

    QWidget* viewer;
    QLabel* header;
    ExternalElementsWidget* externalElements;
    QLayout* layout;
    Imap::Mailbox::TreeItemMessage* message;
    Imap::Mailbox::Model* model;
    Imap::Network::MsgPartNetAccessManager* netAccess;
    QTimer* markAsReadTimer;
    QWebView* emptyView;
    PartWidgetFactory* factory;

    MessageView(const MessageView&); // don't implement
    MessageView& operator=(const MessageView&); // don't implement
};

}

#endif // VIEW_MESSAGEVIEW_H
