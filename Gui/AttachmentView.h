#ifndef ATTACHMENTVIEW_H
#define ATTACHMENTVIEW_H

#include <QFile>
#include <QNetworkReply>
#include <QWidget>
#include "Imap/Network/MsgPartNetAccessManager.h"

namespace Gui {

class AttachmentView : public QWidget
{
    Q_OBJECT
public:
    AttachmentView( QWidget* parent,
                    Imap::Network::MsgPartNetAccessManager* _manager,
                    Imap::Mailbox::TreeItemPart* _part );
private slots:
    void slotDownloadClicked();
    void slotDataTransfered();
    void slotTransferError();
    void slotDeleteReply(QNetworkReply* reply);
private:
    Imap::Network::MsgPartNetAccessManager* manager;
    Imap::Mailbox::TreeItemPart* part;
    QNetworkReply* reply;
    QFile saving;
};

}

#endif // ATTACHMENTVIEW_H
