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

#define KMIME_EXPORT

namespace KMime {

#if 0
/**
  Checks whether @p s contains any non-us-ascii characters.
  @param s
*/
KMIME_EXPORT extern bool isUsAscii( const QString &s );

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
#endif

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

} // namespace KMime

#endif /* __KMIME_UTIL_H__ */
