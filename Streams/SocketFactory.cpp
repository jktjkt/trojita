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
    if ( ! proc->waitForStarted() ) {
        switch( proc->error() ) {
            case QProcess::FailedToStart:
                emit error( tr( "The IMAP process failed to start" ) );
                break;
            case QProcess::Crashed:
                emit error( tr( "The IMAP process has crashed" ) );
                break;
            case QProcess::Timedout:
                emit error( tr( "The IMAP process didn't start in a timely fashion" ) );
                break;
            default:
                emit error( tr( "Unknown error occured while starting the child process" ) );
        }
    }
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
    sslSock->connectToHostEncrypted( _host, _port );
    if ( ! sslSock->waitForEncrypted() ) {
        QString err = tr( "Encrypted connection failed" );
        QList<QSslError> e = sslSock->sslErrors();
        for ( QList<QSslError>::const_iterator it = e.begin(); it != e.end(); ++it ) {
            err.append( QLatin1String( "\r\n" ) );
            err.append( it->errorString() );
        }
        emit error( err );
    }
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
    sslSock->setProtocol( QSsl::AnyProtocol );
    sslSock->setPeerVerifyMode( QSslSocket::QueryPeer );
    sslSock->connectToHost( _host, _port );
    if ( ! sslSock->waitForConnected() ) {
        emit error( tr( "Connection failed: %1").arg( sslSock->errorString() ) );
    }
    return Imap::SocketPtr( new IODeviceSocket( sslSock ) );
}


}
}

#include "SocketFactory.moc"
