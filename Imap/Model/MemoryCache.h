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

#ifndef IMAP_MODEL_MEMORYCACHE_H
#define IMAP_MODEL_MEMORYCACHE_H

#include "Cache.h"
#include <QMap>

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

/** @short A cache implementation that uses only in-memory cache */
class MemoryCache : public AbstractCache {
public:
    MemoryCache();

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
    virtual void setMsgStructure( const QString& mailbox, uint uid, const Imap::Message::AbstractMessage& data );
    virtual void setMsgFlags( const QString& mailbox, uint uid, const QStringList& flags );

private:
    QMap<QString, QList<MailboxMetadata> > _cache;
    QMap<QString, SyncState> _syncState;
    QMap<QString, QList<uint> > _seqToUid;
    QMap<QString, QMap<uint,QStringList> > _flags;
    QMap<QString, QMap<uint,uint> > _sizes;
    QMap<QString, QMap<uint, Imap::Message::Envelope> > _envelopes;
    QMap<QString, QMap<uint, QMap<QString, QByteArray> > > _parts;
};

}

}

#endif /* IMAP_MODEL_MEMORYCACHE_H */
