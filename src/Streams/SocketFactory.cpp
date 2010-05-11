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

namespace Imap {
namespace Mailbox {

SocketFactory::SocketFactory(): _startTls(false)
{
}

void SocketFactory::setStartTlsRequired( const bool doIt )
{
    _startTls = doIt;
}

bool SocketFactory::startTlsRequired()
{
    return _startTls;
}

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
    return Imap::SocketPtr( new IODeviceSocket( proc ) );
}

SslSocketFactory::SslSocketFactory( const QString& host, const quint16 port ):
    _host(host), _port(port)
{
}

Imap::SocketPtr SslSocketFactory::create()
{
    QSslSocket* sslSock = new QSslSocket();
    sslSock->ignoreSslErrors(); // big fat FIXME here!!!
    sslSock->setProtocol( QSsl::AnyProtocol );
    sslSock->setPeerVerifyMode( QSslSocket::QueryPeer );
    IODeviceSocket* ioSock = new IODeviceSocket( sslSock, true );
    connect( sslSock, SIGNAL(encrypted()), ioSock, SIGNAL(connected()) );
    sslSock->connectToHostEncrypted( _host, _port );
    return Imap::SocketPtr( ioSock );
}

TlsAbleSocketFactory::TlsAbleSocketFactory( const QString& host, const quint16 port ):
    _host(host), _port(port)
{
}

Imap::SocketPtr TlsAbleSocketFactory::create()
{
    QSslSocket* sslSock = new QSslSocket();
    sslSock->ignoreSslErrors(); // big fat FIXME here!!!
    sslSock->setProtocol( QSsl::AnyProtocol );
    sslSock->setPeerVerifyMode( QSslSocket::QueryPeer );
    sslSock->connectToHost( _host, _port );
    return Imap::SocketPtr( new IODeviceSocket( sslSock ) );
}


}
}
