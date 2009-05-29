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
#include <QFile>

//#define CACHE_DEBUG
//#define CACHE_STATS

#ifdef CACHE_DEBUG
#define CACHE_STATS
#endif

namespace Imap {
namespace Mailbox {

MemoryCache::MemoryCache( const QString& fileName ): _fileName(fileName)
{
    loadData();
}

MemoryCache::~MemoryCache()
{
    saveData();
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

void MemoryCache::setUidMapping( const QString& mailbox, const QList<uint>& seqToUid )
{
#ifdef CACHE_DEBUG
    qDebug() << "saving UID mapping for" << mailbox << "to" << seqToUid;
#endif
    _seqToUid[ mailbox ] = seqToUid;
}

void MemoryCache::clearUidMapping( const QString& mailbox )
{
#ifdef CACHE_DEBUG
    qDebug() << "clearing UID mapping for" << mailbox;
#endif
    _seqToUid.remove( mailbox );
}

void MemoryCache::clearAllMessages( const QString& mailbox )
{
#ifdef CACHE_DEBUG
    qDebug() << "pruging all info for mailbox" << mailbox;
#endif
    _flags.remove( mailbox );
    _sizes.remove( mailbox );
    _envelopes.remove( mailbox );
    _parts.remove( mailbox );
    _bodyStructure.remove( mailbox );
}

void MemoryCache::clearMessage( const QString mailbox, uint uid )
{
#ifdef CACHE_DEBUG
    qDebug() << "pruging all info for message" << mailbox << uid;
#endif
    if ( _flags.contains( mailbox ) )
        _flags[ mailbox ].remove( uid );
    if ( _sizes.contains( mailbox ) )
        _sizes[ mailbox ].remove( uid );
    if ( _envelopes.contains( mailbox ) )
        _envelopes[ mailbox ].remove( uid );
    if ( _parts.contains( mailbox ) )
        _parts[ mailbox ].remove( uid );
    if ( _bodyStructure.contains( mailbox ) )
        _bodyStructure[ mailbox ].remove( uid );
}

void MemoryCache::setMsgPart( const QString& mailbox, uint uid, const QString& partId, const QByteArray& data )
{
#ifdef CACHE_DEBUG
    qDebug() << "set message part" << mailbox << uid << partId << data.size();
#endif
    _parts[ mailbox ][ uid ][ partId ] = data;
}

void MemoryCache::setMsgEnvelope( const QString& mailbox, uint uid, const Imap::Message::Envelope& envelope )
{
#ifdef CACHE_DEBUG
    qDebug() << "set envelope" << mailbox << uid << envelope;
#endif
    _envelopes[ mailbox ][ uid ] = envelope;
}

void MemoryCache::setMsgSize( const QString& mailbox, uint uid, uint size )
{
#ifdef CACHE_DEBUG
    qDebug() << "set msg size" << mailbox << uid << size;
#endif
    _sizes[ mailbox ][ uid ] = size;
}

void MemoryCache::setMsgStructure( const QString& mailbox, uint uid, const QByteArray& serializedData )
{
#ifdef CACHE_DEBUG
    qDebug() << "set msg body structure" << mailbox << uid << serializedData.size();
#endif
    _bodyStructure[ mailbox ][ uid ] = serializedData;
}

void MemoryCache::setMsgFlags( const QString& mailbox, uint uid, const QStringList& flags )
{
#ifdef CACHE_DEBUG
    qDebug() << "set FLAGS for" << mailbox << uid << flags;
#endif
    _flags[ mailbox ][ uid ] = flags;
}

QList<uint> MemoryCache::uidMapping( const QString& mailbox )
{
    return _seqToUid[ mailbox ];
}

MemoryCache::MessageDataBundle MemoryCache::messageMetadata( const QString& mailbox, uint uid )
{
    MessageDataBundle buf;
    if ( ! _envelopes.contains( mailbox ) ) {
#ifdef CACHE_DEBUG
        qDebug() << "No ENVELOPEs for" << mailbox;
#endif
        return buf;
    }
    if ( ! _envelopes[ mailbox ].contains( uid ) ) {
#ifdef CACHE_DEBUG
        qDebug() << "No ENVELOPE for" << mailbox << uid;
#endif
        return buf;
    }
    if ( ! _bodyStructure.contains( mailbox ) ) {
#ifdef CACHE_DEBUG
        qDebug() << "No BODYSTRUCTUREs for" << mailbox;
#endif
        return buf;
    }
    if ( ! _bodyStructure[ mailbox ].contains( uid ) ) {
#ifdef CACHE_DEBUG
        qDebug() << "No BODYSTRUCTURE for" << mailbox << uid;
#endif
        return buf;
    }
    buf.serializedBodyStructure = _bodyStructure[ mailbox ][ uid ];
    buf.envelope = _envelopes[ mailbox ][ uid ];
    buf.uid = uid;
    // These fields are not critical, so nothing happens if they aren't filed correctly:
    buf.flags = _flags[ mailbox ][ uid ];
    buf.size = _sizes[ mailbox ][ uid ];
    return buf;
}

QByteArray MemoryCache::messagePart( const QString& mailbox, uint uid, const QString& partId )
{
    if ( ! _parts.contains( mailbox ) )
        return QByteArray();
    const QMap<uint, QMap<QString, QByteArray> >& mailboxParts = _parts[ mailbox ];
    if ( ! mailboxParts.contains( uid ) )
        return QByteArray();
    const QMap<QString, QByteArray>& messageParts = mailboxParts[ uid ];
    if ( ! messageParts.contains( partId ) )
        return QByteArray();
    return messageParts[ partId ];
}

bool MemoryCache::loadData()
{
    if ( ! _fileName.isEmpty() ) {
        QFile file( _fileName );
        if ( ! file.open( QIODevice::ReadOnly ) )
            return false;
        QDataStream stream( &file );
        stream >> _cache >> _syncState >> _seqToUid >> _flags >> _sizes >>
                _bodyStructure >> _envelopes >> _parts;
        file.close();
        return true;
    }
    return false;
}

bool MemoryCache::saveData() const
{
#ifdef CACHE_STATS
    dump();
#endif
    if ( ! _fileName.isEmpty() ) {
        QFile file( _fileName );
        if ( ! file.open( QIODevice::WriteOnly ) )
            return false;
        QDataStream stream( &file );
        stream << _cache << _syncState << _seqToUid << _flags << _sizes <<
                _bodyStructure << _envelopes << _parts;
        file.close();
        return true;
    }
    return false;
}

void MemoryCache::dump() const
{
    qDebug() << "--------------------\nStatistics\n--------------------";
    qDebug() << "Sequences:";
    qDebug();
    QStringList mailboxes;
    mailboxes = _seqToUid.keys();
    Q_FOREACH( const QString& mbox, mailboxes ) {
        qDebug() << " " << mbox << _seqToUid[ mbox ];
    }
    qDebug();
    qDebug() << "Envelopes:";
    qDebug();
    mailboxes = _envelopes.keys();
    Q_FOREACH( const QString& mbox, mailboxes ) {
        qDebug() << " " << mbox << _envelopes[ mbox ].keys();
    }
    qDebug();
    qDebug() << "Body structures:";
    qDebug();
    mailboxes = _bodyStructure.keys();
    Q_FOREACH( const QString& mbox, mailboxes ) {
        qDebug() << " " << mbox << _bodyStructure[ mbox ].keys();
    }
}


}
}
