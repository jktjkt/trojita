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

#ifndef STREAMS_SOCKETFACTORY_H
#define STREAMS_SOCKETFACTORY_H

#include <QPointer>
#include <QStringList>
#include "Socket.h"

namespace Streams {

/** @short Abstract interface for creating new socket that is somehow connected
 * to the IMAP server */
class SocketFactory: public QObject
{
    Q_OBJECT
    bool m_startTls;
public:
    SocketFactory();
    virtual ~SocketFactory() {}
    /** @short Create new socket and return a smart pointer to it */
    virtual Socket *create() = 0;
    void setStartTlsRequired(const bool doIt);
    bool startTlsRequired();
signals:
    void error(const QString &);
};

/** @short Manufacture sockets based on QProcess */
class ProcessSocketFactory: public SocketFactory
{
    Q_OBJECT
    /** @short Name of executable file to launch */
    QString executable;
    /** @short Arguments to launch the process with */
    QStringList args;
public:
    ProcessSocketFactory(const QString &executable, const QStringList &args);
    virtual Socket *create();
};

/** @short Manufacture sockets based on QSslSocket */
class SslSocketFactory: public SocketFactory
{
    Q_OBJECT
    /** @short Hostname of the remote host */
    QString host;
    /** @short Port number */
    quint16 port;
public:
    SslSocketFactory(const QString &host, const quint16 port);
    virtual Socket *create();
};

/** @short Factory for regular TCP sockets that are able to STARTTLS */
class TlsAbleSocketFactory: public SocketFactory
{
    Q_OBJECT
    /** @short Hostname of the remote host */
    QString host;
    /** @short Port number */
    quint16 port;
public:
    TlsAbleSocketFactory(const QString &host, const quint16 port);
    virtual Socket *create();
};

/** @short A fake factory suitable for unit tests */
class FakeSocketFactory: public SocketFactory
{
    Q_OBJECT
public:
    explicit FakeSocketFactory(const Imap::ConnectionState initialState);
    virtual Socket *create();
    /** @short Return the last created socket */
    Socket *lastSocket();
    void setInitialState(const Imap::ConnectionState initialState);
private:
    QPointer<Socket> m_last;
    Imap::ConnectionState m_initialState;
};

}

#endif
