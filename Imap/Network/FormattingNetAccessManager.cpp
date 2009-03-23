#include <QNetworkRequest>
#include <QDebug>

#include "ForbiddenReply.h"
#include "FormattingNetAccessManager.h"
#include "FormattingReply.h"
#include "MsgPartNetAccessManager.h"
#include "MsgPartNetworkReply.h"

namespace Imap {
namespace Network {

FormattingNetAccessManager::FormattingNetAccessManager( QObject* parent ):
    QNetworkAccessManager( parent ), partManager( new MsgPartNetAccessManager( this ) )
{
}

void FormattingNetAccessManager::setModelMessage( const Imap::Mailbox::Model* _model,
    Imap::Mailbox::TreeItemMessage* _message )
{
    partManager->setModelMessage( _model, _message );
}

QNetworkReply* FormattingNetAccessManager::createRequest( Operation op,
    const QNetworkRequest& req, QIODevice* outgoingData )
{
    // FIXME: wait for envelope to be fetched (iow, if not fetched yet, return
    // some stub and somehow setup signals for updating the view

    Imap::Mailbox::TreeItemPart* part = partManager->pathToPart( req.url().path() );
    if ( req.url().scheme() == QLatin1String( "trojita-imap" ) &&
         req.url().host() == QLatin1String( "msg" ) ) {
        return new Imap::Network::FormattingReply( this, partManager->model, partManager->message, part );
    } else if ( req.url().scheme() == QLatin1String( "trojita-imap" ) &&
                req.url().host() == QLatin1String( "part" ) ) {
        return new Imap::Network::MsgPartNetworkReply( this, partManager->model, partManager->message,
                                                       part );
    } else {
        qDebug() << "Forbidden per policy:" << req.url();
        return new Imap::Network::ForbiddenReply( this );
    }
}

}
}

#include "FormattingNetAccessManager.moc"
