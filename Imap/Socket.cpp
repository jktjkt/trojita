/* Copyright (C) 2007 Jan Kundrát <jkt@gentoo.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "Imap/Socket.h"

namespace Imap {

// FIXME: delete in dtor?

IODeviceSocket::IODeviceSocket( QIODevice* device ): d(device)
{
    connect( d, SIGNAL(readyRead()), this, SIGNAL(readyRead()) );
    connect( d, SIGNAL(aboutToClose()), this, SIGNAL(aboutToClose()) );
}

bool IODeviceSocket::canReadLine()
{
    return d->canReadLine();
}

QByteArray IODeviceSocket::read( qint64 maxSize )
{
    return d->read( maxSize );
}

QByteArray IODeviceSocket::readLine( qint64 maxSize )
{
    return d->readLine( maxSize );
}

bool IODeviceSocket::waitForReadyRead( int msec )
{
    return d->waitForReadyRead( msec );
}

bool IODeviceSocket::waitForBytesWritten( int msec )
{
    return d->waitForBytesWritten( msec );
}

qint64 IODeviceSocket::write( const QByteArray& byteArray )
{
    return d->write( byteArray );
}

QIODevice* IODeviceSocket::device() const
{
    return d;
}

}

#include "Socket.moc"
