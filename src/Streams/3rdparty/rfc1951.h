/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Messaging Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef STREAMS_RFC1951_H
#define STREAMS_RFC1951_H

#include <QIODevice>

#include <zlib.h>

namespace Streams {

/* From RFC4978 The IMAP COMPRESS:   
   "When using the zlib library (see [RFC1951]), the functions
   deflateInit2(), deflate(), inflateInit2(), and inflate() suffice to
   implement this extension.  The windowBits value must be in the range
   -8 to -15, or else deflateInit2() uses the wrong format.
   deflateParams() can be used to improve compression rate and resource
   use.  The Z_FULL_FLUSH argument to deflate() can be used to clear the
   dictionary (the receiving peer does not need to do anything)."
   
   Total zlib mem use is 176KB plus a 'few kilobytes' per connection that uses COMPRESS:
   96KB for deflate, 24KB for 3x8KB buffers, 32KB plus a 'few' kilobytes for inflate.
*/

class Rfc1951Compressor
{
public:
    explicit Rfc1951Compressor(int chunkSize = 8192);
    ~Rfc1951Compressor();

    bool write(QIODevice *out, QByteArray *in);

private:
    int _chunkSize;
    z_stream _zStream;
    char *_buffer;
};

class Rfc1951Decompressor
{
public:
    explicit Rfc1951Decompressor(int chunkSize = 8192);
    ~Rfc1951Decompressor();

    bool consume(QIODevice *in);
    bool canReadLine() const;
    QByteArray readLine();
    QByteArray read(qint64 maxSize);

private:
    int _chunkSize;
    z_stream _zStream;
    QByteArray _inBuffer;
    char *_stagingBuffer;
    QByteArray _output;
};

}

#endif
