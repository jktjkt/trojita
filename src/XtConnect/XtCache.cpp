/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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

#include "XtCache.h"
#include "Imap/Model/SQLCache.h"

namespace XtConnect {

XtCache::XtCache( QObject* parent, const QString& name, const QString& cacheDir ):
        Imap::Mailbox::AbstractCache(parent), _name(name), _cacheDir(cacheDir)
{
    _sqlCache = new Imap::Mailbox::SQLCache( this );
    connect( _sqlCache, SIGNAL(error(QString)), this, SIGNAL(error(QString)) );
}

XtCache::~XtCache()
{
}

bool XtCache::open()
{
    return _sqlCache->open( _name, _cacheDir + QLatin1String("/imap.cache.sqlite") );
}

QList<Imap::Mailbox::MailboxMetadata> XtCache::childMailboxes( const QString& mailbox ) const
{
    Q_UNUSED(mailbox);
    return QList<Imap::Mailbox::MailboxMetadata>();
}

bool XtCache::childMailboxesFresh( const QString& mailbox ) const
{
    Q_UNUSED(mailbox);
    return false;
}

void XtCache::setChildMailboxes( const QString& mailbox, const QList<Imap::Mailbox::MailboxMetadata>& data )
{
    Q_UNUSED(mailbox);
    Q_UNUSED(data);
}

void XtCache::forgetChildMailboxes( const QString& mailbox )
{
    Q_UNUSED(mailbox);
}

Imap::Mailbox::SyncState XtCache::mailboxSyncState( const QString& mailbox ) const
{
    return _sqlCache->mailboxSyncState( mailbox );
}

void XtCache::setMailboxSyncState( const QString& mailbox, const Imap::Mailbox::SyncState& state )
{
    _sqlCache->setMailboxSyncState( mailbox, state );
}

QList<uint> XtCache::uidMapping( const QString& mailbox ) const
{
    return _sqlCache->uidMapping( mailbox );
}

void XtCache::setUidMapping( const QString& mailbox, const QList<uint>& seqToUid )
{
    _sqlCache->setUidMapping( mailbox, seqToUid );
}

void XtCache::clearUidMapping( const QString& mailbox )
{
    _sqlCache->clearUidMapping( mailbox );
}

void XtCache::clearAllMessages( const QString& mailbox )
{
    _sqlCache->clearAllMessages( mailbox );
}

void XtCache::clearMessage( const QString mailbox, uint uid )
{
    _sqlCache->clearMessage( mailbox, uid );
}

QStringList XtCache::msgFlags( const QString& mailbox, uint uid ) const
{
    Q_UNUSED(mailbox);
    Q_UNUSED(uid);
    return QStringList();
}

void XtCache::setMsgFlags( const QString& mailbox, uint uid, const QStringList& flags )
{
    Q_UNUSED(mailbox);
    Q_UNUSED(uid);
    Q_UNUSED(flags);
}

XtCache::MessageDataBundle XtCache::messageMetadata( const QString& mailbox, uint uid ) const
{
    Q_UNUSED(mailbox);
    Q_UNUSED(uid);
    return MessageDataBundle();
}

void XtCache::setMessageMetadata( const QString& mailbox, uint uid, const MessageDataBundle& metadata )
{
    Q_UNUSED(mailbox);
    Q_UNUSED(uid);
    Q_UNUSED(metadata);
}

QByteArray XtCache::messagePart( const QString& mailbox, uint uid, const QString& partId ) const
{
    Q_UNUSED(mailbox);
    Q_UNUSED(uid);
    Q_UNUSED(partId);
    return QByteArray();
}

void XtCache::setMsgPart( const QString& mailbox, uint uid, const QString& partId, const QByteArray& data )
{
    Q_UNUSED(mailbox);
    Q_UNUSED(uid);
    Q_UNUSED(partId);
    Q_UNUSED(data);
}

XtCache::SavingState XtCache::messageSavingStatus( const QString &mailbox, const uint uid ) const
{
    QStringList flags = _sqlCache->msgFlags( mailbox, uid );
    if ( flags.size() != 1 )
        return STATE_UNKNOWN;
    if ( flags.first() == QLatin1String("S") )
        return STATE_SAVED;
    if ( flags.first() == QLatin1String("D") )
        return STATE_DUPLICATE;
    return STATE_UNKNOWN;
}

void XtCache::setMessageSavingStatus( const QString &mailbox, const uint uid, const SavingState status )
{
    QStringList flags;
    switch ( status ) {
    case STATE_DUPLICATE:
        flags << QLatin1String("D");
        break;
    case STATE_SAVED:
        flags << QLatin1String("S");
        break;
    case STATE_UNKNOWN:
        break;
    }
    _sqlCache->setMsgFlags( mailbox, uid, flags );
}

QVector<Imap::Responses::ThreadingNode> XtCache::messageThreading(const QString &mailbox)
{
    Q_UNUSED(mailbox);
    return QVector<Imap::Responses::ThreadingNode>();
}

void XtCache::setMessageThreading(const QString &mailbox, const QVector<Imap::Responses::ThreadingNode> &threading)
{
    Q_UNUSED(mailbox);
    Q_UNUSED(threading);
}

void XtCache::setRenewalThreshold(const int days)
{
    Q_UNUSED(days);
}

}
