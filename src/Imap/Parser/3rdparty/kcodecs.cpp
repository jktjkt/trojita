/*
   Copyright (C) 2000-2001 Dawit Alemayehu <adawit@kde.org>
   Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License (LGPL)
   version 2 as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

   The encoding and decoding utilities in KCodecs with the exception of
   quoted-printable are based on the java implementation in HTTPClient
   package by Ronald Tschalär Copyright (C) 1996-1999.                          // krazy:exclude=copyright

   The quoted-printable codec as described in RFC 2045, section 6.7. is by
   Rik Hemsley (C) 2001.
*/

#include "kcodecs.h"

#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#define strcasecmp stricmp
#define strncasecmp  strnicmp
#endif

#include <stdlib.h>

//#include <kdebug.h>
#include <QtCore/QIODevice>

namespace KCodecs
{

static const char hexChars[16] =
{
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

static const unsigned int maxQPLineLength = 76;

} // namespace KCodecs


/******************************** KCodecs ********************************/
// strchr(3) for broken systems.
static int rikFindChar(register const char * _s, const char c)
{
  register const char * s = _s;

  while (true)
  {
    if ((0 == *s) || (c == *s)) break; ++s;
    if ((0 == *s) || (c == *s)) break; ++s;
    if ((0 == *s) || (c == *s)) break; ++s;
    if ((0 == *s) || (c == *s)) break; ++s;
  }

  return s - _s;
}

QByteArray KCodecs::quotedPrintableEncode(const QByteArray& in, bool useCRLF)
{
  QByteArray out;
  quotedPrintableEncode (in, out, useCRLF);
  return out;
}

void KCodecs::quotedPrintableEncode(const QByteArray& in, QByteArray& out, bool useCRLF)
{
  out.resize (0);
  if (in.isEmpty())
    return;

  char *cursor;
  const char *data;
  unsigned int lineLength;
  unsigned int pos;

  const unsigned int length = in.size();
  const unsigned int end = length - 1;


  // Reasonable guess for output size when we're encoding
  // mostly-ASCII data. It doesn't really matter, because
  // the underlying allocation routines are quite efficient,
  // but it's nice to have 0 allocations in many cases.
  out.resize ((length*12)/10);
  cursor = out.data();
  data = in.data();
  lineLength = 0;
  pos = 0;

  for (unsigned int i = 0; i < length; i++)
  {
    unsigned char c (data[i]);

    // check if we have to enlarge the output buffer, use
    // a safety margin of 16 byte
    pos = cursor-out.data();
    if (out.size()-pos < 16) {
      out.resize(out.size()+4096);
      cursor = out.data()+pos;
    }

    // Plain ASCII chars just go straight out.

    if ((c >= 33) && (c <= 126) && ('=' != c))
    {
      *cursor++ = c;
      ++lineLength;
    }

    // Spaces need some thought. We have to encode them at eol (or eof).

    else if (' ' == c)
    {
      if
        (
         (i >= length)
         ||
         ((i < end) && ((useCRLF && ('\r' == data[i + 1]) && ('\n' == data[i + 2]))
                        ||
                        (!useCRLF && ('\n' == data[i + 1]))))
        )
      {
        *cursor++ = '=';
        *cursor++ = '2';
        *cursor++ = '0';

        lineLength += 3;
      }
      else
      {
        *cursor++ = ' ';
        ++lineLength;
      }
    }
    // If we find a line break, just let it through.
    else if ((useCRLF && ('\r' == c) && (i < end) && ('\n' == data[i + 1])) ||
             (!useCRLF && ('\n' == c)))
    {
      lineLength = 0;

      if (useCRLF) {
        *cursor++ = '\r';
        *cursor++ = '\n';
        ++i;
      } else {
        *cursor++ = '\n';
      }
    }

    // Anything else is converted to =XX.

    else
    {
      *cursor++ = '=';
      *cursor++ = hexChars[c / 16];
      *cursor++ = hexChars[c % 16];

      lineLength += 3;
    }

    // If we're approaching the maximum line length, do a soft line break.

    if ((lineLength > maxQPLineLength) && (i < end))
    {
      if (useCRLF) {
        *cursor++ = '=';
        *cursor++ = '\r';
        *cursor++ = '\n';
      } else {
        *cursor++ = '=';
        *cursor++ = '\n';
      }

      lineLength = 0;
    }
  }

  out.truncate(cursor - out.data());
}

QByteArray KCodecs::quotedPrintableDecode(const QByteArray & in)
{
  QByteArray out;
  quotedPrintableDecode (in, out);
  return out;
}


void KCodecs::quotedPrintableDecode(const QByteArray& in, QByteArray& out)
{
  // clear out the output buffer
  out.resize (0);
  if (in.isEmpty())
      return;

  char *cursor;
  const unsigned int length = in.size();

  out.resize (length);
  cursor = out.data();

  for (unsigned int i = 0; i < length; i++)
  {
    char c(in[i]);

    if ('=' == c)
    {
      if (i < length - 2)
      {
        char c1 = in[i + 1];
        char c2 = in[i + 2];

        if (('\n' == c1) || ('\r' == c1 && '\n' == c2))
        {
          // Soft line break. No output.
          if ('\r' == c1)
            i += 2;        // CRLF line breaks
          else
            i += 1;
        }
        else
        {
          // =XX encoded byte.

          int hexChar0 = rikFindChar(hexChars, c1);
          int hexChar1 = rikFindChar(hexChars, c2);

          if (hexChar0 < 16 && hexChar1 < 16)
          {
            *cursor++ = char((hexChar0 * 16) | hexChar1);
            i += 2;
          }
        }
      }
    }
    else
    {
      *cursor++ = c;
    }
  }

  out.truncate(cursor - out.data());
}
