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

#include "SQLCache.h"
#include <QSqlError>
#include <QSqlRecord>

namespace {
    class TransactionHelper {
    public:
        TransactionHelper( QSqlDatabase* db ): _db(db), _commited(false)
        {
            _db->transaction();
        }
        ~TransactionHelper()
        {
            if ( ! _commited )
                _db->rollback();
        }
        void commit()
        {
            _db->commit();
            _commited = true;
        }
    private:
        QSqlDatabase* _db;
        bool _commited;
    };
}

namespace Imap {
namespace Mailbox {

SQLCache::SQLCache( const QString& name, const QString& fileName ):
        QObject(0)
{
    db = QSqlDatabase::addDatabase( QLatin1String("QSQLITE"), name );
    db.setDatabaseName( fileName );
    open();
}

SQLCache::~SQLCache()
{
    db.close();
}

bool SQLCache::open()
{
    bool ok = db.open();
    if ( ! ok ) {
        emitError( tr("Can't open database"), db );
        return false;
    }

    TransactionHelper txn( &db );
    QSqlQuery q( QString(), db );

    QSqlRecord trojitaNames = db.record( QLatin1String("trojita") );
    if ( ! trojitaNames.contains( QLatin1String("version") ) ) {
        if ( ! q.exec( QLatin1String("CREATE TABLE trojita ( version STRING NOT NULL )") ) ) {
            emitError( tr("Failed to prepare table structures"), q );
            return false;
        }
        if ( ! q.exec( QLatin1String("INSERT INTO trojita ( version ) VALUES ( 1 )") ) ) {
            emitError( tr("Can't store version info"), q );
            return false;
        }
        if ( ! q.exec( QLatin1String(
                "CREATE TABLE child_mailboxes ( "
                "mailbox STRING NOT NULL PRIMARY KEY, "
                "parent STRING NOT NULL, "
                "separator STRING NOT NULL, "
                "flags BINARY NOT NULL"
                ")"
                ) ) ) {
            emitError( tr("Can't create table child_mailboxes") );
            return false;
        }
        if ( ! q.exec( QLatin1String("CREATE TABLE child_mailboxes_fresh ( mailbox STRING NOT NULL PRIMARY KEY )") ) ) {
            emitError( tr("Can't create table child_mailboxes_fresh") );
            return false;
        }
    }

    if ( ! q.exec( QLatin1String("SELECT version FROM trojita") ) ) {
        emitError( tr("Failed to verify version"), q );
        return false;
    }

    if ( ! q.first() ) {
        // we could probably relax this...
        emitError( tr("Can't determine version info"), q );
        return false;
    }

    uint version = q.value(0).toUInt();
    if ( version != 1 ) {
        emitError( tr("Unknown version"));
        return false;
    }

    txn.commit();

    queryChildMailboxes = QSqlQuery(db);
    if ( ! queryChildMailboxes.prepare( "SELECT mailbox, parent, separator, flags FROM child_mailboxes WHERE mailbox = ?") ) {
        emitError( tr("Failed to prepare queryChildMailboxes"), queryChildMailboxes );
        return false;
    }

    queryChildMailboxesFresh = QSqlQuery(db);
    if ( ! queryChildMailboxesFresh.prepare( QLatin1String("SELECT mailbox FROM child_mailboxes_fresh WHERE mailbox = ?") ) ) {
        emitError( tr("Failed to prepare queryChildMailboxesFresh"), queryChildMailboxesFresh );
        return false;
    }

    return true;
}

void SQLCache::emitError( const QString& message, const QSqlQuery& query ) const
{
    emitError( QString::fromAscii("SQLCache: Query Error: %1: %2").arg( message, query.lastError().text() ) );
}

void SQLCache::emitError( const QString& message, const QSqlDatabase& database ) const
{
    emitError( QString::fromAscii("SQLCache: DB Error: %1: %2").arg( message, database.lastError().text() ) );
}

void SQLCache::emitError( const QString& message ) const
{
    qDebug() << message;
    emit databaseError( message );
}

QList<MailboxMetadata> SQLCache::childMailboxes( const QString& mailbox ) const
{
    // FIXME
    return QList<MailboxMetadata>();
}

bool SQLCache::childMailboxesFresh( const QString& mailbox ) const
{
    queryChildMailboxesFresh.bindValue( 0, mailbox );
    if ( ! queryChildMailboxesFresh.exec() ) {
        emitError( tr("Query queryChildMailboxesFresh failed"), queryChildMailboxesFresh );
        return false;
    }
    return queryChildMailboxesFresh.first();
}

void SQLCache::setChildMailboxes( const QString& mailbox, const QList<MailboxMetadata>& data )
{
    // FIXME
}

void SQLCache::forgetChildMailboxes( const QString& mailbox )
{
    // FIXME
}

SyncState SQLCache::mailboxSyncState( const QString& mailbox ) const
{
    // FIXME
    return SyncState();
}

void SQLCache::setMailboxSyncState( const QString& mailbox, const SyncState& state )
{
    // FIXME
}

void SQLCache::setUidMapping( const QString& mailbox, const QList<uint>& seqToUid )
{
    // FIXME
}

void SQLCache::clearUidMapping( const QString& mailbox )
{
    // FIXME
}

void SQLCache::clearAllMessages( const QString& mailbox )
{
    // FIXME
}

void SQLCache::clearMessage( const QString mailbox, uint uid )
{
    // FIXME
}

void SQLCache::setMsgPart( const QString& mailbox, uint uid, const QString& partId, const QByteArray& data )
{
    // FIXME
}

void SQLCache::setMsgEnvelope( const QString& mailbox, uint uid, const Imap::Message::Envelope& envelope )
{
    // FIXME
}

void SQLCache::setMsgSize( const QString& mailbox, uint uid, uint size )
{
    // FIXME
}

void SQLCache::setMsgStructure( const QString& mailbox, uint uid, const QByteArray& serializedData )
{
    // FIXME
}

void SQLCache::setMsgFlags( const QString& mailbox, uint uid, const QStringList& flags )
{
    // FIXME
}

QList<uint> SQLCache::uidMapping( const QString& mailbox )
{
    // FIXME
    return QList<uint>();
}

AbstractCache::MessageDataBundle SQLCache::messageMetadata( const QString& mailbox, uint uid )
{
    // FIXME
    return AbstractCache::MessageDataBundle();
}

QByteArray SQLCache::messagePart( const QString& mailbox, uint uid, const QString& partId )
{
    // FIXME
    return QByteArray();
}

}
}
