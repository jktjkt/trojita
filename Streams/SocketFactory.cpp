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

#include <QProcess>
#include <QSslSocket>
#include "SocketFactory.h"
#include "IODeviceSocket.h"
#include "UnixSocket.h"

namespace Imap {
namespace Mailbox {

ProcessSocketFactory::ProcessSocketFactory(
        const QString& executable, const QStringList& args):
    _executable(executable), _args(args)
{
}

Imap::SocketPtr ProcessSocketFactory::create()
{
    // FIXME: this may leak memory if an exception strikes in this function
    // (before we return the pointer)
    QProcess* proc = new QProcess();
    proc->start( _executable, _args );
    if ( ! proc->waitForStarted() )
        return Imap::SocketPtr( 0 );

    return Imap::SocketPtr( new IODeviceSocket( proc ) );
}

UnixProcessSocketFactory::UnixProcessSocketFactory(
        const QString& executable, const QStringList& args)
{
    _argv << executable.toLocal8Bit();
    for ( QStringList::const_iterator it = args.begin(); it != args.end(); ++it )
        _argv << it->toLocal8Bit();
}

Imap::SocketPtr UnixProcessSocketFactory::create()
{
    return Imap::SocketPtr( new UnixSocket( _argv ) );
}

SslSocketFactory::SslSocketFactory( const QString& host, const quint16 port ):
    _host(host), _port(port)
{
}

Imap::SocketPtr SslSocketFactory::create()
{
    QSslSocket* sslSock = new QSslSocket();
    sslSock->ignoreSslErrors(); // big fat FIXME here!!!
    sslSock->setPeerVerifyMode( QSslSocket::QueryPeer );
    sslSock->connectToHostEncrypted( _host, _port );
    if ( ! sslSock->waitForEncrypted() ) {
        qDebug() << "Encrypted connection failed";
        QList<QSslError> e = sslSock->sslErrors();
        for ( QList<QSslError>::const_iterator it = e.begin(); it != e.end(); ++it ) {
            qDebug() << *it;
        }
    }
    // FIXME: handle & signal errors
    return Imap::SocketPtr( new IODeviceSocket( sslSock ) );
}

TlsAbleSocketFactory::TlsAbleSocketFactory( const QString& host, const quint16 port ):
    _host(host), _port(port)
{
}

Imap::SocketPtr TlsAbleSocketFactory::create()
{
    QSslSocket* sslSock = new QSslSocket();
    sslSock->ignoreSslErrors(); // big fat FIXME here!!!
    sslSock->setPeerVerifyMode( QSslSocket::QueryPeer );
    sslSock->connectToHost( _host, _port );
    if ( ! sslSock->waitForConnected() ) {
        qDebug() << "Connection failed:" << sslSock->errorString();
    }
    // FIXME: handle & signal errors
    return Imap::SocketPtr( new IODeviceSocket( sslSock ) );
}


}
}
