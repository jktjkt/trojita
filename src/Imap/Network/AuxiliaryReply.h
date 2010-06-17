/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

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
#ifndef IMAP_NET_AUXILIARYREPLY_H
#define IMAP_NET_AUXILIARYREPLY_H

#include <QBuffer>
#include <QNetworkReply>

namespace Imap {
namespace Network {

class AuxiliaryReply : public QNetworkReply
{
Q_OBJECT
public:
    AuxiliaryReply( QObject* parent, const QString& textualMessage );
    virtual void close();
    virtual qint64 bytesAvailable() const;
protected:
    virtual qint64 readData( char* data, qint64 maxSize );
    virtual void abort();
private slots:
    void slotFinish();
private:
    QBuffer buffer;
    QByteArray message;
};

}
}

#endif // IMAP_NET_AUXILIARYREPLY_H
