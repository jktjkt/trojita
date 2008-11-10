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

#include "UnixSocket.h"
#include "../Exceptions.h"
#include <QTextStream>
#include <errno.h>
#include <stdlib.h>

namespace Imap {

UnixSocket::UnixSocket( const QList<QByteArray>& args ): d(new UnixSocketThread(args))
{
    connect( d, SIGNAL(readyRead()), this, SIGNAL(readyRead()), Qt::QueuedConnection );
    connect( d, SIGNAL(aboutToClose()), this, SIGNAL(aboutToClose()), Qt::QueuedConnection );
}

UnixSocket::~UnixSocket()
{
    delete d;
}

bool UnixSocket::canReadLine()
{
    return false; // FIXME
}

QByteArray UnixSocket::read( qint64 maxSize )
{
    //FIXME: internal buffer!
    pauseThread();
    d->accessSemaphore.acquire();
    QByteArray buf;
    buf.resize( maxSize );
    qint64 ret = wrappedRead( d->fdProcess[0], buf.data(), maxSize );
    buf.resize( ret );
    d->selectSemaphore.release();
    return buf;

}

QByteArray UnixSocket::readLine( qint64 maxSize )
{
    return QByteArray(); // FIXME
}

bool UnixSocket::waitForReadyRead( int msec )
{
    return true; // FIXME
}

bool UnixSocket::waitForBytesWritten( int msec )
{
    return true;
}

qint64 UnixSocket::write( const QByteArray& byteArray )
{
    pauseThread();
    d->accessSemaphore.acquire();
    qint64 ret = wrappedWrite( d->fdProcess[1], byteArray.constData(), byteArray.size() );
    d->selectSemaphore.release();
    return ret;
}

void UnixSocket::pauseThread()
{
    while ( -1 == wrappedWrite( d->fdInternalPipe[1], "x", 1 ) );
}

ssize_t UnixSocket::wrappedRead( int fd, void* buf, size_t count )
{
    ssize_t ret = 0;
    do {
        ret = ::read(fd, buf, count);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

ssize_t UnixSocket::wrappedWrite( int fd, const void* buf, size_t count )
{
    ssize_t ret = 0;
    do {
        ret = ::write(fd, buf, count);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

int UnixSocket::wrappedPipe(int pipefd[2])
{
    int ret;
    do {
        ret = ::pipe( pipefd );
    } while ( ret == -1 && errno == EAGAIN );
    return ret;
}


UnixSocketThread::UnixSocketThread( const QList<QByteArray>& args )
{
    int ret = UnixSocket::wrappedPipe( fdInternalPipe );
    if ( ret == -1 ) {
        QByteArray buf;
        QTextStream ss( &buf );
        ss << "UnixSocketThread: Can't create internal pipe (" << errno << ")";
        ss.flush();
        throw SocketException( buf.constData() );
    }

    pid_t child = fork();
    while ( child == -1 && errno == EAGAIN )
        child = fork();

    if ( child == 0 ) {
        // I'm a parent
    } else if ( child == -1 ) {
        // still failed
        QByteArray buf;
        QTextStream ss( &buf );
        ss << "UnixSocketThread: Can't fork (" << errno << ")";
        ss.flush();
        throw SocketException( buf.constData() );
    } else {
        // I'm the child
        do {
            // yuck
            char **argv = new char *[args.size() + 1];
            for ( int i = 0; i < args.size(); ++i )
                argv[i] = ::strdup( args[i].constData() );
            argv[args.size()] = 0;
            ret = ::execv( args[0].constData(), argv );
            delete[] argv;
        } while ( ret == -1 && errno == EAGAIN );
    }

    // FIXME: do something with pipes here

    selectSemaphore.release();
}

void UnixSocketThread::run()
{
    while (true) {
        selectSemaphore.acquire();
        // fdInternalPipe[0] is for reading
        // FIXME: select() goes here
        accessSemaphore.release();
    }
}

}

#include "UnixSocket.moc"
