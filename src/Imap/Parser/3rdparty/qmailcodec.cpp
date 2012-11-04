/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Qt Software Information (qt-info@nokia.com)
**
** This file is part of the Qt Messaging Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the either Technology Preview License Agreement or the
** Beta Release License Agreement.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain
** additional rights. These rights are described in the Nokia Qt LGPL
** Exception version 1.0, included in the file LGPL_EXCEPTION.txt in this
** package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at qt-sales@nokia.com.
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmailcodec.h"
#include <QIODevice>
#include <QTextCodec>
#include <ctype.h>
#include <QtDebug>

// Allow these values to be reduced from test harness code:
int QTOPIAMAIL_EXPORT MaxCharacters = QMailCodec::ChunkCharacters;
// Can be any number:
int QTOPIAMAIL_EXPORT QuotedPrintableMaxLineLength = 74;


/*!
  \class QMailCodec

  \preliminary
  \brief The QMailCodec class provides mechanisms for encoding and decoding between 7-bit ASCII strings
  and arbitrary octet sequences.

  \ingroup messaginglibrary

  Messages transferred via the SMTP protocol must be encoded in 7-bit ASCII characters, even though
  their contents are typically composed in sequences of 8-bit octets.  The QMailCodec class provides
  an interface through which data can be easily converted between an 8-bit octet sequence and
  a 7-bit ASCII character sequence.

  QMailCodec is an abstract class; in order to perform a coding operation, a derived class
  must be used that provides a policy for mapping 8-bit data to and from 7-bit ASCII characters.
  This policy is implemented by overriding the encodeChunk() and decodeChunk() virtual functions.

  Using the QMailCodec interface, data can be encoded or decoded from an input QDataStream to an 
  output QDataStream, or for convenience, from an input QByteArray to an output QByteArray.

  If the data to be encoded is in unicode form, then the QMailCodec interface can be used to
  convert the data to ASCII via an intermediate QTextCodec, which converts the incoming text
  to a sequence of octets.  The QTextCodec used is specified by the name of the encoding
  produced, or that decoded when decoding an ASCII input sequence.  QMailCodec provides functions 
  to encode from a QTextStream to a QDataStream, and to decode from a QDataStream to a QTextStream.
  For convenience, it is also possible to encode a QString to a QByteArray, and to decode a 
  QByteArray to a QString.

  \sa QDataStream, QTextStream, QTextCodec
*/

/*!
    \fn void QMailCodec::encodeChunk(QDataStream& out, const unsigned char* input, int length, bool finalChunk)

    Overridden by derived classes to perform an encoding operation.  The implementation function
    must encode \a length 8-bit octets at the location \a input, writing the resulting ASCII characters 
    to the stream \a out.  If \a finalChunk is false, further calls will be made to encodeChunk()
    with continued input data.  Otherwise, the encoding operation is complete.
*/

/*!
    \fn void QMailCodec::decodeChunk(QDataStream& out, const char* input, int length, bool finalChunk)

    Overridden by derived classes to perform a decoding operation.  The implementation function
    must decode \a length ASCII characters at the location \a input, writing the resulting octets
    to the stream \a out.  If \a finalChunk is false, further calls will be made to decodeChunk()
    with continued input data.  Otherwise, the decoding operation is complete.
*/

/*!
    Destroys a QMailCodec instance.
*/
QMailCodec::~QMailCodec()
{
}

/*!
    \fn QMailCodec::name() const

    Returns a string that identifies the subclass of QMailCodec that this instance belongs to.
*/

static void enumerateCodecs()
{
    static bool enumerated = false;

    if (!enumerated)
    {
        qWarning() << "Available codecs:";
        foreach (const QByteArray& codec, QTextCodec::availableCodecs())
            qWarning() << "  " << codec;

        enumerated = true;
    }
}

static QTextCodec* codecForName(const QByteArray& charset, bool translateAscii = true)
{
    QByteArray encoding(charset.toLower());

    if (!encoding.isEmpty())
    {
        int index;

        if (translateAscii && encoding.contains("ascii")) 
        {
            // We'll assume the text is plain ASCII, to be extracted to Latin-1
            encoding = "ISO-8859-1";
        }
        else if ((index = encoding.indexOf('*')) != -1)
        {
            // This charset specification includes a trailing language specifier
            encoding = encoding.left(index);
        }

        QTextCodec* codec = QTextCodec::codecForName(encoding);
        if (!codec)
        {
            qWarning() << "QMailCodec::codecForName - Unable to find codec for charset" << encoding;
            enumerateCodecs();
        }

        return codec;
    }

    return 0;
}

/*!
    Writes the data read from the stream \a in to the stream \a out, as a sequence 
    of 7-bit ASCII characters.  The unicode characters read from \a in are first 
    encoded to the text encoding \a charset.

    \sa QTextCodec::codecForName()
*/
void QMailCodec::encode(QDataStream& out, QTextStream& in, const QString& charset)
{
    if (QTextCodec* codec = codecForName(charset.toLatin1()))
    {
        while (!in.atEnd())
        {
            QString chunk = in.read(MaxCharacters);
            QByteArray charsetEncoded = codec->fromUnicode(chunk);

            encodeChunk(out, 
                        reinterpret_cast<const unsigned char*>(charsetEncoded.constData()), 
                        charsetEncoded.length(),
                        in.atEnd());
        }
    }
}

/*!
    Writes the data read from the stream \a in to the stream \a out, converting from 
    a sequence of 7-bit ASCII characters.  The characters read from \a in are 
    decoded from the text encoding \a charset to unicode.

    \sa QTextCodec::codecForName()
*/
void QMailCodec::decode(QTextStream& out, QDataStream& in, const QString& charset)
{
    if (QTextCodec* codec = codecForName(charset.toLatin1()))
    {
        QByteArray decoded;
        {
            QDataStream decodedStream(&decoded, QIODevice::WriteOnly);
            
            char* buffer = new char[MaxCharacters];
            while (!in.atEnd())
            {
                int length = in.readRawData(buffer, MaxCharacters);

                // Allow for decoded data to be twice the size without reallocation
                decoded.reserve(decoded.size() + (MaxCharacters * 2));

                decodeChunk(decodedStream, buffer, length, in.atEnd());
            }
            delete [] buffer;
        }

        // This is an unfortunately-necessary copy operation; we should investigate
        // modifying QTextCodec to support a stream interface
        QString unicode = codec->toUnicode(decoded);
        out << unicode;
        out.flush();
    }
}


QString decodeByteArray(const QByteArray &encoded, const QString &charset)
{
    if (QTextCodec *codec = codecForName(charset.toLatin1())) {
        return codec->toUnicode(encoded);
    }
    return QString();
}

/*!
    Writes the data read from the stream \a in to the stream \a out, as a sequence 
    of 7-bit ASCII characters.
*/
void QMailCodec::encode(QDataStream& out, QDataStream& in)
{
    char* buffer = new char[MaxCharacters];
    while (!in.atEnd())
    {
        int length = in.readRawData(buffer, MaxCharacters);

        encodeChunk(out, reinterpret_cast<unsigned char*>(buffer), length, in.atEnd());
    }
    delete [] buffer;
}

/*!
    Writes the data read from the stream \a in to the stream \a out, converting from 
    a sequence of 7-bit ASCII characters.
*/
void QMailCodec::decode(QDataStream& out, QDataStream& in)
{
    char* buffer = new char[MaxCharacters];
    while (!in.atEnd())
    {
        int length = in.readRawData(buffer, MaxCharacters);

        decodeChunk(out, buffer, length, in.atEnd());
    }
    delete [] buffer;
}

/*!
    Writes the data read from the stream \a in to the stream \a out, without conversion.
*/
void QMailCodec::copy(QDataStream& out, QDataStream& in)
{
    char* buffer = new char[MaxCharacters];
    while (!in.atEnd())
    {
        int length = in.readRawData(buffer, MaxCharacters);
        out.writeRawData(buffer, length);
    }
    delete [] buffer;
}

/*!
    Writes the data read from the stream \a in to the stream \a out, without conversion.
*/
void QMailCodec::copy(QTextStream& out, QTextStream& in)
{
    while (!in.atEnd())
    {
        QString input = in.read(MaxCharacters);
        out << input;
    }
}

/*!
    Returns a QByteArray containing the string \a input, encoded to the text encoding \a charset 
    and then to a sequence of 7-bit ASCII characters.

    \sa QTextCodec::codecForName()
*/
QByteArray QMailCodec::encode(const QString& input, const QString& charset)
{
    QByteArray result;
    {
        QDataStream out(&result, QIODevice::WriteOnly);

        // We can't currently guarantee that this is safe - we should investigate modifying
        // QTextStream to support a read-only interface...
        QTextStream in(const_cast<QString*>(&input), QIODevice::ReadOnly);

        encode(out, in, charset);
    }

    return result;
}

/*!
    Returns a QString containing characters decoded from the text encoding \a charset, which 
    are decoded from the sequence of 7-bit ASCII characters read from \a input. 

    \sa QTextCodec::codecForName()
*/
QString QMailCodec::decode(const QByteArray& input, const QString& charset)
{
    QString result;
    {
        QTextStream out(&result, QIODevice::WriteOnly);
        QDataStream in(input);
        decode(out, in, charset);
    }

    return result;
}

/*!
    Returns a QByteArray containing the octets from \a input, encoded to a sequence of 
    7-bit ASCII characters.
*/
QByteArray QMailCodec::encode(const QByteArray& input)
{
    QByteArray result;
    {
        QDataStream out(&result, QIODevice::WriteOnly);
        QDataStream in(input);

        encode(out, in);
    }

    return result;
}

/*!
    Returns a QByteArray containing the octets decoded from the sequence of 7-bit ASCII
    characters in \a input.
*/
QByteArray QMailCodec::decode(const QByteArray& input)
{
    QByteArray result;
    {
        QDataStream out(&result, QIODevice::WriteOnly);
        QDataStream in(input);

        decode(out, in);
    }

    return result;
}


// ASCII character values used throughout
const unsigned char MinPrintableRange = 0x20;
const unsigned char MaxPrintableRange = 0x7e;
const unsigned char HorizontalTab = 0x09;
const unsigned char LineFeed = 0x0a;
const unsigned char FormFeed = 0x0c;
const unsigned char CarriageReturn = 0x0d;
const unsigned char Space = 0x20;
const unsigned char Equals = 0x3d;
const unsigned char QuestionMark = 0x3f;
const unsigned char ExclamationMark = 0x21;
const unsigned char Asterisk = 0x2a;
const unsigned char Plus = 0x2b;
const unsigned char Minus = 0x2d;
const unsigned char Slash = 0x2f;
const unsigned char Underscore = 0x5f;



// Static data and functions for Quoted-Prinatable codec
static const unsigned char NilPreceding = 0x7f;
static const char QuotedPrintableCharacters[16 + 1] = "0123456789ABCDEF";
static const unsigned char* QuotedPrintableValues = reinterpret_cast<const unsigned char*>(QuotedPrintableCharacters);

static bool requiresEscape(unsigned char input, QMailQuotedPrintableCodec::ConformanceType conformance, int charsRemaining)
{
    // For both, we need to escape '=' and anything unprintable
    bool escape = ((input > MaxPrintableRange) || 
                   ((input < MinPrintableRange) && (input != HorizontalTab) && (input != FormFeed)) ||
                   (input == Equals));

    // For RFC 2047, we need to escape '?', '_', ' ' & '\t'
    // In fact, since the output may be used in a header field 'word', then the only characters
    // that can be used un-escaped are: alphanumerics, '!', '*', '+' '-', '/' and '_'
    if (!escape && (conformance == QMailQuotedPrintableCodec::Rfc2047))
    {
        // We can also ignore space, since it will become an underscore
        if ((input != ExclamationMark) && (input != Asterisk) && (input != Plus) && 
            (input != Minus) && (input != Slash) && (input != Underscore) && (input != Space))
        {
            escape = !isalnum(input);
        }
    }

    if (!escape && (input == HorizontalTab || input == Space))
    {
        // The (potentially) last whitespace character on a line must be escaped
        if (charsRemaining <= 3)
            escape = true;
    }

    return escape;
}

static inline void encodeCharacter(QDataStream& out, unsigned char value)
{
    out << static_cast<unsigned char>(Equals);
    out << QuotedPrintableValues[value >> 4];
    out << QuotedPrintableValues[value & 0x0f];
}

static inline void lineBreak(QDataStream& out, int* _encodeLineCharsRemaining, int maximumLineLength)
{
    out << static_cast<unsigned char>(Equals);
    out << static_cast<unsigned char>(LineFeed);

    *_encodeLineCharsRemaining = maximumLineLength;
}

static inline unsigned char decodeCharacter(unsigned char value)
{
    if ((value >= 0x30) && (value <= 0x39))
        return (value - 0x30);

    if ((value >= 0x41) && (value <= 0x46))
        return ((value - 0x41) + 10);

    if ((value >= 0x61) && (value <= 0x66))
        return ((value - 0x61) + 10);

    return 0;
}


/*!
  \class QMailQuotedPrintableCodec

  \preliminary
  \brief The QMailQuotedPrintableCodec class encodes or decodes between 8-bit data and 7-bit ASCII, 
  using the 'quoted printable' character mapping scheme.

  \ingroup messaginglibrary

  The 'quoted printable' character mapping scheme maps arbitrary 8-bit values into 7-bit ASCII
  characters, by replacing values that cannot be directly represented with an escape sequence.
  The mapping scheme used is defined in 
  \l{http://www.ietf.org/rfc/rfc2045.txt} {RFC 2045} (Multipurpose Internet Mail Extensions Part One). 
  A minor variation on the scheme is defined as the '"Q" encoding' for 'encoded words' in
  \l{http://www.ietf.org/rfc/rfc2047.txt} {RFC 2047} (Multipurpose Internet Mail Extensions Part Three). 

  The 'quoted printable' scheme encodes only those incoming octet values that cannot be directly
  represented in ASCII, by replacing the input octet with a three-character sequence that encodes 
  the numeric value of the original octet.  Therefore, the ratio of input length to output length 
  for any input data sequence depends on the percentage of the input that corresponds to ASCII 
  values, with ASCII-like encodings producing only small increases.  With an input data encoding 
  such as Latin-1 (ISO-8859-1), the output maintains a reasonable degree of human-readability.

  An instance of QMailQuotedPrintableCodec contains state information about the encoding or decoding
  operation it performs, so an instance should be used for a single coding operation only:

  \code
  QByteArray asciiData = acquireInput();

  // We know the data is text in Latin-1 encoding, so decode the data from 
  // quoted printable ASCII encoding, and then decode from Latin-1 to unicode
  QMailQuotedPrintableCodec decoder(QMailQuotedPrintableCodec::Text, QMailQuotedPrintableCodec::Rfc2045);
  QString textData = decoder.decode(asciiData, "ISO-8859-1");
  \endcode

  \sa QMailCodec
*/

/*!
    \enum QMailQuotedPrintableCodec::ContentType

    This enumerated type is used to specify whether content is textual data or binary data. 

    \value Text     The data is textual data; newline sequences within the data will be converted during coding.
    \value Binary   The data is not textual, and does not contain newline sequences.
*/

/*!
    \enum QMailQuotedPrintableCodec::ConformanceType

    This enumerated type is used to specify which RFC the coding operation should conform to.

    \value Rfc2045  The coding should be performed according to the requirements of RFC 2045.
    \value Rfc2047  The coding should be performed according to the requirements of RFC 2047's '"Q" encoding'.
*/

/*!
    Constructs a codec object for coding data of type \a content, using the mapping scheme
    specified by the requirements of \a conformance.

    If \a content is QMailQuotedPrintableCodec::Text, then newline sequences will be converted
    between the local representation (for example, 0x0A on Unix) and the transmission standard
    representation (0x0D 0x0A). Otherwise, the data will be coded without modification.

    If \a conformance is QMailQuotedPrintableCodec::Rfc2047, then coding will use the mapping
    scheme of the 
    \l{http://www.ietf.org/rfc/rfc2047.txt} {RFC 2047} '"Q" encoding'; otherwise the scheme defined in 
    \l{http://www.ietf.org/rfc/rfc2045.txt} {RFC 2045} will be used.

    The maximum number of encoded output characters per line can be specified as \a maximumLineLength.  
    If not specified, or specified to a non-positive value, a default value will be used.
*/
QMailQuotedPrintableCodec::QMailQuotedPrintableCodec(ContentType content, ConformanceType conformance, int maximumLineLength)
    : _content(content),
      _conformance(conformance),
      _maximumLineLength(maximumLineLength)
{
    // We're allowed up to 76 chars per output line, but the RFC isn't really clear on 
    // whether this includes the '=' and '\n' of a soft line break, so we'll assume they're counted
    if (_maximumLineLength <= 0)
        _maximumLineLength = QuotedPrintableMaxLineLength;

    _encodeLineCharsRemaining = _maximumLineLength;
    _encodeLastChar = '\0';

    _decodePrecedingInput = NilPreceding;
    _decodeLastChar = '\0';
}

/*! \reimp */
QString QMailQuotedPrintableCodec::name() const
{
    return QLatin1String("QMailQuotedPrintableCodec");
}

bool rfc2047QPNeedsEscpaing(const int unicode)
{
    if (unicode <= Space)
        return true;
    if (unicode == Equals || unicode == QuestionMark || unicode == Underscore)
        return true;
    if (unicode > MaxPrintableRange)
        return true;
    return false;
}

/*! \internal */
void QMailQuotedPrintableCodec::encodeChunk(QDataStream& out, const unsigned char* it, int length, bool finalChunk)
{
    // Set the input pointers relative to this input
    const unsigned char* const end = it + length;

    while (it != end)
    {
        unsigned char input = *it++;

        if (_conformance == Rfc2047) {
            // This is *very* different from the RFC2045 encoding.
            // Just don't bother fighting the QMF code and use this shortcut.
            if (input == Space) {
                out << static_cast<unsigned char>(Underscore);
            } else if (!rfc2047QPNeedsEscpaing(input)) {
                out << input;
            } else {
                encodeCharacter(out, input);
            }
            continue;
        }

        if ((input == CarriageReturn || input == LineFeed) && (_content == Text))
        {
            if (_encodeLastChar == CarriageReturn && input == LineFeed)
            {
                // We have already encoded this character-sequence
            }
            else 
            {
                // We must replace this character with ascii CRLF
                out << CarriageReturn << LineFeed;
            }

            _encodeLastChar = input;
            _encodeLineCharsRemaining = _maximumLineLength;
            continue;
        }

        bool escape = requiresEscape(input, _conformance, _encodeLineCharsRemaining);
        int charsRequired = (escape ? 3 : 1);

        // If we can't fit this character on the line, insert a line break
        if (charsRequired > _encodeLineCharsRemaining)
        {
            lineBreak(out, &_encodeLineCharsRemaining, _maximumLineLength); 

            // We may no longer need the encoding after the line break
            if (input == Space || (input == HorizontalTab && _conformance != Rfc2047))
                charsRequired = 1;
        }

        if (charsRequired == 1)
        {
            if (input == Space && _conformance == Rfc2047) // output space as '_'
                out << static_cast<unsigned char>(Underscore);
            else
                out << input;
        }
        else
            encodeCharacter(out, input);

        _encodeLineCharsRemaining -= charsRequired;

        if ((_encodeLineCharsRemaining == 0) && !(finalChunk && (it == end)))
            lineBreak(out, &_encodeLineCharsRemaining, _maximumLineLength); 

        _encodeLastChar = input;
    }

    Q_UNUSED(finalChunk)
}

/*! \internal */
void QMailQuotedPrintableCodec::decodeChunk(QDataStream& out, const char* it, int length, bool finalChunk)
{
    const char* const end = it + length;

    // The variable _decodePrecedingInput holds any unprocessed input from a previous call:
    // If '=', we've parsed only that char, otherwise, it is the hex value of the first parsed character
    if ((_decodePrecedingInput != NilPreceding) && (it != end))
    {
        unsigned char value = 0;
        if (_decodePrecedingInput == Equals)
        {
            // Get the first escaped char
            unsigned char input = *it++;
            if (input == LineFeed || input == CarriageReturn)
            {
                // This is only a soft-line break
                _decodePrecedingInput = NilPreceding;
            }
            else
            {
                value = decodeCharacter(input);
                _decodePrecedingInput = value;
            }
        }
        else
        {
            // We already have partial escaped input
            value = _decodePrecedingInput;
        }

        if (it != end && _decodePrecedingInput != NilPreceding)
        {
            out << static_cast<unsigned char>((value << 4) | decodeCharacter(*it++));
            _decodePrecedingInput = NilPreceding;
        }
    }

    while (it != end)
    {
        unsigned char input = *it++;
        if (input == Equals)
        {
            // We are in an escape sequence
            if (it == end)
            {
                _decodePrecedingInput = Equals;
            }
            else
            {
                input = *it++;
                if (input == LineFeed || input == CarriageReturn)
                {
                    // This is a soft-line break - move on
                }
                else
                {
                    // This is an encoded character
                    unsigned char value = decodeCharacter(input);

                    if (it == end)
                    {
                        _decodePrecedingInput = value;
                    }
                    else
                    {
                        out << static_cast<unsigned char>((value << 4) | decodeCharacter(*it++));
                    }
                }
            }
        }
        else 
        {
            if ((input == CarriageReturn || input == LineFeed) && (_content == Text))
            {
                if (_decodeLastChar == CarriageReturn && input == LineFeed)
                {
                    // We have already processed this sequence
                }
                else
                {
                    // We should output the local newline sequence, but we can't
                    // because we don't know what it is, and C++ translation-from-\n will
                    // only work if the stream is a file...
                    out << static_cast<unsigned char>('\n');
                }
            }
            else if (input == Underscore && _conformance == Rfc2047)
                out << static_cast<unsigned char>(Space);
            else
                out << input;
        }

        _decodeLastChar = input;
    }

    if (finalChunk && _decodePrecedingInput != NilPreceding)
    {
        qWarning() << "Huh? unfinished escape sequence...";
    }
}

static void writeStream(QDataStream& out, const char* it, int length)
{
    int totalWritten = 0;
    while (totalWritten < length)
    {
        int bytesWritten = out.writeRawData(it + totalWritten, length - totalWritten);
        if (bytesWritten == -1)
            return;

        totalWritten += bytesWritten;
    }
}

/*!
  \class QMailPassThroughCodec

  \preliminary
  \brief The QMailPassThroughCodec class uses the QMailCodec interface to move data between streams
  without coding or decoding.

  \ingroup messaginglibrary

  The QMailPassThroughCodec allows client code to use the same QMailCodec interface to convert data between
  different ASCII encodings, or no encoding at all, without having to be aware of the details involved.

  The pass-through codec is primarily useful when communicating with SMTP servers supporting the
  \l{http://www.ietf.org/rfc/rfc1652.txt} {RFC 1652} (8BITMIME) extension, which permits the exchange
  of data without coding via 7-bit ASCII.  

  A QMailPassThroughCodec can be instantiated directly, but is more likely to be used polymorphically:

  \code
  // Get an object to perform the encoding required for the current server
  QMailCodec* encoder = getCodecForServer(currentServer());

  // If the codec returned is a QMailPassThroughCodec, the input data will 
  // be written to the output stream without encoding to 7-bit ASCII
  encoder->encode(outputStream, inputStream);
  \endcode

  \sa QMailCodec
*/

/*! \reimp */
QString QMailPassThroughCodec::name() const
{
    return QLatin1String("QMailPassThroughCodec");
}

/*! \internal */
void QMailPassThroughCodec::encodeChunk(QDataStream& out, const unsigned char* it, int length, bool finalChunk)
{
    writeStream(out, reinterpret_cast<const char*>(it), length);

    Q_UNUSED(finalChunk)
}

/*! \internal */
void QMailPassThroughCodec::decodeChunk(QDataStream& out, const char* it, int length, bool finalChunk)
{
    writeStream(out, it, length);

    Q_UNUSED(finalChunk)
}


/*!
  \class QMailLineEndingCodec

  \preliminary
  \brief The QMailLineEndingCodec class encodes textual data to use CR/LF line endings required for SMTP transmission.

  \ingroup messaginglibrary

  The QMailLineEndingCodec allows client code to use the QMailCodec interface to encode textual data
  from the local line-ending convention to the CR/LF convention required for SMTP transmission.  The 
  codec will convert from single carriage return or single line feed line-endings to CR/LF pairs, or 
  will preserve data already using the correct encoding.

  Decoded data will have CR/LF pairs converted to \c \n.

  An instance of QMailLineEndingCodec contains state information about the encoding or decoding
  operation it performs, so an instance should be used for a single coding operation only:

  \sa QMailCodec
*/

/*!
    Constructs a codec object for coding text data, converting between the local line-ending
    convention and the CR/LF line-ending sequence required for SMTP transmission.
*/
QMailLineEndingCodec::QMailLineEndingCodec()
    : _lastChar(0)
{
}

/*! \reimp */
QString QMailLineEndingCodec::name() const
{
    return QLatin1String("QMailLineEndingCodec");
}

/*! \internal */
void QMailLineEndingCodec::encodeChunk(QDataStream& out, const unsigned char* it, int length, bool finalChunk)
{
    const unsigned char* const end = it + length;

    const unsigned char* begin = it;
    while (it != end)
    {
        const unsigned char input = *it;
        if (input == CarriageReturn || input == LineFeed)
        {
            if (_lastChar == CarriageReturn && input == LineFeed)
            {
                // We have already encoded this character-sequence; skip the input
                begin = (it + 1);
            }
            else 
            {
                // Write the preceding characters
                if (it > begin)
                    writeStream(out, reinterpret_cast<const char*>(begin), (it - begin));

                // We must replace this character with ascii CRLF
                out << CarriageReturn << LineFeed;
                begin = (it + 1);
            }
        }

        _lastChar = input;
        ++it;
    }

    if (it > begin)
    {
        // Write the remaining characters
        writeStream(out, reinterpret_cast<const char*>(begin), (it - begin));
    }

    Q_UNUSED(finalChunk)
}

/*! \internal */
void QMailLineEndingCodec::decodeChunk(QDataStream& out, const char* it, int length, bool finalChunk)
{
    const char* const end = it + length;

    const char* begin = it;
    while (it != end)
    {
        const char input = *it;
        if (input == CarriageReturn || input == LineFeed)
        {
            if (_lastChar == CarriageReturn && input == LineFeed)
            {
                // We have already processed this sequence
                begin = (it + 1);
            }
            else
            {
                // Write the preceding characters
                if (it > begin)
                    writeStream(out, begin, (it - begin));

                // We should output the local newline sequence, but we can't
                // because we don't know what it is, and C++ translation-from-\n will
                // only work if the stream is a file...
                out << static_cast<unsigned char>('\n');
                begin = (it + 1);
            }
        }

        _lastChar = input;
        ++it;
    }

    if (it > begin)
    {
        // Write the remaining characters
        writeStream(out, begin, (it - begin));
    }

    Q_UNUSED(finalChunk)
}

