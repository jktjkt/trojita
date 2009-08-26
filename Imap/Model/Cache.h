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

#ifndef IMAP_MODEL_CACHE_H
#define IMAP_MODEL_CACHE_H

#include <QSharedPointer>
#include "MailboxMetadata.h"
#include "../Parser/Message.h"

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

/** @short An abstract parent for all IMAP cache implementations */
class AbstractCache {
public:

    /** @short Helper for retrieving all data about a particular message from the cache */
    struct MessageDataBundle {
        /** @short The UID of the message */
        uint uid;
        /** @short Envelope */
        Imap::Message::Envelope envelope;
        /** @short RFC822.SIZE */
        uint size;
        /** @short FLAGS, a volatile parameter! */
        QStringList flags;
        /** @short Serialized form of BODYSTRUCTURE

        Due to the complex nature of BODYSTRUCTURE and the way we use, simly
        archiving the resulting object is far from trivial. The simplest way is
        offered by Imap::Message::AbstractMessage::fromList. Therefore, this item
        contains a QVariantList as serialized by QDataStream.
*/
        QByteArray serializedBodyStructure;

        MessageDataBundle(): uid(0) {}
    };

    virtual ~AbstractCache() {}

    /** @short Return a list of all known child mailboxes */
    virtual QList<MailboxMetadata> childMailboxes( const QString& mailbox ) const = 0;
    /** @short Is the result of childMailboxes() fresh enough? */
    virtual bool childMailboxesFresh( const QString& mailbox ) const = 0;
    /** @short Update cache info about the state of child mailboxes */
    virtual void setChildMailboxes( const QString& mailbox, const QList<MailboxMetadata>& data ) = 0;
    /** @short Forget about child mailboxes */
    virtual void forgetChildMailboxes( const QString& mailbox ) = 0;

    /** @short Return previous known state of a mailbox */
    virtual SyncState mailboxSyncState( const QString& mailbox ) const = 0;
    /** @short Set current syncing state */
    virtual void setMailboxSyncState( const QString& mailbox, const SyncState& state ) = 0;

    /** @short Store the mapping of sequence numbers to UIDs */
    virtual void setUidMapping( const QString& mailbox, const QList<uint>& seqToUid ) = 0;
    /** @short Forget the cached seq->UID mapping for given mailbox */
    virtual void clearUidMapping( const QString& mailbox ) = 0;

    /** @short Remove all messages in given mailbox from the cache */
    virtual void clearAllMessages( const QString& mailbox ) = 0;
    /** @short Remove all info for given message in the mailbox from cache */
    virtual void clearMessage( const QString mailbox, uint uid ) = 0;
    /** @short Save data for one message part */
    virtual void setMsgPart( const QString& mailbox, uint uid, const QString& partId, const QByteArray& data ) = 0;
    /** @short Store the ENVELOPE of one message */
    virtual void setMsgEnvelope( const QString& mailbox, uint uid, const Imap::Message::Envelope& envelope ) = 0;
    /** @short Save the message size */
    virtual void setMsgSize( const QString& mailbox, uint uid, uint size ) = 0;
    /** @short Save the BODYSTRUCTURE of a message */
    virtual void setMsgStructure( const QString& mailbox, uint uid, const QByteArray& serializedData ) = 0;
    /** @short Save flags for one message in mailbox */
    virtual void setMsgFlags( const QString& mailbox, uint uid, const QStringList& flags ) = 0;

    /** @short Retrieve sequence to UID mapping */
    virtual QList<uint> uidMapping( const QString& mailbox ) = 0;

    /** @short Returns all known data for a message in the given mailbox (except real parts data) */
    virtual MessageDataBundle messageMetadata( const QString& mailbox, uint uid ) = 0;

    /** @short Return part data or a null QByteArray if none available */
    virtual QByteArray messagePart( const QString& mailbox, uint uid, const QString& partId ) = 0;
};

/** @short A convenience typedef */
typedef QSharedPointer<AbstractCache> CachePtr;

}

}

#endif /* IMAP_MODEL_CACHE_H */
