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
#include <QTime>
#include <QDebug>

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>

namespace Imap {

UnixSocket::UnixSocket( const QList<QByteArray>& args ): d(new UnixSocketThread(args)), hasLine(false)
{
    connect( d, SIGNAL(readyRead()), this, SIGNAL(readyRead()), Qt::QueuedConnection );
    connect( d, SIGNAL(aboutToClose()), this, SIGNAL(aboutToClose()), Qt::QueuedConnection );
    d->start();
}

UnixSocket::~UnixSocket()
{
    terminate();
    d->wait();
    delete d;
}

/** @short Check if we can read the whole line without blocking
 *
 * Data doesn't have to be stored in our internal buffer.
 * */
bool UnixSocket::canReadLine()
{
    if ( hasLine ) {
        return true;
    }
    if ( buffer.indexOf( QByteArray("\r\n") ) != -1 ) {
        return hasLine = true;
    }
    // We'll have to check the socket and read some reasonable size to be able
    // to tell if we can read stuff
    // FIXME: this should be implemented with checking for CRLF
    while ( waitForReadyRead( 0 ) && buffer.size() < 8192 ) {
        buffer += reallyRead( 8192 );
    }
    if ( buffer.indexOf( QByteArray("\r\n") ) != -1 ) {
        return hasLine = true;
    } else {
        return false;
    }
}

/** @short Retrieve data from stdout
 *
 * If there's some data in our internal buffer, we return everything (up to
 * maxSize) from it. No real read() to the underlying socket is performed.
 *
 * If there are no data in there, we invoke the real read() syscall. If there
 * are no data waiting in the OS buffer, this call will block. If there's
 * something, we return an unspecified portion of data. The only guarantee is
 * that 0 < size <= maxSize.
 * */
QByteArray UnixSocket::read( qint64 maxSize )
{
    if ( ! buffer.isEmpty() ) {
        QByteArray tmp = buffer.left( maxSize );
        buffer = buffer.right( buffer.size() - tmp.size() );
        hasLine = ( buffer.indexOf( QByteArray("\r\n") ) != -1 );
        return tmp;
    }
    return reallyRead( maxSize );
}

/** @short Helper function for read()
 *
 * This function maintains the internal semaphore used for starting/suspending
 * the background thread doing select()s.
 * */
QByteArray UnixSocket::reallyRead( qint64 maxSize )
{
    QByteArray buf;
    buf.resize( maxSize );
    qint64 ret = wrappedRead( d->fdStdout[0], buf.data(), maxSize );
    buf.resize( ret );
    d->readyReadAlreadyDone.release();
    return buf;
}

/** @short Returns a line from the stdout
 *
 * This function will block till there are enough data to read. The argument is
 * ignored, yet.
 * */
QByteArray UnixSocket::readLine( qint64 maxSize )
{
    // FIXME: maxSize
    while ( ! canReadLine() ) {
        waitForReadyRead( -1 );
    }
    int pos = buffer.indexOf( "\r\n" ); // FIXME: keep track of the offset
    QByteArray tmp = buffer.left( pos + 2 );
    buffer = buffer.right( buffer.size() - tmp.size() );
    hasLine = ( buffer.indexOf( QByteArray("\r\n") ) != -1 );
    return tmp;
}

/** @short Waits till there are some data to be read from the socket
 *
 * Any data we might have in the internal buffer are ignored.
 * */
bool UnixSocket::waitForReadyRead( int msec )
{
    fd_set rfds;
    struct timeval tv;
    int ret;

    FD_ZERO( &rfds );
    FD_SET( d->fdStdout[0], &rfds );
    tv.tv_sec = msec / 1000;
    tv.tv_usec = 1000 * (msec % 1000 );
    do {
        ret = select( d->fdStdout[0] + 1, &rfds, 0, 0, msec < 0 ? 0 : &tv );
    } while ( ret == -1 && errno == EINTR );
    if ( ret == 0 ) {
        // timeout
        return false;
    } else if ( ret > 0 ) {
        if ( FD_ISSET( d->fdStdout[0], &rfds ) ) {
            return true;
        } else {
            qDebug() << "select(): wtf, got nothing?";
            return false;
        }
    } else {
        qDebug() << "wfrr: select failed, errno" << errno << d->fdStdout[0] + 1 << &rfds << tv.tv_sec << tv.tv_usec;
        Q_ASSERT(false);
        return false;
    }
}

/** @short Doesn't do anything, as we don't buffer writes */
bool UnixSocket::waitForBytesWritten( int msec )
{
    // it isn't buffered
    return true;
}

/** @short Write data to the underlying socket */
qint64 UnixSocket::write( const QByteArray& byteArray )
{
    return wrappedWrite( d->fdStdin[1], byteArray.constData(), byteArray.size() );
}

/** @short Safe wrapper around syscall that ignores interruptions by signals */
ssize_t UnixSocket::wrappedRead( int fd, void* buf, size_t count )
{
    ssize_t ret = 0;
    do {
        ret = ::read(fd, buf, count);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

/** @short Safe wrapper around syscall that ignores interruptions by signals */
ssize_t UnixSocket::wrappedWrite( int fd, const void* buf, size_t count )
{
    ssize_t ret = 0;
    do {
        ret = ::write(fd, buf, count);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

/** @short Safe wrapper around syscall that ignores interruptions by signals */
int UnixSocket::wrappedPipe( int pipefd[2] )
{
    int ret;
    do {
        ret = ::pipe( pipefd );
    } while ( ret == -1 && errno == EAGAIN );
    return ret;
}

/** @short Safe wrapper around syscall that ignores interruptions by signals */
int UnixSocket::wrappedClose( int fd )
{
    int ret;
    do {
        ret = ::close( fd );
    } while ( ret == -1 && errno == EINTR );
    return ret;
}

/** @short Safe wrapper around syscall that ignores interruptions by signals */
int UnixSocket::wrappedDup2( int oldfd, int newfd )
{
    int ret;
    do {
        ret = ::dup2( oldfd, newfd );
    } while ( ret == -1 && ( errno == EINTR || errno == EBUSY ) );
    return ret;
}

void UnixSocket::terminate()
{
    if ( d->fdStdin[1] >= 0 ) {
        wrappedClose( d->fdStdin[1] );
        d->fdStdin[1] = -1;
    }
    if ( d->fdStdout[0] >= 0 ) {
        wrappedClose( d->fdStdout[0] );
        d->fdStdout[0] = -1;
    }
    if ( d->childPid ) {
        ::kill( d->childPid, SIGTERM );
        d->childPid = 0;
    }
    d->readyReadAlreadyDone.release();
}



UnixSocketThread::UnixSocketThread( const QList<QByteArray>& args ): childPid(0)
{
    int ret = UnixSocket::wrappedPipe( fdStdout );
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
}

void UnixSocketThread::run()
{
    fd_set rfds;
    int ret;
    while (true) {
        FD_ZERO( &rfds );
        FD_SET( fdStdout[0], &rfds );
        do {
            ret = select( fdStdout[0] + 1, &rfds, 0, 0, 0 );
        } while ( ret == -1 && errno == EINTR );
        if ( ret < 0 ) {
            // select() failed
            qDebug() << "select() failed:" << errno;
        } else if ( ret > 0 ) {
            if ( FD_ISSET( fdStdout[0], &rfds ) ) {
                emit readyRead();
                // we don't want to loop around select() till the other threads
                // handle the signal...
                readyReadAlreadyDone.acquire();
            }
        }
    }
}

}

#include "UnixSocket.moc"
