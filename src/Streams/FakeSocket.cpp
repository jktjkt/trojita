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

#include <QBuffer>
#include <QTimer>
#include "FakeSocket.h"

namespace Streams {

FakeSocket::FakeSocket(const Imap::ConnectionState initialState): m_initialState(initialState)
{
    readChannel = new QBuffer(&r, this);
    readChannel->open(QIODevice::ReadWrite);
    writeChannel = new QBuffer(&w, this);
    writeChannel->open(QIODevice::WriteOnly);
    QTimer::singleShot(0, this, SLOT(slotEmitConnected()));
    connect(readChannel, SIGNAL(readyRead()), this, SIGNAL(readyRead()));
}

FakeSocket::~FakeSocket()
{
}

void FakeSocket::slotEmitConnected()
{
    if (m_initialState == Imap::CONN_STATE_LOGOUT) {
        // Special case: a fake socket factory for unconfigured accounts.
        emit disconnected(QString());
        return;
    }

    // We have to use both conventions for letting the world know that "we're finally usable"
    if (m_initialState != Imap::CONN_STATE_CONNECTED_PRETLS_PRECAPS)
        emit stateChanged(Imap::CONN_STATE_CONNECTED_PRETLS_PRECAPS, QString());
    emit stateChanged(m_initialState, QString());
}

void FakeSocket::slotEmitEncrypted()
{
    emit encrypted();
}

void FakeSocket::fakeReading(const QByteArray &what)
{
    // The position of the cursor is shared for both reading and writing, and therefore
    // we have to save and restore it after appending data, otherwise the pointer will
    // be left scrolled to after the actual data, failing further attempts to read the
    // data back. It's pretty obvious when you think about it, but took sime time to
    // debug nevertheless :).
    qint64 pos = readChannel->pos();
    if (pos > 1024 * 1024) {
        // There's too much stale data in the socket already, let's cut it
        QByteArray unProcessedData = readChannel->readAll();
        r.clear();
        readChannel->close();
        static_cast<QBuffer *>(readChannel)->setBuffer(&r);
        readChannel->open(QIODevice::ReadWrite);
        readChannel->write(unProcessedData);
        pos = unProcessedData.size();
    }
    readChannel->seek(r.size());
    readChannel->write(what);
    readChannel->seek(pos);
}

bool FakeSocket::canReadLine()
{
    return readChannel->canReadLine();
}

QByteArray FakeSocket::read(qint64 maxSize)
{
    return readChannel->read(maxSize);
}

QByteArray FakeSocket::readLine(qint64 maxSize)
{
    return readChannel->readLine(maxSize);
}

qint64 FakeSocket::write(const QByteArray &byteArray)
{
    return writeChannel->write(byteArray);
}

void FakeSocket::startTls()
{
    // fake it
    writeChannel->write(QByteArray("[*** STARTTLS ***]"));
    QTimer::singleShot(0, this, SLOT(slotEmitEncrypted()));
}

void FakeSocket::startDeflate()
{
    // fake it
    writeChannel->write(QByteArray("[*** DEFLATE ***]"));
}

bool FakeSocket::isDead()
{
    // Can't really die (yet)
    return false;
}

void FakeSocket::close()
{
    // fake it
    writeChannel->write(QByteArray("[*** close ***]"));
}

QByteArray FakeSocket::writtenStuff()
{
    QByteArray res = w;
    w.clear();
    writeChannel->seek(0);
    return res;
}

}
