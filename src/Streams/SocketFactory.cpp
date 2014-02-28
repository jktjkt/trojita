/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "SocketFactory.h"
#include <stdexcept>
#include <QProcess>
#include <QSslSocket>
#include "IODeviceSocket.h"
#include "FakeSocket.h"

namespace Streams {

SocketFactory::SocketFactory(): m_startTls(false)
{
}

void SocketFactory::setStartTlsRequired(const bool doIt)
{
    m_startTls = doIt;
}

bool SocketFactory::startTlsRequired()
{
    return m_startTls;
}

ProcessSocketFactory::ProcessSocketFactory(
    const QString &executable, const QStringList &args):
    executable(executable), args(args)
{
}

Socket *ProcessSocketFactory::create()
{
    // FIXME: this may leak memory if an exception strikes in this function
    // (before we return the pointer)
    return new ProcessSocket(new QProcess(), executable, args);
}

void ProcessSocketFactory::setProxySettings(const ProxySettings proxySettings, const QString &protocolTag)
{
    throw std::invalid_argument("This socket factory doesn't manufacture network sockets and therefore doesn't support proxy settings");
}

SslSocketFactory::SslSocketFactory(const QString &host, const quint16 port):
    host(host), port(port)
{
}

void SslSocketFactory::setProxySettings(const ProxySettings proxySettings, const QString &protocolTag)
{
    m_proxySettings = proxySettings;
    m_protocolTag = protocolTag;
}

Socket *SslSocketFactory::create()
{
    QSslSocket *sslSock = new QSslSocket();
    SslTlsSocket *sock = new SslTlsSocket(sslSock, host, port, true);
    sock->setProxySettings(m_proxySettings, m_protocolTag);
    return sock;
}


TlsAbleSocketFactory::TlsAbleSocketFactory(const QString &host, const quint16 port):
    host(host), port(port)
{
}

void TlsAbleSocketFactory::setProxySettings(const ProxySettings proxySettings, const QString &protocolTag)
{
    m_proxySettings = proxySettings;
    m_protocolTag = protocolTag;
}

Socket *TlsAbleSocketFactory::create()
{
    QSslSocket *sslSock = new QSslSocket();
    SslTlsSocket *sock = new SslTlsSocket(sslSock, host, port);
    sock->setProxySettings(m_proxySettings, m_protocolTag);
    return sock;
}

FakeSocketFactory::FakeSocketFactory(const Imap::ConnectionState initialState): SocketFactory(), m_initialState(initialState)
{
}

Socket *FakeSocketFactory::create()
{
    return m_last = new FakeSocket(m_initialState);
}

Socket *FakeSocketFactory::lastSocket()
{
    Q_ASSERT(m_last);
    return m_last;
}

void FakeSocketFactory::setInitialState(const Imap::ConnectionState initialState)
{
    m_initialState = initialState;
}

void FakeSocketFactory::setProxySettings(const ProxySettings proxySettings, const QString &protocolTag)
{
    throw std::invalid_argument("This socket factory doesn't manufacture network sockets and therefore doesn't support proxy settings");
}

}
