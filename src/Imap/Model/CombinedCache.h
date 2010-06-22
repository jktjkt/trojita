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

#ifndef IMAP_MODEL_COMBINEDCACHE_H
#define IMAP_MODEL_COMBINEDCACHE_H

#include "Cache.h"

namespace Imap {

namespace Mailbox {

class SQLCache;
class DiskPartCache;

class CombinedCache : public AbstractCache {
    Q_OBJECT
public:
    CombinedCache( QObject* parent, const QString& name, const QString& cacheDir );
    virtual ~CombinedCache();

    virtual QList<MailboxMetadata> childMailboxes( const QString& mailbox ) const;
    virtual bool childMailboxesFresh( const QString& mailbox ) const;
    virtual void setChildMailboxes( const QString& mailbox, const QList<MailboxMetadata>& data );
    virtual void forgetChildMailboxes( const QString& mailbox );

    virtual SyncState mailboxSyncState( const QString& mailbox ) const;
    virtual void setMailboxSyncState( const QString& mailbox, const SyncState& state );

    virtual void setUidMapping( const QString& mailbox, const QList<uint>& seqToUid );
    virtual void clearUidMapping( const QString& mailbox );
    virtual QList<uint> uidMapping( const QString& mailbox ) const;

    virtual void clearAllMessages( const QString& mailbox );
    virtual void clearMessage( const QString mailbox, uint uid );

    virtual MessageDataBundle messageMetadata( const QString& mailbox, uint uid ) const;
    virtual void setMessageMetadata( const QString& mailbox, uint uid, const MessageDataBundle& metadata );

    virtual QStringList msgFlags( const QString& mailbox, uint uid ) const;
    virtual void setMsgFlags( const QString& mailbox, uint uid, const QStringList& flags );

    virtual QByteArray messagePart( const QString& mailbox, uint uid, const QString& partId ) const;
    virtual void setMsgPart( const QString& mailbox, uint uid, const QString& partId, const QByteArray& data );

    /** @short Open a connection to the cache */
    bool open();

private:
    SQLCache* _sqlCache;
    DiskPartCache* _diskPartCache;
    QString _name;
    QString _cacheDir;
};

}

}

#endif /* IMAP_MODEL_COMBINEDCACHE_H */
