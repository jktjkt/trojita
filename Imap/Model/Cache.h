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

#include <tr1/memory>
#include "MailboxMetadata.h"

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

/** @short An abstract parent for all IMAP cache implementations */
class AbstractCache {
public:
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
};

/** @short A convenience typedef */
typedef std::tr1::shared_ptr<AbstractCache> CachePtr;

}

}

#endif /* IMAP_MODEL_CACHE_H */
