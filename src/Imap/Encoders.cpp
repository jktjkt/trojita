/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "Encoders.h"
#include "Parser/3rdparty/rfccodecs.h"
#include "Parser/3rdparty/kcodecs.h"

namespace {

    static QTextCodec* codecForName(const QByteArray& charset, bool translateAscii = true)
    {
        QByteArray encoding(charset.toLower());

        if (!encoding.isEmpty()) {
            int index;

            if (translateAscii && encoding.contains("ascii")) {
                // We'll assume the text is plain ASCII, to be extracted to Latin-1
                encoding = "ISO-8859-1";
            }
            else if ((index = encoding.indexOf('*')) != -1) {
                // This charset specification includes a trailing language specifier
                encoding = encoding.left(index);
            }

            QTextCodec* codec = QTextCodec::codecForName(encoding);
            if (!codec) {
                qWarning() << "codecForName: Unable to find codec for charset" << encoding;
            }

            return codec;
        }

        return 0;
    }

    // ASCII character values used throughout
    const unsigned char MaxPrintableRange = 0x7e;
    const unsigned char Space = 0x20;
    const unsigned char Equals = 0x3d;
    const unsigned char QuestionMark = 0x3f;
    const unsigned char Underscore = 0x5f;

    /** @short Check the given unicode code point if it has to be escaped in the quoted-printable encoding according to RFC2047 */
    static inline bool rfc2047QPNeedsEscpaing(const int unicode)
    {
        if (unicode <= Space)
            return true;
        if (unicode == Equals || unicode == QuestionMark || unicode == Underscore)
            return true;
        if (unicode > MaxPrintableRange)
            return true;
        return false;
    }

    /** @short Check the given unicode code point if it has to be escaped according to RFC 2231

    The rules might be a little stricter than required, actually.
    */
    static inline bool rfc2311NeedsEscaping(const int unicode)
    {
        const unsigned char Ascii_Zero = 0x30;
        const unsigned char Ascii_Nine = 0x39;
        const unsigned char Ascii_A = 0x41;
        const unsigned char Ascii_Z = 0x5a;
        const unsigned char Ascii_a = 0x61;
        const unsigned char Ascii_z = 0x7a;
        const unsigned char Ascii_Minus = 0x2d;
        const unsigned char Ascii_Dot = 0x2e;
        const unsigned char Ascii_Underscore = 0x5f;

        if (unicode == Ascii_Minus || unicode == Ascii_Dot || unicode == Ascii_Underscore)
            return false;
        if (unicode >= Ascii_Zero && unicode <= Ascii_Nine)
            return false;
        if (unicode >= Ascii_A && unicode <= Ascii_Z)
            return false;
        if (unicode >= Ascii_a && unicode <= Ascii_z)
            return false;
        return true;
    }

    /** @short Find the most efficient encoding for the given unicode string

    It can be either just plain ASCII, or ISO-Latin1 using the Quoted-Printable encoding, or
    a full-blown UTF-8 scheme with Base64 encoding.
    */
    static Imap::Rfc2047StringCharacterSetType charsetForInput(const QString& input)
    {
        // shamelessly stolen from QMF's qmailmessage.cpp

        // See if this input needs encoding
        Imap::Rfc2047StringCharacterSetType latin1 = Imap::RFC2047_STRING_ASCII;

        const QChar* it = input.constData();
        const QChar* const end = it + input.length();
        for ( ; it != end; ++it) {
            if ((*it).unicode() > 0xff) {
                // Multi-byte characters included - we need to use UTF-8
                return Imap::RFC2047_STRING_UTF8;
            }
            else if (!latin1 && rfc2047QPNeedsEscpaing(it->unicode()))
            {
                // We need encoding from latin-1
                latin1 = Imap::RFC2047_STRING_LATIN;
            }
        }

        return latin1;
    }

    /** @short Convert a hex digit into a number */
    static inline int hexValueOfChar(const char input)
    {
        if (input >= '0' && input <= '9') {
            return input - '0';
        } else if (input >= 'A' && input <= 'F') {
            return 0x0a + input - 'A';
        } else if (input >= 'a' && input <= 'f') {
            return 0x0a + input - 'a';
        } else {
            return -1;
        }
    }

    /** @short Translate a quoted-printable-encoded array of bytes into binary characters

    The transformations performed are according to RFC 2047; underscores are transferred into spaces
    and the three-character =12 escapes are turned into a single byte value.
    */
    static inline QByteArray translateQuotedPrintableToBin(const QByteArray &input)
    {
        QByteArray res;
        for (int i = 0; i < input.size(); ++i) {
            if (input[i] == '_') {
                res += ' ';
            } else if (input[i] == '=' && i < input.size() - 2) {
                int hi = hexValueOfChar(input[++i]);
                int lo = hexValueOfChar(input[++i]);
                if (hi != -1 && lo != -1) {
                    res += static_cast<char>((hi << 4) + lo);
                } else {
                    res += input.mid(i - 2, 3);
                }
            } else {
                res += input[i];
            }
        }
        return res;
    }


    /** @short Translate a percent-encoded array of bytes into binary characters

    The only transformation performed is RFC 2231 percent decoding.
    */
    static inline QByteArray translatePercentToBin(const QByteArray &input)
    {
        QByteArray res;
        for (int i = 0; i < input.size(); ++i) {
            if (input[i] == '%' && i < input.size() - 2) {
                int hi = hexValueOfChar(input[++i]);
                int lo = hexValueOfChar(input[++i]);
                if (hi != -1 && lo != -1) {
                    res += static_cast<char>((hi << 4) + lo);
                } else {
                    res += input.mid(i - 2, 3);
                }
            } else {
                res += input[i];
            }
        }
        return res;
    }

    /** @short Decode an encoded-word as per RFC2047 into a unicode string */
    static QString decodeWord(const QByteArray &fullWord, const QByteArray &charset, const QByteArray &encoding, const QByteArray &encoded)
    {
        if (encoding == "Q") {
            return Imap::decodeByteArray(translateQuotedPrintableToBin(encoded), charset);
        } else if (encoding == "B") {
            return Imap::decodeByteArray(QByteArray::fromBase64(encoded), charset);
        } else {
            return fullWord;
        }
    }

    /** @short Decode a header in the RFC 2047 format into a unicode string */
    static QString decodeWordSequence(const QByteArray& str)
    {
        QRegExp whitespace("^\\s+$");

        QString out;

        // Any idea why this isn't matching?
        //QRegExp encodedWord("\\b=\\?\\S+\\?\\S+\\?\\S*\\?=\\b");
        QRegExp encodedWord("\"?=\\?(\\S+)\\?(\\S+)\\?(.*)\\?=\"?");

        // set minimal=true, to match sequences which do not have whit space in between 2 encoded words; otherwise by default greedy matching is performed
        // eg. "Sm=?ISO-8859-1?B?9g==?=rg=?ISO-8859-1?B?5Q==?=sbord" will match "=?ISO-8859-1?B?9g==?=rg=?ISO-8859-1?B?5Q==?=" as a single encoded word without minimal=true
        // with minimal=true, "=?ISO-8859-1?B?9g==?=" will be the first encoded word and "=?ISO-8859-1?B?5Q==?=" the second.
        // -- assuming there are no nested encodings, will there be?
        encodedWord.setMinimal(true);

        int pos = 0;
        int lastPos = 0;

        while (pos != -1) {
            pos = encodedWord.indexIn(str, pos);
            if (pos != -1) {
                int endPos = pos + encodedWord.matchedLength();

                QString preceding(str.mid(lastPos, (pos - lastPos)));
                QString decoded = decodeWord(str.mid(pos, (endPos - pos)), encodedWord.cap(1).toLatin1(),
                                             encodedWord.cap(2).toUpper().toLatin1(), encodedWord.cap(3).toLatin1());

                // If there is only whitespace between two encoded words, it should not be included
                if (!whitespace.exactMatch(preceding))
                    out.append(preceding);

                out.append(decoded);

                pos = endPos;
                lastPos = pos;
            }
        }

        // Copy anything left
        out.append(QString::fromUtf8(str.mid(lastPos)));

        return out;
    }

    QByteArray toHexChar(const ushort unicode, char prefix)
    {
        const char hexChars[] = "0123456789ABCDEF";
        return QByteArray() + prefix + hexChars[(unicode >> 4) & 0xf] + hexChars[unicode & 0xf];
    }

}

namespace Imap {

QByteArray encodeRFC2047String(const QString &text, const Rfc2047StringCharacterSetType charset)
{
    // We can't allow more than 75 chars per encoded-word, including the boiler plate (7 chars and the size of the encoding spec)
    // -- this is defined by RFC2047.
    int maximumEncoded = 75 - 7;
    QByteArray encoding;
    if (charset == RFC2047_STRING_UTF8)
        encoding = "utf-8";
    else
        encoding = "iso-8859-1";
    maximumEncoded -= encoding.size();

    // If this is an encodedWord, we need to include any whitespace that we don't want to lose
    if (charset == RFC2047_STRING_UTF8) {
        QByteArray res;
        int start = 0;

        while (start < text.size()) {
            // as long as we have something to work on...
            int size = maximumEncoded;
            QByteArray candidate;

            // Find the character boundary at which we have to split the input.
            // Remember that we're iterating on Unicode codepoints now, not on raw bytes.
            while (true) {
                candidate = text.mid(start, size).toUtf8();
                int utf8Size = candidate.size();
                int base64Size = utf8Size * 4 / 3 + utf8Size % 3;
                if (base64Size <= maximumEncoded) {
                    // if this chunk's size is small enough, great
                    QByteArray encoded = candidate.toBase64();
                    if (!res.isEmpty())
                        res.append("\r\n ");
                    res.append("=?utf-8?B?" + encoded + "?=");
                    start += size;
                    break;
                } else {
                    // otherwise, try with something smaller
                    --size;
                    Q_ASSERT(size >= 1);
                }
            }
        }
        return res;
    } else {
        QByteArray buf = "=?" + encoding + "?Q?";
        int i = 0;
        int currentLineLength = 0;
        while (i < text.size()) {
            QByteArray symbol;
            const ushort unicode = text[i].unicode();
            if (unicode == 0x20) {
                symbol = "_";
            } else if (!rfc2047QPNeedsEscpaing(unicode)) {
                symbol += text[i].toLatin1();
            } else {
                symbol = toHexChar(unicode, '=');
            }
            currentLineLength += symbol.size();
            if (currentLineLength > maximumEncoded) {
                buf += "?=\r\n =?" + encoding + "?Q?";
                currentLineLength = 0;
            }
            buf += symbol;
            ++i;
        }
        buf += "?=";
        return buf;
    }
}

/** @short Interpret the raw byte array as a sequence of bytes in the given encoding */
QString decodeByteArray(const QByteArray &encoded, const QString &charset)
{
    if (QTextCodec *codec = codecForName(charset.toLatin1())) {
        return codec->toUnicode(encoded);
    }
    return QString::fromUtf8(encoded, encoded.size());
}

/** @short Encode the given string into RFC2047 form, preserving the ASCII leading part if possible */
QByteArray encodeRFC2047StringWithAsciiPrefix(const QString &text)
{
    // The maximal recommended line length, as defined by RFC 5322
    const int maxLineLength = 78;

    // Find first character which needs escaping
    int pos = 0;
    while (pos < text.size() && pos < maxLineLength &&
           (text[pos].unicode() == 0x20 || !rfc2047QPNeedsEscpaing(text[pos].unicode())))
        ++pos;

    // Find last character of a word which doesn't need escaping
    if (pos != text.size()) {
        while (pos > 0 && text[pos-1].unicode() != 0x20)
            --pos;
        if (pos > 0 && text[pos].unicode() == 0x20)
            --pos;
    }

    QByteArray prefix = text.left(pos).toUtf8();
    if (pos == text.size())
        return prefix;

    QString rest = text.mid(pos);
    Rfc2047StringCharacterSetType charset = charsetForInput(rest);

    return prefix + encodeRFC2047String(rest, charset);
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
                    return encodeRFC2047String(text, RFC2047_STRING_LATIN);
                }
            }

            return quotedString(unquoted);
        }
    }

    /* If the text has characters outside of the basic ASCII set, then
       it has to be encoded using the RFC2047 encoded-word syntax. */
    return encodeRFC2047String(text, RFC2047_STRING_UTF8);
}

/** @short Decode RFC2231-style eextended parameter values into a real Unicode string */
QString extractRfc2231Param(const QMap<QByteArray, QByteArray> &parameters, const QByteArray &key)
{
    QMap<QByteArray, QByteArray>::const_iterator it = parameters.constFind(key);
    if (it != parameters.constEnd()) {
        // This parameter is not using the RFC 2231 syntax for extended parameters.
        // I have no idea whether this is correct, but I *guess* that trying to use RFC2047 is not going to hurt.
        return decodeRFC2047String(*it);
    }

    if (parameters.constFind(key + "*0") != parameters.constEnd()) {
        // There's a 2231-style continuation *without* the language/charset extension
        QByteArray raw;
        int num = 0;
        while ((it = parameters.constFind(key + '*' + QByteArray::number(num++))) != parameters.constEnd()) {
            raw += *it;
        }
        return decodeRFC2047String(raw);
    }

    QByteArray raw;
    if ((it = parameters.constFind(key + '*')) != parameters.constEnd()) {
        // No continuation, but language/charset is present
        raw = *it;
    } else if (parameters.constFind(key + "*0*") != parameters.constEnd()) {
        // Both continuation *and* the lang/charset extension are in there
        int num = 0;
        // The funny thing is that the other values might or might not end with the trailing star,
        // at least according to the example in the RFC
        do {
            if ((it = parameters.constFind(key + '*' + QByteArray::number(num))) != parameters.constEnd())
                raw += *it;
            else if ((it = parameters.constFind(key + '*' + QByteArray::number(num) + '*')) != parameters.constEnd())
                raw += *it;
            ++num;
        } while (it != parameters.constEnd());
    }

    // Process 2231-style language/charset continuation, if present
    int pos1 = raw.indexOf('\'', 0);
    int pos2 = raw.indexOf('\'', qMax(1, pos1 + 1));
    if (pos1 != -1 && pos2 != -1) {
        return decodeByteArray(translatePercentToBin(raw.mid(pos2 + 1)), raw.left(pos1));
    }

    // Fallback: it could be empty, or otherwise malformed. Just treat it as UTF-8 for compatibility
    return QString::fromUtf8(raw);
}

/** @short Produce a parameter for a MIME message header */
QByteArray encodeRfc2231Parameter(const QByteArray &key, const QString &value)
{
    if (value.isEmpty())
        return key + "=\"\"";

    bool safeAscii = true;

    // Find "dangerous" characters
    for (int i = 0; i < value.size(); ++i) {
        if (rfc2311NeedsEscaping(value[i].unicode())) {
            safeAscii = false;
            break;
        }
    }

    if (safeAscii)
        return key + '=' + value.toUtf8();

    QByteArray res = key + "*=\"utf-8''";
    QByteArray encoded = value.toUtf8();
    for (int i = 0; i < encoded.size(); ++i) {
        char unicode = encoded[i];
        if (rfc2311NeedsEscaping(unicode))
            res += toHexChar(unicode, '%');
        else
            res += unicode;
    }
    return res + '"';
}

/** @short Insert extra spaces to match the SHOULD from RFC3676

The format=flowed specification contains a SHOULD provision that long paragraphs are to be wrapped so that they are no longer
than 78 characters. This function will insert line breaks (with the appropriate space guards) so that the lines of text are no
longer than a certain amount of *characters*. Please note that this limit is about Unicode code points as provided by QString
and hence not a limit on the number of actual bytes that the line will occupy after being encoded into UTF-8 and the result
encoded in quoted-printable.

This function also pretends that "space" is only the ASCII space; it will not attempt to insert any spacing into the middle of
a sequence of other characters, even if that sequence contained something which can be considered a "word boundary" in some
non-Latin script. I guess that violating a SHOULD by producing slightly longer chunk of bytes is better than breaking a foreign
script which I know nothing about.
*/
QString wrapFormatFlowed(const QString &input)
{
    QRegExp justQuotationAndSpaces(QLatin1String("[> ]"));

    // Determining a proper cutoff is not easy. The Q-P allows for at most 76 chars per line, not counting the trailing CR LF.
    // Suppose there's exactly one multibyte character which gets decoded into two bytes in UTF-8. After these two bytes are
    // encoded via quoted-printable, they will take up six bytes together (each byte is converted to hex and prepended by "=").
    // This means that such a line could contain only 70 ASCII characters and one non-ASCII one (assuming it gets encoded as a
    // two-byte sequence in UTF-8) and it will still fit into a single line in Q-P.
    //
    // However, the only points for making this guess right are:
    // a) to fit the SHOULD requirement in RFC 3676 [1] which says that line lengths should be <= 78 characters,
    // b) to avoid Q-P doing excessive line wrapping later on
    //
    // If the text contain non-ASCII characters, these are not going to be readable without a proper Q-P decoder. As such, the
    // argument for not triggering "useless" breaks at the QP time has much lower weight in such situations -- the aesthetics of
    // the raw, QP-encoded form are not terribly relevant IMHO.
    //
    // The b) leads us to 76 characters, while yet another sentence in RFC 3676 suggests wrapping at 72, and even speaks about
    // 66 as aestheticaly pleasing.
    //
    // Finally, because these "76 characters" have to include the trailing space, we're now at 75.
    //
    // [1] http://tools.ietf.org/html/rfc3676#section-4.2
    const int defaultCutof = 75;

    QStringList res;
    Q_FOREACH(QString line, input.split(QLatin1Char('\n'), QString::KeepEmptyParts)) {
        line.remove(QLatin1Char('\r'));
        if (line.isEmpty()) {
            res << line;
            continue;
        }

        int previousBreak = 0;
        while (previousBreak < line.size()) {
            // Find a place to insert the line break
            int size = defaultCutof;
            if (line.size() <= previousBreak + size) {
                // We can safely use this chunk
            } else if (line.at(previousBreak + size) == QLatin1Char(' ')) {
                // We've found our space -- let's just use it and be done here
            } else {
                // OK, let's find a place to break the words at. At first, go back and try to find any possibility of reusing
                // an existing space.
                while (size > 0 && line.at(previousBreak + size) != QLatin1Char(' ')) {
                    --size;
                    if (justQuotationAndSpaces.exactMatch(line.left(size))) {
                        // Too bad; there is nothing but quotation at the beginning of this line. We cannot break the line here!
                        // Doing so would create line which look like this:
                        //   >[space]
                        //   quoted text goes here
                        size = 0;
                    }
                }
                if (size == 0) {
                    size = defaultCutof;
                    while (previousBreak + size < line.size() && line.at(previousBreak + size) != QLatin1Char(' ')) {
                        ++size;
                    }
                }
            }

            Q_ASSERT(previousBreak + size >= line.size() || line.at(previousBreak + size) == QLatin1Char(' '));

            // Now we want to insert the newline *after* the space
            ++size;

            res << line.mid(previousBreak, size);
            previousBreak += size;
        }
    }
    return res.join(QLatin1String("\r\n"));
}

void decodeContentTransferEncoding(const QByteArray &rawData, const QByteArray &encoding, QByteArray *outputData)
{
    Q_ASSERT(outputData);
    if (encoding == "quoted-printable") {
        *outputData = quotedPrintableDecode(rawData);
    } else if (encoding == "base64") {
        *outputData = QByteArray::fromBase64(rawData);
    } else if (encoding.isEmpty() || encoding == "7bit" || encoding == "8bit" || encoding == "binary") {
        *outputData = rawData;
    } else {
        qDebug() << "Warning: unknown encoding" << encoding;
        *outputData = rawData;
    }
}

}
