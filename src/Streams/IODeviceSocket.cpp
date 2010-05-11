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

#include <QSslSocket>
#include <QTimer>
#include "IODeviceSocket.h"
#include "Imap/Exceptions.h"

namespace Imap {

IODeviceSocket::IODeviceSocket( QIODevice* device, const bool startEncrypted ): d(device), _startEncrypted(startEncrypted)
{
    connect( d, SIGNAL(readyRead()), this, SIGNAL(readyRead()) );
    connect( d, SIGNAL(readChannelFinished()), this, SLOT( handleStateChanged() ) );
    if ( QAbstractSocket* sock = qobject_cast<QAbstractSocket*>( device ) ) {
        connect( sock, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(handleStateChanged()) );
        connect( sock, SIGNAL( error( QAbstractSocket::SocketError ) ), this, SLOT(handleSocketError(QAbstractSocket::SocketError)) );
    } else if ( QProcess* proc = qobject_cast<QProcess*>( device ) ) {
        connect( proc, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(handleStateChanged()) );
        connect( proc, SIGNAL(error(QProcess::ProcessError)), this, SLOT(handleProcessError(QProcess::ProcessError)) );
    }
    delayedDisconnect = new QTimer();
    delayedDisconnect->setSingleShot( true );
    connect(delayedDisconnect, SIGNAL(timeout()), this, SLOT(emitError()));
}

IODeviceSocket::~IODeviceSocket()
{
    if ( QProcess* proc = qobject_cast<QProcess*>( d ) ) {
        // Be nice to it, let it die peacefully before using an axe
        proc->terminate();
        proc->waitForFinished(200);
        proc->kill();
    }

    delete d;
}

bool IODeviceSocket::canReadLine()
{
    return d->canReadLine();
}

QByteArray IODeviceSocket::read( qint64 maxSize )
{
    return d->read( maxSize );
}

QByteArray IODeviceSocket::readLine( qint64 maxSize )
{
    return d->readLine( maxSize );
}

qint64 IODeviceSocket::write( const QByteArray& byteArray )
{
    return d->write( byteArray );
}

void IODeviceSocket::startTls()
{
    QSslSocket* sock = qobject_cast<QSslSocket*>( d );
    if ( ! sock ) {
        throw InvalidArgument( "This IODeviceSocket is not a QSslSocket, and therefore doesn't support STARTTLS." );
    } else {
        sock->startClientEncryption();
    }
}

bool IODeviceSocket::isDead()
{
    if ( QProcess* proc = qobject_cast<QProcess*>( d ) ) {
        return proc->state() != QProcess::Running;
    } else if ( QAbstractSocket* sock = qobject_cast<QAbstractSocket*>( d ) ) {
        return sock->state() != QAbstractSocket::ConnectedState;
    } else {
        Q_ASSERT( false );
        return false;
    }
}

void IODeviceSocket::handleStateChanged()
{
    /* Qt delivers the stateChanged() signal before the error() one.
    That's a problem because we really want to provide a nice error message
    to the user and QAbstractSocket::error() is not set yet by the time this
    function executes. That's why we have to delay the first disconnected() signal. */

    if ( QProcess* proc = qobject_cast<QProcess*>( d ) ) {
        switch ( proc->state() ) {
            case QProcess::Running:
                emit connected();
                emit stateChanged(Imap::CONN_STATE_ESTABLISHED);
                break;
            case QProcess::Starting:
                emit stateChanged(Imap::CONN_STATE_CONNECTING);
                break;
            case QProcess::NotRunning:
                {
                    QString stdErr = QString::fromLocal8Bit( proc->readAllStandardError() );
                    if ( stdErr.isEmpty() )
                        disconnectedMessage = tr("The QProcess has exited with return code %1.").arg(
                                proc->exitCode() );
                    else
                        disconnectedMessage = tr("The QProcess has exited with return code %1:\n\n%2").arg(
                                    proc->exitCode() ).arg( stdErr );
                    delayedDisconnect->start();
                }
                break;
        }
    } else if ( QAbstractSocket* sock = qobject_cast<QAbstractSocket*>( d ) ) {
        switch ( sock->state() ) {
            case QAbstractSocket::HostLookupState:
                emit stateChanged(Imap::CONN_STATE_HOST_LOOKUP);
                break;
            case QAbstractSocket::ConnectingState:
                emit stateChanged(Imap::CONN_STATE_CONNECTING);
                break;
            case QAbstractSocket::BoundState:
            case QAbstractSocket::ListeningState:
                break;
            case QAbstractSocket::ConnectedState:
                if ( ! _startEncrypted ) {
                    emit connected();
                    emit stateChanged(Imap::CONN_STATE_ESTABLISHED);
                }
                break;
            case QAbstractSocket::UnconnectedState:
            case QAbstractSocket::ClosingState:
                disconnectedMessage = tr("Socket is disconnected: %1").arg( sock->errorString() );
                delayedDisconnect->start();
                break;
        }
    } else {
        Q_ASSERT( false );
    }
}

void IODeviceSocket::handleSocketError( QAbstractSocket::SocketError err )
{
    Q_UNUSED( err );
    QAbstractSocket* sock = qobject_cast<QAbstractSocket*>( d );
    Q_ASSERT( sock );
    delayedDisconnect->stop();
    emit disconnected( tr( "The underlying socket is having troubles: %1" ).arg( sock->errorString() ) );
}

void IODeviceSocket::emitError()
{
    emit disconnected( disconnectedMessage );
}

void IODeviceSocket::handleProcessError( QProcess::ProcessError err )
{
    Q_UNUSED( err );
    QProcess* proc = qobject_cast<QProcess*>( d );
    Q_ASSERT( proc );
    emit disconnected( tr( "The QProcess is having troubles: %1" ).arg( proc->errorString() ) );
}

}
