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
#ifndef IMAP_SOCKET_H
#define IMAP_SOCKET_H

#include <memory>
#include <QAbstractSocket>

namespace Imap {

    class Socket: public QObject {
        Q_OBJECT
    public:
        virtual bool canReadLine() = 0;
        virtual QByteArray read( qint64 maxSize ) = 0;
        virtual QByteArray readLine( qint64 maxSize = 0 ) = 0;
        virtual bool waitForReadyRead( int msec ) = 0;
        virtual bool waitForBytesWritten( int msec ) = 0;
        virtual qint64 write( const QByteArray& byteArray ) = 0;
        virtual void startTls() = 0;
        virtual bool isDead() = 0;
        virtual ~Socket() {};
    signals:
        void disconnected( const QString );
        void readyRead();
    };

    typedef std::auto_ptr<Socket> SocketPtr;

};

#endif /* IMAP_SOCKET_H */
