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

#include "Imap/Cache.h"
#include <QDebug>

namespace Imap {
namespace Mailbox {

NoCache::NoCache()
{
    //_cache[ "" ] = QList<MailboxMetadata>() << MailboxMetadata( "INBOX", "", QStringList() << "\\HASNOCHILDREN" );
}

QList<MailboxMetadata> NoCache::childMailboxes( const QString& mailbox ) const
{
    return _cache[ mailbox ];
}

bool NoCache::childMailboxesFresh( const QString& mailbox ) const
{
    return _cache.contains( mailbox );
}

void NoCache::setChildMailboxes( const QString& mailbox, const QList<MailboxMetadata>& data )
{
    _cache[ mailbox ] = data;
}

void NoCache::forgetChildMailboxes( const QString& mailbox )
{
    for ( QMap<QString,QList<MailboxMetadata> >::iterator it = _cache.begin();
          it != _cache.end(); /* do nothing */ ) {
        if ( it.key().startsWith( mailbox ) ) {
            it = _cache.erase( it );
        } else {
            ++it;
        }
    }
}

}
}
