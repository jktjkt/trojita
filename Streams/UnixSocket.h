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
    class UnixSocketDeadWatcher;

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
        virtual void startTls();

    private:
        UnixSocketThread* d;
        QByteArray buffer;
        bool hasLine;

        QByteArray reallyRead( qint64 maxSize );
        void terminate();

        friend class UnixSocketThread;
        friend class UnixSocketDeadWatcher;
        static ssize_t wrappedRead( int fd, void* buf, size_t count );
        static ssize_t wrappedWrite( int fd, const void* buf, size_t count );
        static int wrappedPipe( int pipefd[2] );
        static int wrappedClose( int fd );
        static int wrappedDup2( int oldfd, int newfd );
        static pid_t wrappedWaitpid(pid_t pid, int *status, int options);
    };

    class UnixSocketThread: public QThread {
        Q_OBJECT

    public:
        UnixSocketThread( const QList<QByteArray>& args );

    protected:
        virtual void run();

    signals:
        void readyRead();
        void readChannelFinished();

    private:
        friend class UnixSocket;
        friend class UnixSocketDeadWatcher;
        QSemaphore readyReadAlreadyDone;

        int fdStdin[2];
        int fdStdout[2];
        int fdExitPipe[2];
        pid_t childPid;
    };

};

#endif /* IMAP_UNIX_SOCKET_H */
