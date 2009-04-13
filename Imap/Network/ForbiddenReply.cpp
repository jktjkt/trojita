#include <QTimer>

#include "ForbiddenReply.h"

namespace Imap {
namespace Network {

ForbiddenReply::ForbiddenReply( QObject* parent):
    QNetworkReply( parent )
{
    setError( QNetworkReply::ContentOperationNotPermittedError,
        tr("Remote Content Is Banned"));
    QTimer::singleShot( 0, this, SLOT( slotFinish() ) );
}

void ForbiddenReply::slotFinish()
{
    emit error( QNetworkReply::ContentOperationNotPermittedError );
    emit finished();
}

}
}
#include "ForbiddenReply.moc"
