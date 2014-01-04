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

#ifndef XTCONNECT_XTCACHE
#define XTCONNECT_XTCACHE

#include "Imap/Model/Cache.h"

namespace Imap {
namespace Mailbox {
class SQLCache;
}
}

namespace XtConnect {

/** @short A special-purpose cache for XtConnect

This cache is pretty similar to Imap::Mailbox::CombinedCache in that it leverages existing
features SQLCache for most of its tasks.  However, features which are not important in the
context of XtConnect (remembering mailboxes, storing message parts etc) are not implemented
at all.

Please see note in AbstractCache's documentation about a potential for extending the caching
setup with a reworked MemoryCache; this will have to be done when the actual TreeItem* stop
storing data inside themselves.
*/
class XtCache : public Imap::Mailbox::AbstractCache {
    Q_OBJECT
public:
    /** @short Constructor

      Create new instance, using the @arg name as the name for the database connection.
      Store all data into the @arg cacheDir directory. Actual opening of the DB connection
      is deferred till a call to the load() method.
*/
    XtCache( QObject* parent, const QString& name, const QString& cacheDir );

    virtual ~XtCache();

    /** @short Do nothing */
    virtual QList<Imap::Mailbox::MailboxMetadata> childMailboxes( const QString& mailbox ) const;
    /** @short Returns false */
    virtual bool childMailboxesFresh( const QString& mailbox ) const;
    /** @short Do nothing */
    virtual void setChildMailboxes( const QString& mailbox, const QList<Imap::Mailbox::MailboxMetadata>& data );
    /** @short Do nothing */
    virtual void forgetChildMailboxes( const QString& mailbox );

    virtual Imap::Mailbox::SyncState mailboxSyncState( const QString& mailbox ) const;
    virtual void setMailboxSyncState( const QString& mailbox, const Imap::Mailbox::SyncState& state );

    virtual void setUidMapping( const QString& mailbox, const QList<uint>& seqToUid );
    virtual void clearUidMapping( const QString& mailbox );
    virtual QList<uint> uidMapping( const QString& mailbox ) const;

    virtual void clearAllMessages( const QString& mailbox );
    virtual void clearMessage( const QString mailbox, uint uid );

    virtual MessageDataBundle messageMetadata( const QString& mailbox, uint uid ) const;
    virtual void setMessageMetadata( const QString& mailbox, uint uid, const MessageDataBundle& metadata );

    /** @short Do nothing */
    virtual QStringList msgFlags( const QString& mailbox, uint uid ) const;
    /** @short Returns no data */
    virtual void setMsgFlags( const QString& mailbox, uint uid, const QStringList& flags );

    /** @short ALways returns an empty QByteArray */
    virtual QByteArray messagePart( const QString& mailbox, uint uid, const QString& partId ) const;
    /** @short Do nothing */
    virtual void setMsgPart( const QString& mailbox, uint uid, const QString& partId, const QByteArray& data );

    /** @short Do nothing */
    virtual QVector<Imap::Responses::ThreadingNode> messageThreading(const QString &mailbox);
    /** @short Do nothing */
    virtual void setMessageThreading(const QString &mailbox, const QVector<Imap::Responses::ThreadingNode> &threading);

    /** @short Open a connection to the cache */
    bool open();

    void setRenewalThreshold(const int days);

    /** @short Saving status of a message */
    typedef enum {
        STATE_SAVED, /**< Message has been already saved into the DB */
        STATE_DUPLICATE, /**< A duplicate message is already in the DB */
        STATE_UNKNOWN /**< The DB doesn't know anything about this message */
    } SavingState;

    /** @short Has it been stored in the DB already? */
    SavingState messageSavingStatus( const QString &mailbox, const uint uid ) const;
    /** @short Set message saving status */
    void setMessageSavingStatus( const QString &mailbox, const uint uid, const SavingState status );

private:
    /** @short The SQL-based cache */
    Imap::Mailbox::SQLCache* _sqlCache;
    /** @short Name of the DB connection */
    QString _name;
    /** @short Directory to serve as a cache root */
    QString _cacheDir;
};

}

#endif /* XTCONNECT_XTCACHE */
