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
#include <QDebug>
#include <errno.h>
#include <stdlib.h>

namespace Imap {

UnixSocket::UnixSocket( const QList<QByteArray>& args ): d(new UnixSocketThread(args))
{
    connect( d, SIGNAL(readyRead()), this, SIGNAL(readyRead()), Qt::QueuedConnection );
    connect( d, SIGNAL(aboutToClose()), this, SIGNAL(aboutToClose()), Qt::QueuedConnection );
    d->start();
}

UnixSocket::~UnixSocket()
{
    delete d;
}

bool UnixSocket::canReadLine()
{
    qDebug() << "UnixSocket::canReadLine()";
    return false; // FIXME
}

QByteArray UnixSocket::read( qint64 maxSize )
{
    qDebug() << "UnixSocket::read(" << maxSize << ")";
    //FIXME: internal buffer!
    pauseThread();
    QByteArray buf;
    buf.resize( maxSize );
    qint64 ret = wrappedRead( d->fdStdout[0], buf.data(), maxSize );
    buf.resize( ret );
    d->selectSemaphore.release();
    qDebug() << "released SELECT";
    qDebug() << "UnixSocket::read(): return" << buf.size() << "bytes";
    return buf;

}

QByteArray UnixSocket::readLine( qint64 maxSize )
{
    qDebug() << "UnixSocket::readLine(" << maxSize << ")";
    return QByteArray(); // FIXME
}

bool UnixSocket::waitForReadyRead( int msec )
{
    qDebug() << "UnixSocket::waitForReadyRead(" << msec << ")";
    return true; // FIXME
}

bool UnixSocket::waitForBytesWritten( int msec )
{
    qDebug() << "UnixSocket::waitForBytesWritten(" << msec << ")";
    return true;
}

qint64 UnixSocket::write( const QByteArray& byteArray )
{
    qDebug() << "UnixSocket::write(" << byteArray.size() << "bytes)";
    pauseThread();
    qint64 ret = wrappedWrite( d->fdStdin[1], byteArray.constData(), byteArray.size() );
    d->selectSemaphore.release();
    qDebug() << "released SELECT";
    qDebug() << "UnixSocket::write(): return" << ret;
    return ret;
}

void UnixSocket::pauseThread()
{
    qDebug() << "UnixSocket::pauseThread()";
    int ret = wrappedWrite( d->fdInternalPipe[1], "x", 1 );
    qDebug() << "UnixSocket::pauseThread(): write() returned" << ret;
    qDebug() << "...acquiring ACCESS...";
    d->accessSemaphore.acquire();
    qDebug() << "got ACCESS";
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

int UnixSocket::wrappedPipe( int pipefd[2] )
{
    int ret;
    do {
        ret = ::pipe( pipefd );
    } while ( ret == -1 && errno == EAGAIN );
    return ret;
}

int UnixSocket::wrappedClose( int fd )
{
    int ret;
    do {
        ret = ::close( fd );
    } while ( ret == -1 && errno == EINTR );
    return ret;
}

int UnixSocket::wrappedDup2( int oldfd, int newfd )
{
    int ret;
    do {
        ret = ::dup2( oldfd, newfd );
    } while ( ret == -1 && ( errno == EINTR || errno == EBUSY ) );
    return ret;
}


UnixSocketThread::UnixSocketThread( const QList<QByteArray>& args )
{
    int ret = UnixSocket::wrappedPipe( fdInternalPipe );
    if ( ret == -1 ) {
        QByteArray buf;
        QTextStream ss( &buf );
        ss << "UnixSocketThread: Can't create internal pipe: " << errno;
        ss.flush();
        throw SocketException( buf.constData() );
    }
    
    ret = UnixSocket::wrappedPipe( fdStdout );
    if ( ret == -1 ) {
        QByteArray buf;
        QTextStream ss( &buf );
        ss << "UnixSocketThread: Can't create stdout pipe: " << errno;
        ss.flush();
        throw SocketException( buf.constData() );
    }

    ret = UnixSocket::wrappedPipe( fdStdin );
    if ( ret == -1 ) {
        QByteArray buf;
        QTextStream ss( &buf );
        ss << "UnixSocketThread: Can't create stdin pipe: " << errno;
        ss.flush();
        throw SocketException( buf.constData() );
    }


    childPid = fork();
    while ( childPid && errno == EAGAIN )
        childPid = fork();

    if ( childPid == 0 ) {
        // I'm the child
        if ( UnixSocket::wrappedDup2( fdStdin[0], 0 ) == -1 ) {
            QByteArray buf;
            QTextStream ss( &buf );
            ss << "UnixSocketThread: Can't dup2(stdin): " << errno;
            ss.flush();
            throw SocketException( buf.constData() );
        }
        if ( UnixSocket::wrappedDup2( fdStdout[1], 1 ) == -1 ) {
            QByteArray buf;
            QTextStream ss( &buf );
            ss << "UnixSocketThread: Can't dup2(stdout): " << errno;
            ss.flush();
            throw SocketException( buf.constData() );
        }
        // FIXME: error checking?
        UnixSocket::wrappedClose( fdStdin[0] );
        UnixSocket::wrappedClose( fdStdin[1] );
        UnixSocket::wrappedClose( fdStdout[0] );
        UnixSocket::wrappedClose( fdStdout[1] );
        do {
            // yuck
            char **argv = new char *[args.size() + 1];
            for ( int i = 0; i < args.size(); ++i )
                argv[i] = ::strdup( args[i].constData() );
            argv[args.size()] = 0;
            ret = ::execv( args[0].constData(), argv );
            delete[] argv;
        } while ( ret == -1 && errno == EAGAIN );
        QByteArray buf;
        QTextStream ss( &buf );
        ss << "UnixSocketThread: execv() failed: " << errno;
        ss.flush();
        throw SocketException( buf.constData() );
    } else if ( childPid == -1 ) {
        // still failed
        QByteArray buf;
        QTextStream ss( &buf );
        ss << "UnixSocketThread: Can't fork: " << errno;
        ss.flush();
        throw SocketException( buf.constData() );
    } else {
        // I'm a parent
    }

    selectSemaphore.release();
}

void UnixSocketThread::run()
{
    fd_set rfds;
    struct timeval tv;
    int ret;
    while (true) {
        qDebug() << "acquirung SELECT...";
        selectSemaphore.acquire();
        qDebug() << "got SELECT semaphore";
        while (true) {
            FD_ZERO( &rfds );
            FD_SET( fdInternalPipe[0], &rfds );
            FD_SET( fdStdout[0], &rfds );
            tv.tv_sec = 2;
            tv.tv_usec = 0;
            do {
                ret = select( qMax( fdStdout[0], fdInternalPipe[0] ) + 1, &rfds, 0, 0, &tv );
            } while ( ret == -1 && errno == EINTR );
            if ( ret < 0 ) {
                // select() failed
                qDebug() << "select() failed";
            } else if ( ret == 0 ) {
                // timeout
                qDebug() << "select(): timeout";
            } else {
                if ( FD_ISSET( fdInternalPipe[0], &rfds ) ) {
                    qDebug() << "select(): big brother wants us to sleep...";
                    break;
                } else if ( FD_ISSET( fdStdout[0], &rfds ) ) {
                    qDebug() << "select(): got some data";
                    emit readyRead();
                } else {
                    qDebug() << "select(): wtf, got nothing?";
                }
            }
        }
        qDebug() << "releaseing ACCESS";
        accessSemaphore.release();
    }
}

UnixSocketThread::~UnixSocketThread()
{
    // FIXME: kill the son
}

}

#include "UnixSocket.moc"
