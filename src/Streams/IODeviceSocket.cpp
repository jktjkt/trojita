/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <QSslSocket>
#include <QTimer>
#include "IODeviceSocket.h"
#include "Imap/Exceptions.h"

namespace Imap
{

IODeviceSocket::IODeviceSocket(QIODevice *device): d(device)
{
    connect(d, SIGNAL(readyRead()), this, SIGNAL(readyRead()));
    connect(d, SIGNAL(readChannelFinished()), this, SLOT(handleStateChanged()));
    delayedDisconnect = new QTimer();
    delayedDisconnect->setSingleShot(true);
    connect(delayedDisconnect, SIGNAL(timeout()), this, SLOT(emitError()));
    QTimer::singleShot(0, this, SLOT(delayedStart()));
}

IODeviceSocket::~IODeviceSocket()
{
    d->deleteLater();
}

bool IODeviceSocket::canReadLine()
{
    return d->canReadLine();
}

QByteArray IODeviceSocket::read(qint64 maxSize)
{
    return d->read(maxSize);
}

QByteArray IODeviceSocket::readLine(qint64 maxSize)
{
    return d->readLine(maxSize);
}

qint64 IODeviceSocket::write(const QByteArray &byteArray)
{
    return d->write(byteArray);
}

void IODeviceSocket::startTls()
{
    QSslSocket *sock = qobject_cast<QSslSocket *>(d);
    if (! sock) {
        throw InvalidArgument("This IODeviceSocket is not a QSslSocket, and therefore doesn't support STARTTLS.");
    } else {
        sock->startClientEncryption();
    }
}

void IODeviceSocket::emitError()
{
    emit disconnected(disconnectedMessage);
}

ProcessSocket::ProcessSocket(QProcess *proc, const QString &executable, const QStringList &args):
    IODeviceSocket(proc), executable(executable), args(args)
{
    connect(proc, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(handleStateChanged()));
    connect(proc, SIGNAL(error(QProcess::ProcessError)), this, SLOT(handleProcessError(QProcess::ProcessError)));
}

ProcessSocket::~ProcessSocket()
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
    emit disconnected(tr("The QProcess is having troubles: %1").arg(proc->errorString()));
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
        emit stateChanged(Imap::CONN_STATE_CONNECTING, tr("Starting process `%1 %2`").arg(executable, args.join(QLatin1String(" "))));
        break;
    case QProcess::NotRunning: {
        if (delayedDisconnect->isActive())
            break;
        QString stdErr = QString::fromLocal8Bit(proc->readAllStandardError());
        if (stdErr.isEmpty())
            disconnectedMessage = tr("The QProcess has exited with return code %1.").arg(
                                      proc->exitCode());
        else
            disconnectedMessage = tr("The QProcess has exited with return code %1:\n\n%2").arg(
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

QList<QSslCertificate> ProcessSocket::sslChain() const
{
    return QList<QSslCertificate>();
}

QList<QSslError> ProcessSocket::sslErrors() const
{
    return QList<QSslError>();
}

SslTlsSocket::SslTlsSocket(QSslSocket *sock, const QString &host, const quint16 port, const bool startEncrypted):
    IODeviceSocket(sock), startEncrypted(startEncrypted), host(host), port(port)
{
    // The Qt API for deciding about whereabouts of a SSL connection is unfortunately blocking, ie. one is expected to
    // call a function from a slot attached to the sslErrors signal to tell the code whether to proceed or not.
    // In QML, one cannot display a dialog box with a nested event loop, so this means that we have to deal with SSL/TLS
    // establishing at higher level.
    sock->ignoreSslErrors();
    sock->setProtocol(QSsl::AnyProtocol);
    sock->setPeerVerifyMode(QSslSocket::QueryPeer);

    connect(sock, SIGNAL(encrypted()), this, SIGNAL(encrypted()));
    connect(sock, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(handleStateChanged()));
    connect(sock, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(handleSocketError(QAbstractSocket::SocketError)));
}

void SslTlsSocket::handleStateChanged()
{
    /* Qt delivers the stateChanged() signal before the error() one.
    That's a problem because we really want to provide a nice error message
    to the user and QAbstractSocket::error() is not set yet by the time this
    function executes. That's why we have to delay the first disconnected() signal. */

    QAbstractSocket *sock = qobject_cast<QAbstractSocket *>(d);
    Q_ASSERT(sock);
    switch (sock->state()) {
    case QAbstractSocket::HostLookupState:
        emit stateChanged(Imap::CONN_STATE_HOST_LOOKUP, tr("Looking up %1...").arg(host));
        break;
    case QAbstractSocket::ConnectingState:
        emit stateChanged(Imap::CONN_STATE_CONNECTING, tr("Connecting to %1:%2%3...").arg(
                              host, QString::number(port), startEncrypted ? tr(" (SSL)") : QString()));
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
