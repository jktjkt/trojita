/* Copyright (C) 2010 Jan Kundrát <jkt@gentoo.org>

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

#ifndef IMAP_MODEL_SQLCACHE_H
#define IMAP_MODEL_SQLCACHE_H

#include "Cache.h"
#include <QSqlDatabase>

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

/** @short A cache implementation using an sqlite database for the underlying storage

  This class should not be used on its own, as it simply puts everything into a database.
This is clearly a suboptimal way to store large binary data, like e-mail attachments. The
purpose of this class is therefore to serve as one of a few caching backends, which are
subsequently used by an intelligent cache manager.
 */
class SQLCache : public AbstractCache {
public:
    SQLCache( const QString& fileName );
    ~SQLCache();

    virtual QList<MailboxMetadata> childMailboxes( const QString& mailbox ) const;
    virtual bool childMailboxesFresh( const QString& mailbox ) const;
    virtual void setChildMailboxes( const QString& mailbox, const QList<MailboxMetadata>& data );
    virtual void forgetChildMailboxes( const QString& mailbox );

    virtual SyncState mailboxSyncState( const QString& mailbox ) const;
    virtual void setMailboxSyncState( const QString& mailbox, const SyncState& state );

    virtual void setUidMapping( const QString& mailbox, const QList<uint>& seqToUid );
    virtual void clearUidMapping( const QString& mailbox );

    virtual void clearAllMessages( const QString& mailbox );
    virtual void clearMessage( const QString mailbox, uint uid );
    virtual void setMsgPart( const QString& mailbox, uint uid, const QString& partId, const QByteArray& data );
    virtual void setMsgEnvelope( const QString& mailbox, uint uid, const Imap::Message::Envelope& envelope );
    virtual void setMsgSize( const QString& mailbox, uint uid, uint size );
    virtual void setMsgStructure( const QString& mailbox, uint uid, const QByteArray& serializedData );
    virtual void setMsgFlags( const QString& mailbox, uint uid, const QStringList& flags );

    virtual QList<uint> uidMapping( const QString& mailbox );
    virtual MessageDataBundle messageMetadata( const QString& mailbox, uint uid );
    virtual QByteArray messagePart( const QString& mailbox, uint uid, const QString& partId );

    /** @short Open a connection to the cache */
    bool open();

private:
    QSqlDatabase db;
};

}

}

#endif /* IMAP_MODEL_SQLCACHE_H */
