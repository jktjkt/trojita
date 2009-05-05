/*  -*- c++ -*-
    kmime_util.h

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
#ifndef __KMIME_UTIL_H__
#define __KMIME_UTIL_H__

#include <QtCore/QString>
#include "kmime_export.h"

namespace KMime {

/**
  Consult the charset cache. Only used for reducing mem usage by
  keeping strings in a common repository.
  @param name
*/
KMIME_EXPORT extern QByteArray cachedCharset( const QByteArray &name );

/**
  Consult the language cache. Only used for reducing mem usage by
  keeping strings in a common repository.
  @param name
*/
KMIME_EXPORT extern QByteArray cachedLanguage( const QByteArray &name );

/**
  Checks whether @p s contains any non-us-ascii characters.
  @param s
*/
KMIME_EXPORT extern bool isUsAscii( const QString &s );

//@cond PRIVATE
extern const uchar specialsMap[16];
extern const uchar tSpecialsMap[16];
extern const uchar aTextMap[16];
extern const uchar tTextMap[16];
extern const uchar eTextMap[16];

inline bool isOfSet( const uchar map[16], unsigned char ch )
{
  return ( ch < 128 ) && ( map[ ch/8 ] & 0x80 >> ch%8 );
}
inline bool isSpecial( char ch )
{
  return isOfSet( specialsMap, ch );
}
inline bool isTSpecial( char ch )
{
  return isOfSet( tSpecialsMap, ch );
}
inline bool isAText( char ch )
{
  return isOfSet( aTextMap, ch );
}
inline bool isTText( char ch )
{
  return isOfSet( tTextMap, ch );
}
inline bool isEText( char ch )
{
  return isOfSet( eTextMap, ch );
}
//@endcond

/**
  Decodes string @p src according to RFC2047,i.e., the construct
   =?charset?[qb]?encoded?=

  @param src       source string.
  @param usedCS    the detected charset is returned here
  @param defaultCS the charset to use in case the detected
                   one isn't known to us.
  @param forceCS   force the use of the default charset.

  @return the decoded string.
*/
KMIME_EXPORT extern QString decodeRFC2047String(
  const QByteArray &src, QByteArray &usedCS, const QByteArray &defaultCS = QByteArray(),
  bool forceCS = false );

/** Decode string @p src according to RFC2047 (ie. the
    =?charset?[qb]?encoded?= construct).

    @param src       source string.
    @return the decoded string.
*/
KMIME_EXPORT extern QString decodeRFC2047String( const QByteArray &src );

/**
  Encodes string @p src according to RFC2047 using charset @p charset.

  @param src           source string.
  @param charset       charset to use.
  @param addressHeader if this flag is true, all special chars
                       like <,>,[,],... will be encoded, too.
  @param allow8bitHeaders if this flag is true, 8Bit headers are allowed.

  @return the encoded string.
*/
KMIME_EXPORT extern QByteArray encodeRFC2047String(
  const QString &src, const QByteArray &charset, bool addressHeader=false,
  bool allow8bitHeaders=false );

/**
  Uses current time, pid and random numbers to construct a string
  that aims to be unique on a per-host basis (ie. for the local
  part of a message-id or for multipart boundaries.

  @return the unique string.
  @see multiPartBoundary
*/
KMIME_EXPORT extern QByteArray uniqueString();

/**
  Constructs a random string (sans leading/trailing "--") that can
  be used as a multipart delimiter (ie. as @p boundary parameter
  to a multipart/... content-type).

  @return the randomized string.
  @see uniqueString
*/
KMIME_EXPORT extern QByteArray multiPartBoundary();

/**
  Unfolds the given header if necessary.
  @param header The header to unfold.
*/
KMIME_EXPORT extern QByteArray unfoldHeader( const QByteArray &header );

/**
  Tries to extract the header with name @p name from the string
  @p src, unfolding it if necessary.

  @param src  the source string.
  @param name the name of the header to search for.

  @return the first instance of the header @p name in @p src
          or a null QCString if no such header was found.
*/
KMIME_EXPORT extern QByteArray extractHeader( const QByteArray &src,
                                 const QByteArray &name );

/**
  Tries to extract the headers with name @p name from the string
  @p src, unfolding it if necessary.

  @param src  the source string.
  @param name the name of the header to search for.

  @return all instances of the header @p name in @p src

  @since 4.2
*/
KMIME_EXPORT extern QList<QByteArray> extractHeaders( const QByteArray &src,
                                 const QByteArray &name );

/**
  Converts all occurrences of "\r\n" (CRLF) in @p s to "\n" (LF).

  This function is expensive and should be used only if the mail
  will be stored locally. All decode functions can cope with both
  line endings.

  @param s source string containing CRLF's

  @return the string with CRLF's substitued for LF's
  @see CRLFtoLF(const char*) LFtoCRLF
*/
KMIME_EXPORT extern QByteArray CRLFtoLF( const QByteArray &s );

/**
  Converts all occurrences of "\r\n" (CRLF) in @p s to "\n" (LF).

  This function is expensive and should be used only if the mail
  will be stored locally. All decode functions can cope with both
  line endings.

  @param s source string containing CRLF's

  @return the string with CRLF's substitued for LF's
  @see CRLFtoLF(const QCString&) LFtoCRLF
*/
KMIME_EXPORT extern QByteArray CRLFtoLF( const char *s );

/**
  Converts all occurrences of "\n" (LF) in @p s to "\r\n" (CRLF).

  This function is expensive and should be used only if the mail
  will be transmitted as an RFC822 message later. All decode
  functions can cope with and all encode functions can optionally
  produce both line endings, which is much faster.

  @param s source string containing CRLF's

  @return the string with CRLF's substitued for LF's
  @see CRLFtoLF(const QCString&) LFtoCRLF
*/
KMIME_EXPORT extern QByteArray LFtoCRLF( const QByteArray &s );

/**
  Removes quote (DQUOTE) characters and decodes "quoted-pairs"
  (ie. backslash-escaped characters)

  @param str the string to work on.
  @see addQuotes
*/
KMIME_EXPORT extern void removeQuots( QByteArray &str );

/**
  Removes quote (DQUOTE) characters and decodes "quoted-pairs"
  (ie. backslash-escaped characters)

  @param str the string to work on.
  @see addQuotes
*/
KMIME_EXPORT extern void removeQuots( QString &str );

/**
  Converts the given string into a quoted-string if the string contains
  any special characters (ie. one of ()<>@,.;:[]=\").

  @param str us-ascii string to work on.
  @param forceQuotes if @p true, always add quote characters.
*/
KMIME_EXPORT extern void addQuotes( QByteArray &str, bool forceQuotes );

} // namespace KMime

#endif /* __KMIME_UTIL_H__ */
