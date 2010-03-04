#ifndef SIMPLEPARTWIDGET_H
#define SIMPLEPARTWIDGET_H

#include <QAction>
#include <QFile>

#include "AbstractPartWidget.h"
#include "EmbeddedWebView.h"
#include "Imap/Network/MsgPartNetAccessManager.h"

namespace Gui {

/** @short Widget that handles display of primitive message parts

More complicated parts are handled by other widgets. Role of this one is to
simply render data that can't be broken down to more trivial pieces.
*/

class SimplePartWidget : public EmbeddedWebView, public AbstractPartWidget
{
    Q_OBJECT
public:
    SimplePartWidget( QWidget* parent,
                      Imap::Network::MsgPartNetAccessManager* manager,
                      Imap::Mailbox::TreeItemPart* _part );
    virtual QString quoteMe() const;
private slots:
    void slotSaveContents();
    void slotDataTransfered();
    void slotTransferError();
    void slotDeleteReply(QNetworkReply* reply);
private:
    Imap::Mailbox::TreeItemPart* part;
    QNetworkReply* reply;
    QFile saving;
    QAction* saveAction;
    bool saved;

    SimplePartWidget(const SimplePartWidget&); // don't implement
    SimplePartWidget& operator=(const SimplePartWidget&); // don't implement
};

}

#endif // SIMPLEPARTWIDGET_H
