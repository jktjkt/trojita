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

#include "DiskPartCache.h"
#include <QDebug>
#include <QDir>

namespace {
    QString fileErrorToString( const QFile::FileError e )
    {
        switch ( e ) {
        case QFile::NoError:
            return QObject::tr("QFile::NoError");
        case QFile::ReadError:
            return QObject::tr("QFile::ReadError");
        case QFile::WriteError:
            return QObject::tr("QFile::WriteError");
        case QFile::FatalError:
            return QObject::tr("QFile::FatalError");
        case QFile::ResourceError:
            return QObject::tr("QFile::ResourceError");
        case QFile::OpenError:
            return QObject::tr("QFile::OpenError");
        case QFile::AbortError:
            return QObject::tr("QFile::AbortError");
        case QFile::TimeOutError:
            return QObject::tr("QFile::TimeOutError");
        case QFile::UnspecifiedError:
            return QObject::tr("QFile::UnspecifiedError");
        case QFile::RemoveError:
            return QObject::tr("QFile::RemoveError");
        case QFile::RenameError:
            return QObject::tr("QFile::RenameError");
        case QFile::PositionError:
            return QObject::tr("QFile::PositionError");
        case QFile::ResizeError:
            return QObject::tr("QFile::ResizeError");
        case QFile::PermissionsError:
            return QObject::tr("QFile::PermissionsError");
        case QFile::CopyError:
            return QObject::tr("QFile::CopyError");
        }
        return QObject::tr("Unrecognized QFile error");
    }
}

namespace Imap {
namespace Mailbox {

DiskPartCache::DiskPartCache( QObject* parent, const QString& cacheDir ): QObject(parent), _cacheDir(cacheDir)
{
    if ( ! _cacheDir.endsWith( QChar('/') ) )
        _cacheDir.append( QChar('/') );
}

void DiskPartCache::clearAllMessages( const QString& mailbox )
{
    QDir dir( dirForMailbox( mailbox ) );
    Q_FOREACH( const QString& fname, dir.entryList( QStringList() << QLatin1String("*.cache") ) ) {
        if ( ! dir.remove( fname ) ) {
            emit error( tr("Couldn't remove file %1 for mailbox %2").arg( fname, mailbox ) );
        }
    }
}

void DiskPartCache::clearMessage( const QString mailbox, uint uid )
{
    QDir dir( dirForMailbox( mailbox ) );
    Q_FOREACH( const QString& fname, dir.entryList( QStringList() << QString::fromAscii("%1_*.cache").arg( QString::number( uid ) ) ) ) {
        if ( ! dir.remove( fname ) ) {
            emit error( tr("Couldn't remove file %1 for message %2, mailbox %3").arg( fname, QString::number( uid ), mailbox ) );
        }
    }
}

QByteArray DiskPartCache::messagePart( const QString& mailbox, uint uid, const QString& partId ) const
{
    QFile buf( QString::fromAscii("%1/%2_%3.cache").arg( dirForMailbox( mailbox ), QString::number( uid ), partId ) );
    if ( ! buf.open( QIODevice::ReadOnly ) ) {
        return QByteArray();
    }
    return qUncompress( buf.readAll() );
}

void DiskPartCache::setMsgPart( const QString& mailbox, uint uid, const QString& partId, const QByteArray& data )
{
    QString myPath = dirForMailbox( mailbox );
    QDir dir(myPath);
    dir.mkpath( myPath );
    QString fileName = QString::fromAscii("%1/%2_%3.cache").arg( myPath, QString::number( uid ), partId );
    QFile buf( fileName );
    if ( ! buf.open( QIODevice::WriteOnly ) ) {
        emit error( tr("Couldn't save the part %1 of message %2 (mailbox %3) into file %4: %5 (%6)").arg(
                partId, QString::number(uid), mailbox, fileName, buf.errorString(), fileErrorToString( buf.error() ) ) );
    }
    buf.write( qCompress( data ) );
}

QString DiskPartCache::dirForMailbox( const QString& mailbox ) const
{
    return _cacheDir + mailbox.toUtf8().toBase64();
}

}
}

