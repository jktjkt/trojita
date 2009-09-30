#ifndef IMAP_ENCODERS_H
#define IMAP_ENCODERS_H

#include <QString>

namespace Imap {

QByteArray encodeRFC2047String( const QString& text );

QString decodeRFC2047String( const QByteArray& raw );

QByteArray encodeImapFolderName( const QString& text );

QString decodeImapFolderName( const QByteArray& raw );

QByteArray quotedPrintableDecode( const QByteArray& raw );

}

#endif // IMAP_ENCODERS_H
