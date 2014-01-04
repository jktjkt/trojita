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

#ifndef STREAMS_FAKE_SOCKET_H
#define STREAMS_FAKE_SOCKET_H

#include <QAbstractSocket>
#include <QProcess>
#include "Socket.h"

class QTimer;

namespace Streams {

/** @short A fake socket implementation, useful for automated unit tests

See the unit tests in tests/ for how to use this class.
*/
class FakeSocket: public Socket
{
    Q_OBJECT
public:
    explicit FakeSocket(const Imap::ConnectionState initialState);
    ~FakeSocket();
    virtual bool canReadLine();
    virtual QByteArray read(qint64 maxSize);
    virtual QByteArray readLine(qint64 maxSize = 0);
    virtual qint64 write(const QByteArray &byteArray);
    virtual void startTls();
    virtual void startDeflate();
    virtual bool isDead();
    virtual void close();

    /** @short Return data written since the last call to this function */
    QByteArray writtenStuff();

private slots:
    /** @short Delayed informing about being connected */
    void slotEmitConnected();
    /** @short Delayed informing about being encrypted */
    void slotEmitEncrypted();

public slots:
    /** @short Simulate arrival of some data

    The provided @arg what data are appended to the internal buffer and relevant signals
    are emitted. This function currently does not free the occupied memory, which might
    eventually lead to certain troubles.
    */
    void fakeReading(const QByteArray &what);

private:
    QIODevice *readChannel;
    QIODevice *writeChannel;

    QByteArray r, w;

    Imap::ConnectionState m_initialState;

    FakeSocket(const FakeSocket &); // don't implement
    FakeSocket &operator=(const FakeSocket &); // don't implement
};

};

#endif
