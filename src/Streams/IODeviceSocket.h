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
#ifndef STREAMS_IODEVICE_SOCKET_H
#define STREAMS_IODEVICE_SOCKET_H

#include <QProcess>
#include <QSslSocket>
#include "Socket.h"

class QTimer;

namespace Streams {

class Rfc1951Compressor;
class Rfc1951Decompressor;
class SocketFactory;

/** @short Helper class for all sockets which are based on a QIODevice */
class IODeviceSocket: public Socket
{
    Q_OBJECT
    Q_DISABLE_COPY(IODeviceSocket)
public:
    explicit IODeviceSocket(QIODevice *device);
    ~IODeviceSocket();
    virtual bool canReadLine();
    virtual QByteArray read(qint64 maxSize);
    virtual QByteArray readLine(qint64 maxSize = 0);
    virtual qint64 write(const QByteArray &byteArray);
    virtual void startTls();
    virtual void startDeflate();
    virtual bool isDead() = 0;
private slots:
    virtual void handleStateChanged() = 0;
    virtual void delayedStart() = 0;
    virtual void handleReadyRead();
    void emitError();
protected:
    QIODevice *d;
    Rfc1951Compressor *m_compressor;
    Rfc1951Decompressor *m_decompressor;
    QTimer *delayedDisconnect;
    QString disconnectedMessage;
};

/** @short A QProcess-based socket */
class ProcessSocket: public IODeviceSocket
{
    Q_OBJECT
    Q_DISABLE_COPY(ProcessSocket)
public:
    ProcessSocket(QProcess *proc, const QString &executable, const QStringList &args);
    ~ProcessSocket();
    bool isDead();
    virtual void close();
private slots:
    void handleStateChanged();
    void handleProcessError(QProcess::ProcessError);
    void delayedStart();
private:
    QString executable;
    QStringList args;
};

/** @short An SSL socket, usable both in SSL-from-start and STARTTLS-on-demand mode */
class SslTlsSocket: public IODeviceSocket
{
    Q_OBJECT
    Q_DISABLE_COPY(SslTlsSocket)
public:
    /** Set the @arg startEncrypted to true if the wrapper is supposed to emit
    connected() only after it has established proper encryption */
    SslTlsSocket(QSslSocket *sock, const QString &host, const quint16 port, const bool startEncrypted=false);
    bool isDead();
    virtual QList<QSslCertificate> sslChain() const;
    virtual QList<QSslError> sslErrors() const;
    bool isConnectingEncryptedSinceStart() const;
    virtual void close();
private slots:
    void handleStateChanged();
    void handleSocketError(QAbstractSocket::SocketError);
    void delayedStart();
private:
    bool startEncrypted;
    QString host;
    quint16 port;
};

};

#endif
