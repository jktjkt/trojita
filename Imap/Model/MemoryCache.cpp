/* Copyright (C) 2007 Jan Kundrát <jkt@gentoo.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "MemoryCache.h"
#include <QDebug>

#define CACHE_DEBUG

QDebug operator<<( QDebug& dbg, const Imap::Mailbox::MailboxMetadata& metadata )
{
    return dbg << metadata.mailbox << metadata.separator << metadata.flags;
}

QDebug operator<<( QDebug& dbg, const Imap::Mailbox::SyncState& state )
{
    return dbg << "UIDVALIDITY" << state.uidValidity() << "UIDNEXT" << state.uidNext() <<
            "EXISTS" << state.exists() << "UNSEEN" << state.unSeen() <<
            "RECENT" << state.recent() << "PERMANENTFLAGS" << state.permanentFlags();
}


namespace Imap {
namespace Mailbox {

MemoryCache::MemoryCache()
{
    //_cache[ "" ] = QList<MailboxMetadata>() << MailboxMetadata( "INBOX", "", QStringList() << "\\HASNOCHILDREN" );
}

QList<MailboxMetadata> MemoryCache::childMailboxes( const QString& mailbox ) const
{
    return _cache[ mailbox ];
}

bool MemoryCache::childMailboxesFresh( const QString& mailbox ) const
{
    return _cache.contains( mailbox );
}

void MemoryCache::setChildMailboxes( const QString& mailbox, const QList<MailboxMetadata>& data )
{
#ifdef CACHE_DEBUG
    qDebug() << "setting child mailboxes for" << mailbox << "to" << data;
#endif
    _cache[ mailbox ] = data;
}

void MemoryCache::forgetChildMailboxes( const QString& mailbox )
{
    for ( QMap<QString,QList<MailboxMetadata> >::iterator it = _cache.begin();
          it != _cache.end(); /* do nothing */ ) {
        if ( it.key().startsWith( mailbox ) ) {
#ifdef CACHE_DEBUG
                qDebug() << "forgetting about mailbox" << it.key();
#endif
            it = _cache.erase( it );
        } else {
            ++it;
        }
    }
}

SyncState MemoryCache::mailboxSyncState( const QString& mailbox ) const
{
    return _syncState[ mailbox ];
}

void MemoryCache::setMailboxSyncState( const QString& mailbox, const SyncState& state )
{
#ifdef CACHE_DEBUG
    qDebug() << "setting mailbox sync state of" << mailbox << "to" << state;
#endif
    _syncState[ mailbox ] = state;
}

}
}
