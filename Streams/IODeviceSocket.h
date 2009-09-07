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
#ifndef IMAP_IODEVICE_SOCKET_H
#define IMAP_IODEVICE_SOCKET_H

#include <QAbstractSocket>
#include <QProcess>
#include "Socket.h"

namespace Imap {

    class IODeviceSocket: public Socket {
        Q_OBJECT
    public:
        IODeviceSocket( QIODevice* device );
        ~IODeviceSocket();
        virtual bool canReadLine();
        virtual QByteArray read( qint64 maxSize );
        virtual QByteArray readLine( qint64 maxSize = 0 );
        virtual bool waitForReadyRead( int msec );
        virtual bool waitForBytesWritten( int msec );
        virtual qint64 write( const QByteArray& byteArray );
        virtual void startTls();
        virtual bool isDead();
    private slots:
        void handleStateChanged();
        void handleSocketError( QAbstractSocket::SocketError );
        void handleProcessError( QProcess::ProcessError );
    private:
        QIODevice* d;
    };

};

#endif /* IMAP_IODEVICE_SOCKET_H */
