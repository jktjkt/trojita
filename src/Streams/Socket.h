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
#ifndef STREAMS_SOCKET_H
#define STREAMS_SOCKET_H

#include <memory>
#include <QAbstractSocket>
#include <QSslError>
#include "../Imap/ConnectionState.h"

namespace Streams {

/** @short A common wrapepr class for implementing remote sockets

  This class extends the basic QIODevice-like API by a few handy methods,
so that the upper layers do not have to worry about low-level socket details.
*/
class Socket: public QObject
{
    Q_OBJECT
public:
    virtual ~Socket();

    /** @short Returns true if there's enough data to read, including the CR-LF pair */
    virtual bool canReadLine() = 0;

    /** @short Read at most @arg maxSize bytes from the socket */
    virtual QByteArray read(qint64 maxSize) = 0;

    /** @short Read a line from the socket (up to the @arg maxSize bytes) */
    virtual QByteArray readLine(qint64 maxSize = 0) = 0;

    /** @short Write the contents of the @arg byteArray buffer to the socket */
    virtual qint64 write(const QByteArray &byteArray) = 0;

    /** @short Negotiate and start encryption with the remote peer

      Please note that this function can throw an exception if the
    underlying socket implementation does not support TLS (an example of
    such an implementation is QProcess-backed socket).
    */
    virtual void startTls() = 0;

    /** @short Return true if the socket is no longer usable */
    virtual bool isDead() = 0;

    /** @short Return complete SSL certificate chain of the peer */
    virtual QList<QSslCertificate> sslChain() const;

    /** @short Return a list of SSL errors encountered during this connection */
    virtual QList<QSslError> sslErrors() const;

    /** @short Is this socket starting ecnryption from the very start? */
    virtual bool isConnectingEncryptedSinceStart() const;

    /** @short Close the connection */
    virtual void close() = 0;

    /** @short Start the DEFLATE algorithm on both directions of this stream */
    virtual void startDeflate() = 0;
signals:
    /** @short The socket got disconnected */
    void disconnected(const QString);

    /** @short Some data could be read from the socket */
    void readyRead();

    /** @short Low-level state of the connection has changed */
    void stateChanged(Imap::ConnectionState state, const QString &message);

    /** @short The socket is now encrypted */
    void encrypted();
};

}

#endif
