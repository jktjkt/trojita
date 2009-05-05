/*
  kmime_util.cpp

  KMime, the KDE internet mail/usenet news message library.
  Copyright (c) 2001 the KMime authors.
  See file AUTHORS for details

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include "kmime_util.h"
#include "kmime_util_p.h"
#include "kmime_header_parsing.h"

#include <config-kmime.h>
#include <kdefakes.h> // for strcasestr
#include <kglobal.h>
#include <klocale.h>
#include <kcharsets.h>
#include <kcodecs.h>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QTextCodec>

#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

using namespace KMime;

namespace KMime {

QList<QByteArray> c_harsetCache;
QList<QByteArray> l_anguageCache;

QByteArray cachedCharset( const QByteArray &name )
{
  foreach ( const QByteArray& charset, c_harsetCache ) {
    if ( qstricmp( name.data(), charset.data() ) == 0 ) {
      return charset;
    }
  }

  c_harsetCache.append( name.toUpper() );
  //kDebug(5320) << "KNMimeBase::cachedCharset() number of cs" << c_harsetCache.count();
  return c_harsetCache.last();
}

QByteArray cachedLanguage( const QByteArray &name )
{
  foreach ( const QByteArray& language, l_anguageCache ) {
    if ( qstricmp( name.data(), language.data() ) == 0 ) {
      return language;
    }
  }

  l_anguageCache.append( name.toUpper() );
  //kDebug(5320) << "KNMimeBase::cachedCharset() number of cs" << c_harsetCache.count();
  return l_anguageCache.last();
}

bool isUsAscii( const QString &s )
{
  uint sLength = s.length();
  for ( uint i=0; i<sLength; i++ ) {
    if ( s.at( i ).toLatin1() <= 0 ) { // c==0: non-latin1, c<0: non-us-ascii
      return false;
    }
  }
  return true;
}

// "(),.:;<>@[\]
const uchar specialsMap[16] = {
  0x00, 0x00, 0x00, 0x00, // CTLs
  0x20, 0xCA, 0x00, 0x3A, // SPACE ... '?'
  0x80, 0x00, 0x00, 0x1C, // '@' ... '_'
  0x00, 0x00, 0x00, 0x00  // '`' ... DEL
};

// "(),:;<>@[\]/=?
const uchar tSpecialsMap[16] = {
  0x00, 0x00, 0x00, 0x00, // CTLs
  0x20, 0xC9, 0x00, 0x3F, // SPACE ... '?'
  0x80, 0x00, 0x00, 0x1C, // '@' ... '_'
  0x00, 0x00, 0x00, 0x00  // '`' ... DEL
};

// all except specials, CTLs, SPACE.
const uchar aTextMap[16] = {
  0x00, 0x00, 0x00, 0x00,
  0x5F, 0x35, 0xFF, 0xC5,
  0x7F, 0xFF, 0xFF, 0xE3,
  0xFF, 0xFF, 0xFF, 0xFE
};

// all except tspecials, CTLs, SPACE.
const uchar tTextMap[16] = {
  0x00, 0x00, 0x00, 0x00,
  0x5F, 0x36, 0xFF, 0xC0,
  0x7F, 0xFF, 0xFF, 0xE3,
  0xFF, 0xFF, 0xFF, 0xFE
};

// none except a-zA-Z0-9!*+-/
const uchar eTextMap[16] = {
  0x00, 0x00, 0x00, 0x00,
  0x40, 0x35, 0xFF, 0xC0,
  0x7F, 0xFF, 0xFF, 0xE0,
  0x7F, 0xFF, 0xFF, 0xE0
};

QString decodeRFC2047String( const QByteArray &src, QByteArray &usedCS,
                             const QByteArray &defaultCS, bool forceCS )
{
  QByteArray result;
  QByteArray spaceBuffer;
  const char *scursor = src.constData();
  const char *send = scursor + src.length();
  bool onlySpacesSinceLastWord = false;

  while ( scursor != send ) {
     // space
    if ( isspace( *scursor ) && onlySpacesSinceLastWord ) {
      spaceBuffer += *scursor++;
      continue;
    }

    // possible start of an encoded word
    if ( *scursor == '=' ) {
      QByteArray language;
      QString decoded;
      ++scursor;
      const char *start = scursor;
      if ( HeaderParsing::parseEncodedWord( scursor, send, decoded, language, usedCS, defaultCS, forceCS ) ) {
        result += decoded.toUtf8();
        onlySpacesSinceLastWord = true;
        spaceBuffer.clear();
      } else {
        if ( onlySpacesSinceLastWord ) {
          result += spaceBuffer;
          onlySpacesSinceLastWord = false;
        }
        result += '=';
        scursor = start; // reset cursor after parsing failure
      }
      continue;
    } else {
      // unencoded data
      if ( onlySpacesSinceLastWord ) {
        result += spaceBuffer;
        onlySpacesSinceLastWord = false;
      }
      result += *scursor;
      ++scursor;
    }
  }

  return QString::fromUtf8(result);
}

QString decodeRFC2047String( const QByteArray &src )
{
  QByteArray usedCS;
  return decodeRFC2047String( src, usedCS, "utf-8", false );
}

QByteArray encodeRFC2047String( const QString &src, const QByteArray &charset,
                                bool addressHeader, bool allow8BitHeaders )
{
  QByteArray encoded8Bit, result, usedCS;
  int start=0, end=0;
  bool nonAscii=false, ok=true, useQEncoding=false;
  QTextCodec *codec=0;

  usedCS = charset;
  codec = KGlobal::charsets()->codecForName( usedCS, ok );

  if ( !ok ) {
    //no codec available => try local8Bit and hope the best ;-)
    usedCS = KGlobal::locale()->encoding();
    codec = KGlobal::charsets()->codecForName( usedCS, ok );
  }

  if ( usedCS.contains( "8859-" ) ) { // use "B"-Encoding for non iso-8859-x charsets
    useQEncoding = true;
  }

  encoded8Bit = codec->fromUnicode( src );

  if ( allow8BitHeaders ) {
    return encoded8Bit;
  }

  uint encoded8BitLength = encoded8Bit.length();
  for ( unsigned int i=0; i<encoded8BitLength; i++ ) {
    if ( encoded8Bit[i] == ' ' ) { // encoding starts at word boundaries
      start = i + 1;
    }

    // encode escape character, for japanese encodings...
    if ( ( (signed char)encoded8Bit[i] < 0 ) || ( encoded8Bit[i] == '\033' ) ||
         ( addressHeader && ( strchr( "\"()<>@,.;:\\[]=", encoded8Bit[i] ) != 0 ) ) ) {
      end = start;   // non us-ascii char found, now we determine where to stop encoding
      nonAscii = true;
      break;
    }
  }

  if ( nonAscii ) {
    while ( ( end < encoded8Bit.length() ) && ( encoded8Bit[end] != ' ' ) ) {
      // we encode complete words
      end++;
    }

    for ( int x=end; x<encoded8Bit.length(); x++ ) {
      if ( ( (signed char)encoded8Bit[x]<0) || ( encoded8Bit[x] == '\033' ) ||
           ( addressHeader && ( strchr("\"()<>@,.;:\\[]=",encoded8Bit[x]) != 0 ) ) ) {
        end = encoded8Bit.length();     // we found another non-ascii word

        while ( ( end < encoded8Bit.length() ) && ( encoded8Bit[end] != ' ' ) ) {
          // we encode complete words
          end++;
        }
      }
    }

    result = encoded8Bit.left( start ) + "=?" + usedCS;

    if ( useQEncoding ) {
      result += "?Q?";

      char c, hexcode;// "Q"-encoding implementation described in RFC 2047
      for ( int i=start; i<end; i++ ) {
        c = encoded8Bit[i];
        if ( c == ' ' ) { // make the result readable with not MIME-capable readers
          result += '_';
        } else {
          if ( ( ( c >= 'a' ) && ( c <= 'z' ) ) || // paranoid mode, encode *all* special chars to avoid problems
              ( ( c >= 'A' ) && ( c <= 'Z' ) ) ||  // with "From" & "To" headers
              ( ( c >= '0' ) && ( c <= '9' ) ) ) {
            result += c;
          } else {
            result += '=';                 // "stolen" from KMail ;-)
            hexcode = ((c & 0xF0) >> 4) + 48;
            if ( hexcode >= 58 ) {
              hexcode += 7;
            }
            result += hexcode;
            hexcode = (c & 0x0F) + 48;
            if ( hexcode >= 58 ) {
              hexcode += 7;
            }
            result += hexcode;
          }
        }
      }
    } else {
      result += "?B?" + encoded8Bit.mid( start, end - start ).toBase64();
    }

    result +="?=";
    result += encoded8Bit.right( encoded8Bit.length() - end );
  } else {
    result = encoded8Bit;
  }

  return result;
}

QByteArray uniqueString()
{
  static char chars[] = "0123456789abcdefghijklmnopqrstuvxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  time_t now;
  char p[11];
  int pos, ran;
  unsigned int timeval;

  p[10] = '\0';
  now = time( 0 );
  ran = 1 + (int)(1000.0*rand() / (RAND_MAX + 1.0));
  timeval = (now / ran) + getpid();

  for ( int i=0; i<10; i++ ) {
    pos = (int) (61.0*rand() / (RAND_MAX + 1.0));
    //kDebug(5320) << pos;
    p[i] = chars[pos];
  }

  QByteArray ret;
  ret.setNum( timeval );
  ret += '.';
  ret += p;

  return ret;
}

QByteArray multiPartBoundary()
{
  return "nextPart" + uniqueString();
}

QByteArray unfoldHeader( const QByteArray &header )
{
  QByteArray result;
  int pos = 0, foldBegin = 0, foldMid = 0, foldEnd = 0;
  while ( ( foldMid = header.indexOf( '\n', pos ) ) >= 0 ) {
    foldBegin = foldEnd = foldMid;
    // find the first space before the line-break
    while ( foldBegin > 0 ) {
      if ( !QChar( header[foldBegin - 1] ).isSpace() ) {
        break;
      }
      --foldBegin;
    }
    // find the first non-space after the line-break
    while ( foldEnd <= header.length() - 1 ) {
      if ( !QChar( header[foldEnd] ).isSpace() ) {
        break;
      }
      ++foldEnd;
    }
    result += header.mid( pos, foldBegin - pos );
    if ( foldEnd < header.length() -1 )
      result += ' ';
    pos = foldEnd;
  }
  result += header.mid( pos, header.length() - pos );
  return result;
}

int indexOfHeader( const QByteArray &src, const QByteArray &name, int &end, int &dataBegin, bool *folded )
{
  QByteArray n = name;
  n.append( ':' );
  int begin = -1;

  if ( qstrnicmp( n.constData(), src.constData(), n.length() ) == 0 ) {
    begin = 0;
  } else {
    n.prepend('\n');
    const char *p = strcasestr( src.constData(), n.constData() );
    if ( !p ) {
      begin = -1;
    } else {
      begin = p - src.constData();
      ++begin;
    }
  }

  if ( begin > -1) {     //there is a header with the given name
    dataBegin = begin + name.length() + 1; //skip the name
    // skip the usual space after the colon
    if ( src.at( dataBegin ) == ' ' ) {
      ++dataBegin;
    }
    end = dataBegin;
    int len = src.length() - 1;
    if ( folded )
      *folded = false;

    if ( src.at(end) != '\n' ) {  // check if the header is not empty
      while ( true ) {
        end = src.indexOf( '\n', end + 1 );
        if ( end == -1 || end == len ||
             ( src[end+1] != ' ' && src[end+1] != '\t' ) ) {
          //break if we reach the end of the string, honor folded lines
          break;
        } else {
          if ( folded )
            *folded = true;
        }
      }
    }

    if ( end < 0 ) {
      end = len + 1; //take the rest of the string
    }
    return begin;

  } else {
    dataBegin = -1;
    return -1; //header not found
  }
}

QByteArray extractHeader( const QByteArray &src, const QByteArray &name )
{
  int begin, end;
  bool folded;
  indexOfHeader( src, name, end, begin, &folded );

  if ( begin >= 0 ) {
    if ( !folded ) {
      return src.mid( begin, end - begin );
    } else {
      QByteArray hdrValue = src.mid( begin, end - begin );
      return unfoldHeader( hdrValue );
    }
  } else {
    return QByteArray(); //header not found
  }
}

QList<QByteArray> extractHeaders( const QByteArray &src, const QByteArray &name )
{
  int begin, end;
  bool folded;
  QList<QByteArray> result;
  QByteArray copySrc( src );

  indexOfHeader( copySrc, name, end, begin, &folded );
  while ( begin >= 0 ) {
    if ( !folded ) {
      result.append( copySrc.mid( begin, end - begin ) );
    } else {
      QByteArray hdrValue = copySrc.mid( begin, end - begin );
      result.append( unfoldHeader( hdrValue ) );
    }

    // get the next one, a tiny bit ugly, but we don't want the previous to be found again...
    copySrc = copySrc.mid( end );
    indexOfHeader( copySrc, name, end, begin, &folded );
  }

  return result;
}

void removeHeader( QByteArray &header, const QByteArray &name )
{
  int begin, end, dummy;
  begin = indexOfHeader( header, name, end, dummy );
  if ( begin >= 0 ) {
    header.remove( begin, end - begin + 1 );
  }
}

QByteArray CRLFtoLF( const QByteArray &s )
{
  QByteArray ret = s;
  ret.replace( "\r\n", "\n" );
  return ret;
}

QByteArray LFtoCRLF( const QByteArray &s )
{
  QByteArray ret = s;
  ret.replace( "\n", "\r\n" );
  return ret;
}

namespace {
template < typename T > void removeQuotesGeneric( T & str )
{
  bool inQuote = false;
  for ( int i = 0; i < str.length(); ++i ) {
    if ( str[i] == '"' ) {
      str.remove( i, 1 );
      i--;
      inQuote = !inQuote;
    } else {
      if ( inQuote && ( str[i] == '\\' ) ) {
        str.remove( i, 1 );
      }
    }
  }
}
}

void removeQuots( QByteArray &str )
{
  removeQuotesGeneric( str );
}

void removeQuots( QString &str )
{
  removeQuotesGeneric( str );
}

void addQuotes( QByteArray &str, bool forceQuotes )
{
  bool needsQuotes=false;
  for ( int i=0; i < str.length(); i++ ) {
    if ( strchr("()<>@,.;:[]=\\\"", str[i] ) != 0 ) {
      needsQuotes = true;
    }
    if ( str[i] == '\\' || str[i] == '\"' ) {
      str.insert( i, '\\' );
      i++;
    }
  }

  if ( needsQuotes || forceQuotes ) {
    str.insert( 0, '\"' );
    str.append( "\"" );
  }
}

} // namespace KMime
