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

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QTextCodec>

namespace KMime {

#if 0
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
#endif

QByteArray encodeRFC2047String( const QString &src, const QByteArray &charset,
                                bool addressHeader, bool allow8BitHeaders )
{
  QByteArray encoded8Bit, result, usedCS;
  int start=0, end=0;
  bool nonAscii=false, useQEncoding=false;
  QTextCodec *codec= QTextCodec::codecForName( charset );

  usedCS = charset;
  /*codec = KGlobal::charsets()->codecForName( usedCS, ok );

  if ( !ok ) {
    //no codec available => try local8Bit and hope the best ;-)
    usedCS = KGlobal::locale()->encoding();
    codec = KGlobal::charsets()->codecForName( usedCS, ok );
  }*/

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

} // namespace KMime
