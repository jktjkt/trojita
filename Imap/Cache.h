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

#ifndef IMAP_CACHE_H
#define IMAP_CACHE_H

#include <tr1/memory>
#include <QtGlobal>

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

/** @short An abstract parent for all IMAP cache implementations */
class AbstractCache {
public:
    virtual ~AbstractCache() {};

    /** @short Server sends us new UIDNEXT */
    virtual void setNewNumbers( const uint uidValidity, const uint uidNext, const uint exists ) = 0;

    /** @short Throw away all cached information */
    virtual void forget() = 0;

    /** @short Get stored (old?) value of UIDNEXT */
    virtual uint getUidNext() = 0;

    /** @short Get stored (old?) value of EXISTS */
    virtual uint getExists() = 0;

    /** @short Get stored (old?) value of UIDVALIDITY */
    virtual uint getUidValidity() = 0;

    virtual bool seqToUid( const uint seq, uint& uid ) = 0;
    virtual bool uidToSeq( const uint uid, uint& seq ) = 0;
    virtual void addSeqUid( const uint seq, const uint uid ) = 0;

    virtual void forgetSeqUid() = 0;
};

/** @short A cache implementation that actually doesn't cache anything */
class NoCache : public AbstractCache {
    uint _uidNext, _uidValidity, _exists;
public:
    NoCache();
    virtual void setNewNumbers( const uint uidValidity, const uint uidNext, const uint exists );
    virtual void forget();
    virtual uint getUidNext();
    virtual uint getExists();
    virtual uint getUidValidity();
    virtual bool seqToUid( const uint seq, uint& uid );
    virtual bool uidToSeq( const uint uid, uint& seq );
    virtual void addSeqUid( const uint seq, const uint uid );
    virtual void forgetSeqUid();
};

/** @short A convenience typedef */
typedef std::tr1::shared_ptr<AbstractCache> CachePtr;

}

}

#endif /* IMAP_CACHE_H */
