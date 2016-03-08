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

#include "IODeviceSocket.h"
#include <stdexcept>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTimer>
#include "TrojitaZlibStatus.h"
#if TROJITA_COMPRESS_DEFLATE
#include "3rdparty/rfc1951.h"
#endif
#include "Common/InvokeMethod.h"

namespace Streams {

IODeviceSocket::IODeviceSocket(QIODevice *device): d(device), m_compressor(0), m_decompressor(0)
{
    connect(d, &QIODevice::readyRead, this, &IODeviceSocket::handleReadyRead);
    connect(d, &QIODevice::readChannelFinished, this, &IODeviceSocket::handleStateChanged);
    delayedDisconnect = new QTimer();
    delayedDisconnect->setSingleShot(true);
    connect(delayedDisconnect, &QTimer::timeout, this, &IODeviceSocket::emitError);
    EMIT_LATER_NOARG(this, delayedStart);
}

IODeviceSocket::~IODeviceSocket()
{
    d->deleteLater();
#if TROJITA_COMPRESS_DEFLATE
    delete m_compressor;
    delete m_decompressor;
#endif
}

bool IODeviceSocket::canReadLine()
{
#if TROJITA_COMPRESS_DEFLATE
    if (m_decompressor) {
        return m_decompressor->canReadLine();
    }
#endif
    return d->canReadLine();
}

QByteArray IODeviceSocket::read(qint64 maxSize)
{
#if TROJITA_COMPRESS_DEFLATE
    if (m_decompressor) {
        return m_decompressor->read(maxSize);
    }
#endif
    return d->read(maxSize);
}

QByteArray IODeviceSocket::readLine(qint64 maxSize)
{
#if TROJITA_COMPRESS_DEFLATE
    if (m_decompressor) {
        // FIXME: well, we apparently don't respect the maxSize argument...
        return m_decompressor->readLine();
    }
#endif
    return d->readLine(maxSize);
}

qint64 IODeviceSocket::write(const QByteArray &byteArray)
{
#if TROJITA_COMPRESS_DEFLATE
    if (m_compressor) {
        m_compressor->write(d, &const_cast<QByteArray&>(byteArray));
        return byteArray.size();
    }
#endif
    return d->write(byteArray);
}

void IODeviceSocket::startTls()
{
    QSslSocket *sock = qobject_cast<QSslSocket *>(d);
    if (! sock)
        throw std::invalid_argument("This IODeviceSocket is not a QSslSocket, and therefore doesn't support STARTTLS.");
#if TROJITA_COMPRESS_DEFLATE
    if (m_compressor || m_decompressor)
        throw std::invalid_argument("DEFLATE is already active, cannot STARTTLS");
#endif
    sock->startClientEncryption();
}

void IODeviceSocket::startDeflate()
{
    if (m_compressor || m_decompressor)
        throw std::invalid_argument("DEFLATE compression is already active");

#if TROJITA_COMPRESS_DEFLATE
    m_compressor = new Rfc1951Compressor();
    m_decompressor = new Rfc1951Decompressor();
#else
    throw std::invalid_argument("Trojita got built without zlib support");
#endif
}

void IODeviceSocket::handleReadyRead()
{
#if TROJITA_COMPRESS_DEFLATE
    if (m_decompressor) {
        m_decompressor->consume(d);
    }
#endif
    emit readyRead();
}

void IODeviceSocket::emitError()
{
    emit disconnected(disconnectedMessage);
}

ProcessSocket::ProcessSocket(QProcess *proc, const QString &executable, const QStringList &args):
    IODeviceSocket(proc), executable(executable), args(args)
{
    connect(proc, &QProcess::stateChanged, this, &ProcessSocket::handleStateChanged);
    connect(proc, static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error), this, &ProcessSocket::handleProcessError);
}

ProcessSocket::~ProcessSocket()
{
    close();
}

void ProcessSocket::close()
{
    QProcess *proc = qobject_cast<QProcess *>(d);
    Q_ASSERT(proc);
    // Be nice to it, let it die peacefully before using an axe
    // QTBUG-5990, don't call waitForFinished() on a process which hadn't started
    if (proc->state() == QProcess::Running) {
        proc->terminate();
        proc->waitForFinished(200);
        proc->kill();
    }
}

bool ProcessSocket::isDead()
{
    QProcess *proc = qobject_cast<QProcess *>(d);
    Q_ASSERT(proc);
    return proc->state() != QProcess::Running;
}

void ProcessSocket::handleProcessError(QProcess::ProcessError err)
{
    Q_UNUSED(err);
    QProcess *proc = qobject_cast<QProcess *>(d);
    Q_ASSERT(proc);
    delayedDisconnect->stop();
    emit disconnected(tr("Disconnected: %1").arg(proc->errorString()));
}

void ProcessSocket::handleStateChanged()
{
    /* Qt delivers the stateChanged() signal before the error() one.
    That's a problem because we really want to provide a nice error message
    to the user and QAbstractSocket::error() is not set yet by the time this
    function executes. That's why we have to delay the first disconnected() signal. */

    QProcess *proc = qobject_cast<QProcess *>(d);
    Q_ASSERT(proc);
    switch (proc->state()) {
    case QProcess::Running:
        emit stateChanged(Imap::CONN_STATE_CONNECTED_PRETLS_PRECAPS, tr("The process has started"));
        break;
    case QProcess::Starting:
        emit stateChanged(Imap::CONN_STATE_CONNECTING, tr("Starting process `%1 %2`").arg(executable, args.join(QStringLiteral(" "))));
        break;
    case QProcess::NotRunning: {
        if (delayedDisconnect->isActive())
            break;
        QString stdErr = QString::fromLocal8Bit(proc->readAllStandardError());
        if (stdErr.isEmpty())
            disconnectedMessage = tr("The process has exited with return code %1.").arg(
                                      proc->exitCode());
        else
            disconnectedMessage = tr("The process has exited with return code %1:\n\n%2").arg(
                                      proc->exitCode()).arg(stdErr);
        delayedDisconnect->start();
    }
    break;
    }
}

void ProcessSocket::delayedStart()
{
    QProcess *proc = qobject_cast<QProcess *>(d);
    Q_ASSERT(proc);
    proc->start(executable, args);
}

SslTlsSocket::SslTlsSocket(QSslSocket *sock, const QString &host, const quint16 port, const bool startEncrypted):
    IODeviceSocket(sock), startEncrypted(startEncrypted), host(host), port(port), m_proxySettings(ProxySettings::RespectSystemProxy)
{
    // The Qt API for deciding about whereabouts of a SSL connection is unfortunately blocking, ie. one is expected to
    // call a function from a slot attached to the sslErrors signal to tell the code whether to proceed or not.
    // In QML, one cannot display a dialog box with a nested event loop, so this means that we have to deal with SSL/TLS
    // establishing at higher level.
    sock->ignoreSslErrors();
    sock->setProtocol(QSsl::AnyProtocol);
    sock->setPeerVerifyMode(QSslSocket::QueryPeer);

    // In response to the attacks related to the SSL compression, Digia has decided to disable SSL compression starting in
    // Qt 4.8.4 -- see http://qt.digia.com/en/Release-Notes/security-issue-september-2012/.
    // I have brought this up on the imap-protocol mailing list; the consensus seemed to be that the likelihood of an
    // successful exploit on an IMAP conversation is very unlikely.  The compression itself is, on the other hand, a
    // very worthwhile goal, so we explicitly enable it again.
    // Unfortunately, this was backported to older Qt versions as well (see qt4.git's 3488f1db96dbf70bb0486d3013d86252ebf433e0),
    // but there is no way of enabling compression back again.
    QSslConfiguration sslConf = sock->sslConfiguration();
    sslConf.setSslOption(QSsl::SslOptionDisableCompression, false);
    sock->setSslConfiguration(sslConf);

    connect(sock, &QSslSocket::encrypted, this, &Socket::encrypted);
    connect(sock, &QAbstractSocket::stateChanged, this, &SslTlsSocket::handleStateChanged);
    connect(sock, static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, &SslTlsSocket::handleSocketError);
}

void SslTlsSocket::setProxySettings(const ProxySettings proxySettings, const QString &protocolTag)
{
    m_proxySettings = proxySettings;
    m_protocolTag = protocolTag;
}

void SslTlsSocket::close()
{
    QSslSocket *sock = qobject_cast<QSslSocket*>(d);
    Q_ASSERT(sock);
    sock->abort();
    emit disconnected(tr("Connection closed"));
}

void SslTlsSocket::handleStateChanged()
{
    /* Qt delivers the stateChanged() signal before the error() one.
    That's a problem because we really want to provide a nice error message
    to the user and QAbstractSocket::error() is not set yet by the time this
    function executes. That's why we have to delay the first disconnected() signal. */

    QAbstractSocket *sock = qobject_cast<QAbstractSocket *>(d);
    Q_ASSERT(sock);
    QString proxyMsg;
    switch (sock->proxy().type()) {
    case QNetworkProxy::NoProxy:
        break;
    case QNetworkProxy::HttpCachingProxy:
        Q_ASSERT_X(false, "proxy detection",
                   "Qt should have returned a proxy capable of tunneling, but we got back an HTTP proxy.");
        break;
    case QNetworkProxy::FtpCachingProxy:
        Q_ASSERT_X(false, "proxy detection",
                   "Qt should have returned a proxy capable of tunneling, but we got back an FTP proxy.");
        break;
    case QNetworkProxy::DefaultProxy:
        proxyMsg = tr(" (via proxy %1)").arg(sock->proxy().hostName());
        break;
    case QNetworkProxy::Socks5Proxy:
        proxyMsg = tr(" (via SOCKS5 proxy %1)").arg(sock->proxy().hostName());
        break;
    case QNetworkProxy::HttpProxy:
        proxyMsg = tr(" (via HTTP proxy %1)").arg(sock->proxy().hostName());
        break;
    }
    switch (sock->state()) {
    case QAbstractSocket::HostLookupState:
        emit stateChanged(Imap::CONN_STATE_HOST_LOOKUP, tr("Looking up %1%2...").arg(host,
                              sock->proxy().capabilities().testFlag(QNetworkProxy::HostNameLookupCapability) ?
                              proxyMsg : QString()));
        break;
    case QAbstractSocket::ConnectingState:
        emit stateChanged(Imap::CONN_STATE_CONNECTING, tr("Connecting to %1:%2%3%4...").arg(
                              host, QString::number(port), startEncrypted ? tr(" (SSL)") : QString(),
                              sock->proxy().capabilities().testFlag(QNetworkProxy::TunnelingCapability) ?
                              proxyMsg : QString()));
        break;
    case QAbstractSocket::BoundState:
    case QAbstractSocket::ListeningState:
        break;
    case QAbstractSocket::ConnectedState:
        if (! startEncrypted) {
            emit stateChanged(Imap::CONN_STATE_CONNECTED_PRETLS_PRECAPS, tr("Connected"));
        } else {
            emit stateChanged(Imap::CONN_STATE_SSL_HANDSHAKE, tr("Negotiating encryption..."));
        }
        break;
    case QAbstractSocket::UnconnectedState:
    case QAbstractSocket::ClosingState:
        disconnectedMessage = tr("Socket is disconnected: %1").arg(sock->errorString());
        delayedDisconnect->start();
        break;
    }
}

void SslTlsSocket::handleSocketError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err);
    QAbstractSocket *sock = qobject_cast<QAbstractSocket *>(d);
    Q_ASSERT(sock);
    delayedDisconnect->stop();
    emit disconnected(tr("The underlying socket is having troubles when processing connection to %1:%2: %3").arg(
                          host, QString::number(port), sock->errorString()));
}

bool SslTlsSocket::isDead()
{
    QAbstractSocket *sock = qobject_cast<QAbstractSocket *>(d);
    Q_ASSERT(sock);
    return sock->state() != QAbstractSocket::ConnectedState;
}

void SslTlsSocket::delayedStart()
{
    QSslSocket *sock = qobject_cast<QSslSocket *>(d);
    Q_ASSERT(sock);

    switch (m_proxySettings) {
    case Streams::ProxySettings::RespectSystemProxy:
    {
        QNetworkProxy setting;
        QNetworkProxyQuery query = QNetworkProxyQuery(host, port, m_protocolTag, QNetworkProxyQuery::TcpSocket);

        // set to true if a capable setting is found
        bool capableSettingFound = false;

        // set to true if at least one valid setting is found
        bool settingFound = false;

        // FIXME: this static function works particularly slow in Windows
        QList<QNetworkProxy> proxySettingsList = QNetworkProxyFactory::systemProxyForQuery(query);

        /* Proxy Settings are read from the user's environment variables by the above static method.
         * A peculiar case is with *nix systems, where an undefined environment variable is returned as
         * an empty string. Such entries *might* exist in our proxySettingsList, and shouldn't be processed.
         * One good check is to use hostName() of the QNetworkProxy object, and treat the Proxy Setting as invalid if
         * the host name is empty. */
        Q_FOREACH (setting, proxySettingsList) {
            if (!setting.hostName().isEmpty()) {
                settingFound = true;

                // now check whether setting has capabilities
                if (setting.capabilities().testFlag(QNetworkProxy::TunnelingCapability)) {
                    sock->setProxy(setting);
                    capableSettingFound = true;
                    break;
                }
            }
        }

        if (!settingFound || proxySettingsList.isEmpty()) {
            sock->setProxy(QNetworkProxy::NoProxy);
        } else if (!capableSettingFound) {
            emit disconnected(tr("The underlying socket is having troubles when processing connection to %1:%2: %3")
                              .arg(host, QString::number(port), QStringLiteral("Cannot find proxy setting capable of tunneling")));
        }
        break;
    }
    case Streams::ProxySettings::DirectConnect:
        sock->setProxy(QNetworkProxy::NoProxy);
        break;
    }

    if (startEncrypted)
        sock->connectToHostEncrypted(host, port);
    else
        sock->connectToHost(host, port);
}

QList<QSslCertificate> SslTlsSocket::sslChain() const
{
    QSslSocket *sock = qobject_cast<QSslSocket *>(d);
    Q_ASSERT(sock);
    return sock->peerCertificateChain();
}

QList<QSslError> SslTlsSocket::sslErrors() const
{
    QSslSocket *sock = qobject_cast<QSslSocket *>(d);
    Q_ASSERT(sock);
    return sock->sslErrors();
}

bool SslTlsSocket::isConnectingEncryptedSinceStart() const
{
    return startEncrypted;
}

}
