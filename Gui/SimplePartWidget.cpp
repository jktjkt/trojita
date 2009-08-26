#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkReply>

#include "SimplePartWidget.h"
#include "Imap/Model/MailboxTree.h"

namespace Gui {

SimplePartWidget::SimplePartWidget( QWidget* parent,
                                    Imap::Network::MsgPartNetAccessManager* manager,
                                    Imap::Mailbox::TreeItemPart* _part ):
        EmbeddedWebView( parent, manager ), part(_part), reply(0)
{
    QUrl url;
    url.setScheme( QLatin1String("trojita-imap") );
    url.setHost( QLatin1String("msg") );
    url.setPath( part->pathToPart() );
    load( url );

    saveAction = new QAction( tr("Save..."), this );
    connect( saveAction, SIGNAL(triggered()), this, SLOT(slotSaveContents()) );
    this->addAction( saveAction );

    connect( page()->networkAccessManager(), SIGNAL(finished(QNetworkReply*)),
             this, SLOT(slotDeleteReply(QNetworkReply*)) );

    setContextMenuPolicy( Qt::ActionsContextMenu );
}

void SimplePartWidget::slotSaveContents()
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
    reply = page()->networkAccessManager()->get( request );
    connect( reply, SIGNAL(finished()), this, SLOT(slotDataTransfered()) );
    connect( reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(slotTransferError()) );
    saved = false;
}

void SimplePartWidget::slotDataTransfered()
{
    Q_ASSERT( reply );
    if ( reply->error() == QNetworkReply::NoError ) {
        saving.open( QIODevice::WriteOnly );
        saving.write( reply->readAll() );
        saving.close();
        saved = true;
    }
}

void SimplePartWidget::slotTransferError()
{
    Q_ASSERT( reply );
    QMessageBox::critical( this, tr("Can't save attachment"),
                           tr("Unable to save the attachment. Error:\n%1").arg( reply->errorString() ) );
}

void SimplePartWidget::slotDeleteReply(QNetworkReply* reply)
{
    if ( reply == this->reply ) {
        if ( ! saved ) {
            slotDataTransfered();
        }
        delete reply;
        this->reply = 0;
    }
}


}

#include "SimplePartWidget.moc"
