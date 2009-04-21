#include <QNetworkRequest>
#include <QDebug>

#include "AuxiliaryReply.h"
#include "ForbiddenReply.h"
#include "FormattingNetAccessManager.h"
#include "FormattingReply.h"
#include "MsgPartNetAccessManager.h"
#include "MsgPartNetworkReply.h"
#include "MultipartAlternativeReply.h"
#include "MultipartSignedReply.h"
#include "MultipartMixedReply.h"
#include "Imap/Model/MailboxTree.h"

namespace Imap {
namespace Network {

FormattingNetAccessManager::FormattingNetAccessManager( QObject* parent ):
    QNetworkAccessManager( parent ), partManager( new MsgPartNetAccessManager( this ) )
{
}

void FormattingNetAccessManager::setModelMessage( Imap::Mailbox::Model* _model,
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
        if ( part ) {
            if ( part->mimeType().startsWith( QLatin1String( "multipart/" ) ) ) {
                if ( part->mimeType() == QLatin1String( "multipart/signed" ) ) {
                    return new Imap::Network::MultipartSignedReply( this, partManager->model, partManager->message, part );
                } else if ( part->mimeType() == QLatin1String( "multipart/mixed" ) ) {
                    return new Imap::Network::MultipartMixedReply( this, partManager->model, partManager->message, part );
                } else if ( part->mimeType() == QLatin1String( "multipart/alternative" ) ) {
                    return new Imap::Network::MultipartAlternativeReply( this, partManager->model, partManager->message, part );
                } else {
                    return new Imap::Network::AuxiliaryReply( this,
                        tr("Message type %1 is not supported").arg( part->mimeType() ) );
                }
            } else {
                // text/* or anything generic
                return new Imap::Network::FormattingReply( this, partManager->model, partManager->message, part );
            }
        } else {
            qDebug() << "Forbidden per policy:" << req.url();
            return new Imap::Network::ForbiddenReply( this );
        }
    } else if ( req.url().scheme() == QLatin1String( "trojita-imap" ) &&
                req.url().host() == QLatin1String( "part" ) ) {
        if ( part )
            return new Imap::Network::MsgPartNetworkReply( this, partManager->model, partManager->message,
                                                           part );
        else
            return new Imap::Network::AuxiliaryReply( this, tr( "Can't find message part %1" ).arg( req.url().toString() ) );
    } else if ( req.url().scheme() == QLatin1String("data") ) {
        // The whole point of this Network Access Manager is to filter out remote connections
        // while allowing our special internal ones. It's therefore safe to allow
        // the data: URL scheme.
        return QNetworkAccessManager::createRequest( op, req, outgoingData );
    } else {
        qDebug() << "Forbidden per policy:" << req.url();
        return new Imap::Network::ForbiddenReply( this );
    }
}

}
}

#include "FormattingNetAccessManager.moc"
