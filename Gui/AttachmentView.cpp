#include "AttachmentView.h"
#include "Imap/Model/MailboxTree.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>

namespace Gui {

AttachmentView::AttachmentView( QWidget* parent,
                                Imap::Network::MsgPartNetAccessManager* _manager,
                                Imap::Mailbox::TreeItemPart* _part):
    QWidget( parent ), manager(_manager), part(_part), reply(0)
{
    QHBoxLayout* layout = new QHBoxLayout( this );
    QLabel* lbl = new QLabel( tr("Attachment %1 (%2)").arg( part->fileName(), part->mimeType() ) );
    layout->addWidget( lbl );
    QPushButton* download = new QPushButton( tr("Download") );
    download->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Maximum );
    layout->addWidget( download );
    connect( download, SIGNAL(clicked()), this, SLOT(slotDownloadClicked()) );
}

void AttachmentView::slotDownloadClicked()
{
    QString saveFileName = QFileDialog::getSaveFileName( this, tr("Save Attachment"),
                                                         part->fileName(), QString(),
                                                         0, QFileDialog::HideNameFilterDetails
                                                       );
    if ( saveFileName.isEmpty() )
        return;

    saving.setFileName( saveFileName );

    QNetworkRequest request;
    QUrl url;
    url.setScheme( QLatin1String("trojita-imap") );
    url.setHost( QLatin1String("msg") );
    url.setPath( part->pathToPart() );
    request.setUrl( url );
    reply = manager->get( request );
    connect( reply, SIGNAL(finished()), this, SLOT(slotDataTransfered()) );
    connect( reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(slotTransferError()) );
    connect( manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(slotDeleteReply(QNetworkReply*)) );
    saved = false;
}

void AttachmentView::slotDataTransfered()
{
    Q_ASSERT( reply );
    if ( reply->error() == QNetworkReply::NoError ) {
        saving.open( QIODevice::WriteOnly );
        saving.write( reply->readAll() );
        saving.close();
        saved = true;
    }
}

void AttachmentView::slotTransferError()
{
    Q_ASSERT( reply );
    QMessageBox::critical( this, tr("Can't save attachment"),
                           tr("Unable to save the attachment. Error:\n%1").arg( reply->errorString() ) );
}

void AttachmentView::slotDeleteReply(QNetworkReply* reply)
{
    if ( reply == this->reply ) {
        if ( ! saved )
            slotDataTransfered();
        delete reply;
        this->reply = 0;
    }
}

}

#include "AttachmentView.moc"
