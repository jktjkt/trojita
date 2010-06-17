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
#include <QDebug>
#include <QStringList>
#include <QTimer>

#include "FormattingReply.h"
#include "MsgPartNetAccessManager.h"
#include "MsgPartNetworkReply.h"
#include "Imap/Model/MailboxTree.h"

namespace Imap {

namespace Network {

FormattingReply::FormattingReply( QObject* parent, Imap::Mailbox::Model* _model,
        Imap::Mailbox::TreeItemMessage* _msg, Imap::Mailbox::TreeItemPart* _part ):
    QNetworkReply( parent ), model(_model), msg(_msg), part(_part), pendingCount(0)
{
    setOpenMode( QIODevice::ReadOnly | QIODevice::Unbuffered );
    requestAnotherPart( part );
}

void FormattingReply::abort()
{
    close();
}

void FormattingReply::close()
{
    buffer.close();
}

qint64 FormattingReply::bytesAvailable() const
{
    return buffer.bytesAvailable() + QNetworkReply::bytesAvailable();
}

qint64 FormattingReply::readData( char* data, qint64 maxSize )
{
    qint64 res = buffer.read( data, maxSize );
    return res;
}

void FormattingReply::requestAnotherPart( Imap::Mailbox::TreeItemPart* anotherPart )
{
    MsgPartNetworkReply* reply = new MsgPartNetworkReply( this, model, msg, anotherPart );
    replies.append( reply );
    pendingBitmap.append( true );
    ++pendingCount;
    connect( reply, SIGNAL( finished() ), this, SLOT( anotherReplyFinished() ) );
}

void FormattingReply::anotherReplyFinished()
{
    MsgPartNetworkReply* anotherReply = qobject_cast<MsgPartNetworkReply*>( sender() );
    Q_ASSERT( anotherReply );
    int offset = replies.indexOf( anotherReply );
    Q_ASSERT( offset != -1 );
    if ( pendingBitmap[ offset ] ) {
        pendingBitmap[ offset ] = false;
        --pendingCount;
    }
    disconnect( anotherReply, SIGNAL( finished() ), this, SLOT( anotherReplyFinished() ) );

    if ( offset == 0 )
        mainReplyFinished();
    else if ( pendingCount == 0 )
        everythingFinished();
}

void FormattingReply::mainReplyFinished()
{
    QString mimeType = part->mimeType();
    if ( mimeType.startsWith( QLatin1String( "text/" ) ) ) {
        setData( part->charset().isEmpty() ?
                    mimeType :
                    QString("%1; charset=%2").arg( mimeType ).arg( part->charset() ),
                 replies[0]->readAll() );
        everythingFinished();
    } else {
        setData( mimeType, replies[0]->readAll() );
        everythingFinished();
    }
}

void FormattingReply::setData( const QString& mimeType, const QByteArray& data )
{
    setHeader( QNetworkRequest::ContentTypeHeader, mimeType );
    buffer.setData( data );
}

void FormattingReply::everythingFinished()
{
    Q_ASSERT( ! buffer.isOpen() );
    buffer.open( QIODevice::ReadOnly );
    emit readyRead();
    emit finished();
}

}
}
