#include <QTimer>

#include "AuxiliaryReply.h"

namespace Imap {
namespace Network {

AuxiliaryReply::AuxiliaryReply( QObject* parent, const QString& textualMessage ):
    QNetworkReply( parent ), message( textualMessage.toUtf8() )
{
    buffer.setData( message );
    QTimer::singleShot( 0, this, SLOT( slotFinish() ) );
    setOpenMode( QIODevice::ReadOnly | QIODevice::Unbuffered );
    buffer.open( QIODevice::ReadOnly );
    setHeader( QNetworkRequest::ContentTypeHeader, QLatin1String( "text/plain" ) );
}

void AuxiliaryReply::slotFinish()
{
    emit readyRead();
    emit finished();
}

void AuxiliaryReply::abort()
{
    close();
}

void AuxiliaryReply::close()
{
    buffer.close();
}

qint64 AuxiliaryReply::bytesAvailable() const
{
    return buffer.bytesAvailable() + QNetworkReply::bytesAvailable();
}

qint64 AuxiliaryReply::readData( char* data, qint64 maxSize )
{
    return buffer.read( data, maxSize );
}


}
}
#include "AuxiliaryReply.moc"
