/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
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

    static QByteArray generateEncodedWord(const QByteArray& codec, char encoding, const QByteArray& text)
    {
        QByteArray result("=?");
        result.append(codec);
        result.append('?');
        result.append(encoding);
        result.append('?');
        result.append(text);
        result.append("?=");
        return result;
    }

    QByteArray generateEncodedWord(const QByteArray& codec, char encoding, const QList<QByteArray>& list)
    {
        QByteArray result;

        foreach (const QByteArray& item, list)
        {
            if (!result.isEmpty())
                result.append(' ');

            result.append(generateEncodedWord(codec, encoding, item));
        }

        return result;
    }

    QString decodeWord( const QByteArray& encodedWord )
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
                    QByteArray charset = unquoteString(encodedWord.mid(index[0] + 2, (index[1] - index[0] - 2)));
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

    QString decodeWordSequence(const QByteArray& str)
    {
        static const QRegExp whitespace("^\\s+$");

        QString out;

        // Any idea why this isn't matching?
        //QRegExp encodedWord("\\b=\\?\\S+\\?\\S+\\?\\S*\\?=\\b");
        QRegExp encodedWord("=\\?\\S+\\?\\S+\\?\\S*\\?=");

        int pos = 0;
        int lastPos = 0;
        int length = str.length();

        while (pos != -1) {
            pos = encodedWord.indexIn(str, pos);
            if (pos != -1) {
                int endPos = pos + encodedWord.matchedLength();

                if ( ((pos == 0) || (::isspace(str[pos - 1]))) &&
                     ((endPos == length) || (::isspace(str[endPos]))) ) {

                    QString preceding = QString::fromUtf8(str.mid(lastPos, (pos - lastPos)));
                    QString decoded = decodeWord(str.mid(pos, (endPos - pos)));

                    // If there is only whitespace between two encoded words, it should not be included
                    if (!whitespace.exactMatch(preceding))
                        out.append(preceding);

                    out.append(decoded);

                    pos = endPos;
                    lastPos = pos;
                }
                else
                    pos = endPos;
            }
        }

        // Copy anything left
        out.append(QString::fromUtf8(str.mid(lastPos)));

        return out;
    }


    static QList<QByteArray> split(const QByteArray& input, const QByteArray& separator)
    {
        QList<QByteArray> result;

        int index = -1;
        int lastIndex = -1;
        do
        {
            lastIndex = index;
            index = input.indexOf(separator, lastIndex + 1);

            int offset = (lastIndex == -1 ? 0 : lastIndex + separator.length());
            int length = (index == -1 ? -1 : index - offset);
            result.append(input.mid(offset, length));
        } while (index != -1);

        return result;
    }

}

namespace Imap {

QByteArray encodeRFC2047String( const QString& text )
{
    return encodeRFC2047String(text.toUtf8(), "UTF-8");
}

QByteArray encodeRFC2047String( const QByteArray& text, const QByteArray& encoding )
{
    // We can't allow more than 75 chars per encoded-word, including the boiler plate...
    int maximumEncoded = 75 - 7 - encoding.size();

#ifdef ENCODER_USE_QUOTED_PRINTABLE_UNICODE
    QMailQuotedPrintableCodec codec(QMailQuotedPrintableCodec::Text, QMailQuotedPrintableCodec::Rfc2047, maximumEncoded);
    QByteArray encoded = codec.encode(text);
    return generateEncodedWord(encoding, 'Q', split(encoded, "=\n"));
#else
    QMailBase64Codec codec(QMailBase64Codec::Binary, maximumEncoded);
    QByteArray encoded = codec.encode(text);
    return generateEncodedWord(encoding, 'B', split(encoded, "\r\n"));
#endif
}

QString decodeRFC2047String( const QByteArray& raw )
{
    return ::decodeWordSequence( raw );
}

QByteArray encodeImapFolderName( const QString& text )
{
    return KIMAP::encodeImapFolderName( text ).toLatin1();
}

QString decodeImapFolderName( const QByteArray& raw )
{
    return KIMAP::decodeImapFolderName( raw );
}

QByteArray quotedPrintableDecode( const QByteArray& raw )
{
    return KCodecs::quotedPrintableDecode( raw );
}

QByteArray quotedPrintableEncode(const QByteArray &raw)
{
    return KCodecs::quotedPrintableEncode(raw);
}


QByteArray quotedString( const QByteArray& unquoted, QuotedStringStyle style )
{
    QByteArray quoted;
    char lhq, rhq;
    
    /* Compose a double-quoted string according to RFC2822 3.2.5 "quoted-string" */
    switch (style) {
    default:
    case DoubleQuoted:
        lhq = rhq = '"';
        break;
    case SquareBrackets:
        lhq = '[';
        rhq = ']';
        break;
    case Parentheses:
        lhq = '(';
        rhq = ')';
        break;
    }

    quoted.append(lhq);
    for(int i = 0; i < unquoted.size(); i++) {
        char ch = unquoted[i];
        if (ch == 9 || ch == 10 || ch == 13) {
            /* Newlines and tabs: these are only allowed in
               quoted-strings as folding-whitespace, where
               they are "semantically invisible".  If we
               really want to include them, we probably need
               to do so as RFC2047 strings. But it's unlikely
               that that's a desirable behavior in the final
               application. Instead, translate embedded
               tabs/newlines into normal whitespace. */
            quoted.append(' ');
        } else {
            if (ch == lhq || ch == rhq || ch == '\\')
                quoted.append('\\');  /* Quoted-pair */
            quoted.append(ch);
        }
    }
    quoted.append(rhq);

    return quoted;
}

/* encodeRFC2047Phrase encodes an arbitrary string into a
   byte-sequence for use in a "structured" mail header (such as To:,
   From:, or Received:). The result will match the "phrase"
   production. */
static QRegExp atomPhraseRx("[ \\tA-Za-z0-9!#$&'*+/=?^_`{}|~-]*");
QByteArray encodeRFC2047Phrase( const QString &text )
{
    /* We want to know if we can encode as ASCII. But bizarrely, Qt
       (on my system at least) doesn't have an ASCII codec. So we use
       the ISO-8859-1 superset, and check for any non-ASCII characters
       in the result. */
    QTextCodec *latin1 = QTextCodec::codecForMib(4);

    if (latin1->canEncode(text)) {
        /* Attempt to represent it as an RFC2822 'phrase' --- either a
           sequence of atoms or as a quoted-string. */
        
        if (atomPhraseRx.exactMatch(text)) {
            /* Simplest case: a sequence of atoms (not dot-atoms) */
            return latin1->fromUnicode(text);
        } else {
            /* Next-simplest representation: a quoted-string */
            QByteArray unquoted = latin1->fromUnicode(text);
            
            /* Check for non-ASCII characters. */
            for(int i = 0; i < unquoted.size(); i++) {
                char ch = unquoted[i];
                if (ch < 1 || ch >= 127) {
                    /* This string contains non-ASCII characters, so the
                       only way to represent it in a mail header is as an
                       RFC2047 encoded-word. */
                    return encodeRFC2047String(unquoted, "ISO-8859-1");
                }
            }

            return quotedString(unquoted);
        }
    }

    /* If the text has characters outside of the basic ASCII set, then
       it has to be encoded using the RFC2047 encoded-word syntax. */
    return encodeRFC2047String(text);
}

}
