#include "Encoders.h"
#include "Parser/3rdparty/qmailcodec.h"
#include "Parser/3rdparty/rfccodecs.h"

namespace Imap {

QByteArray encodeRFC2047String( const QString& text )
{
    QMailQuotedPrintableCodec codec( QMailQuotedPrintableCodec::Text, QMailQuotedPrintableCodec::Rfc2047 );
    return codec.encode( text, QLatin1String("utf-8") );
}

QString decodeRFC2047String( const QByteArray& raw )
{
    QMailQuotedPrintableCodec codec( QMailQuotedPrintableCodec::Binary, QMailQuotedPrintableCodec::Rfc2047 );
    return codec.decode( raw, QLatin1String("utf-8") );
}

QByteArray encodeImapFolderName( const QString& text )
{
    return KIMAP::encodeImapFolderName( text ).toAscii();
}

QString decodeImapFolderName( const QByteArray& raw )
{
    return KIMAP::decodeImapFolderName( raw );
}

QString quotedPrintableDecode( const QByteArray& raw );

}
