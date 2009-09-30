#include "Encoders.h"
#include "Parser/3rdparty/qmailcodec.h"
#include "Parser/3rdparty/rfccodecs.h"
#include "Parser/3rdparty/kcodecs.h"

namespace {
    template<typename StringType>
    StringType unquoteString(const StringType& src)
    {
        // If a string has double-quote as the first and last characters, return the string
        // between those characters
        int length = src.length();
        if (length)
        {
            typename StringType::const_iterator const begin = src.constData();
            typename StringType::const_iterator const last = begin + length - 1;

            if ((last > begin) && (*begin == '"' && *last == '"'))
                return src.mid(1, length - 2);
        }

        return src;
    }
}

namespace Imap {

QByteArray encodeRFC2047String( const QString& text )
{
    QMailQuotedPrintableCodec codec( QMailQuotedPrintableCodec::Text, QMailQuotedPrintableCodec::Rfc2047 );
    return codec.encode( text, QLatin1String("utf-8") );
}

QString decodeRFC2047String( const QByteArray& encodedWord )
{
    QString result;
    int index[4];

    // Find the parts of the input
    index[0] = encodedWord.indexOf("=?");
    if (index[0] != -1)
    {
        index[1] = encodedWord.indexOf('?', index[0] + 2);
        if (index[1] != -1)
        {
            index[2] = encodedWord.indexOf('?', index[1] + 1);
            index[3] = encodedWord.lastIndexOf("?=");
            if ((index[2] != -1) && (index[3] > index[2]))
            {
                QByteArray charset = ::unquoteString(encodedWord.mid(index[0] + 2, (index[1] - index[0] - 2)));
                QByteArray encoding = encodedWord.mid(index[1] + 1, (index[2] - index[1] - 1)).toUpper();
                QByteArray encoded = encodedWord.mid(index[2] + 1, (index[3] - index[2] - 1));

                if (encoding == "Q")
                {
                    QMailQuotedPrintableCodec codec(QMailQuotedPrintableCodec::Text, QMailQuotedPrintableCodec::Rfc2047);
                    result = codec.decode(encoded, charset);
                }
                else if (encoding == "B")
                {
                    QMailBase64Codec codec(QMailBase64Codec::Binary);
                    result = codec.decode(encoded, charset);
                }
            }
        }
    }

    if (result.isEmpty())
        result = encodedWord;

    return result;
}

QByteArray encodeImapFolderName( const QString& text )
{
    return KIMAP::encodeImapFolderName( text ).toAscii();
}

QString decodeImapFolderName( const QByteArray& raw )
{
    return KIMAP::decodeImapFolderName( raw );
}

QByteArray quotedPrintableDecode( const QByteArray& raw )
{
    return KCodecs::quotedPrintableDecode( raw );
}

}
