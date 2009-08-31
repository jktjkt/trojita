#ifndef ATTACHMENTVIEW_H
#define ATTACHMENTVIEW_H

#include <QFile>
#include <QNetworkReply>
#include <QWidget>
#include "Imap/Network/MsgPartNetAccessManager.h"

namespace Gui {

/** @short Widget depicting an attachment

  This widget provides a graphical representation about an e-mail attachment,
  that is, an interactive item which shows some basic information like the MIME
  type of the body part and the download button.  It also includes code for
  handling the actual download.
*/
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
    bool saved;
};

}

#endif // ATTACHMENTVIEW_H
