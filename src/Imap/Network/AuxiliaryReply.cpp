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
#include <QTimer>

#include "AuxiliaryReply.h"

namespace Imap {
namespace Network {

AuxiliaryReply::AuxiliaryReply( QObject* parent, const QString& textualMessage ):
    QNetworkReply( parent ), message( textualMessage.toUtf8() )
{
    buffer.setData( message );
    QTimer::singleShot( 0, this, SLOT( slotFinish() ) );
    setOpenMode( QIODevice::ReadOnly | QIODevice::Unbuffered );
    buffer.open( QIODevice::ReadOnly );
    setHeader( QNetworkRequest::ContentTypeHeader, QLatin1String( "text/plain" ) );
}

void AuxiliaryReply::slotFinish()
{
    emit readyRead();
    emit finished();
}

void AuxiliaryReply::abort()
{
    close();
}

void AuxiliaryReply::close()
{
    buffer.close();
}

qint64 AuxiliaryReply::bytesAvailable() const
{
    return buffer.bytesAvailable() + QNetworkReply::bytesAvailable();
}

qint64 AuxiliaryReply::readData( char* data, qint64 maxSize )
{
    return buffer.read( data, maxSize );
}


}
}

