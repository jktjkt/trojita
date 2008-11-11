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
#ifndef IMAP_UNIX_SOCKET_H
#define IMAP_UNIX_SOCKET_H

#include <QThread>
#include <QSemaphore>
#include <unistd.h>
#include "Socket.h"

namespace Imap {

    class UnixSocketThread;

    class UnixSocket: public Socket {
        Q_OBJECT

    public:
        UnixSocket( const QList<QByteArray>& args );
        ~UnixSocket();
        virtual bool canReadLine();
        virtual QByteArray read( qint64 maxSize );
        virtual QByteArray readLine( qint64 maxSize = 0 );
        virtual bool waitForReadyRead( int msec );
        virtual bool waitForBytesWritten( int msec );
        virtual qint64 write( const QByteArray& byteArray );

    signals:
        void aboutToClose();
        void readyRead();

    private:
        UnixSocketThread* d;
        QByteArray buffer;
        bool hasLine;

        void pauseThread();

        QByteArray reallyRead( qint64 maxSize );

        friend class UnixSocketThread;
        static ssize_t wrappedRead( int fd, void* buf, size_t count );
        static ssize_t wrappedWrite( int fd, const void* buf, size_t count );
        static int wrappedPipe( int pipefd[2] );
        static int wrappedClose( int fd );
        static int wrappedDup2( int oldfd, int newfd );
    };

    class UnixSocketThread: public QThread {
        Q_OBJECT

    public:
        UnixSocketThread( const QList<QByteArray>& args );
        ~UnixSocketThread();

    protected:
        virtual void run();

    signals:
        void aboutToClose();
        void readyRead();

    private:
        friend class UnixSocket;
        QSemaphore readyReadAlreadyDone;

        int fdInternalPipe[2];
        int fdStdin[2];
        int fdStdout[2];
        pid_t childPid;
    };

};

#endif /* IMAP_UNIX_SOCKET_H */
