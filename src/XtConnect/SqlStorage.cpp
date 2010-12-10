/*
    Certain enhancements (www.xtuple.com/trojita-enhancements)
    are copyright © 2010 by OpenMFG LLC, dba xTuple.  All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    - Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    - Neither the name of xTuple nor the names of its contributors may be used to
    endorse or promote products derived from this software without specific prior
    written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
    ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "SqlStorage.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QSqlError>
#include <QVariant>

namespace XtConnect {

SqlStorage::SqlStorage(QObject *parent) :
    QObject(parent)
{
}

void SqlStorage::open()
{
    db = QSqlDatabase::addDatabase( QLatin1String("QPSQL"), QLatin1String("xtconnect-sqlstorage") );
    db.setUserName( "xtrole" );
    db.setDatabaseName("xtbatch");

    if ( ! db.open() ) {
        _fail( "Failed to open database connection", db );
    }

    _prepareStatements();
}

void SqlStorage::_prepareStatements()
{
    _queryInsertMail = QSqlQuery(db);
    if ( ! _queryInsertMail.prepare( QLatin1String("INSERT INTO xtbatch.eml "
                                                   "(eml_hash, eml_date, eml_subj, eml_body, eml_msg, eml_status) "
                                                   "VALUES (?, ?, ?, ?, ?, 'I') RETURNING eml_id") ) )
        _fail( "Failed to prepare query _queryInsertMail", _queryInsertMail );

    _queryInsertAddress = QSqlQuery(db);
    if ( ! _queryInsertAddress.prepare( QLatin1String("INSERT INTO xtbatch.emladdr "
                                                      "(emladdr_eml_id, emladdr_type, emladdr_addr, emladdr_name) "
                                                      "VALUES (?, ?, ?, ?)") ) )
        _fail( "Failed to prepare query _queryInsertAddress", _queryInsertAddress );
}

QVariant SqlStorage::insertMail( const QDateTime &dateTime, const QString &subject, const QString &plainBody, const QByteArray &data, ResultType &result )
{
    QCryptographicHash hash( QCryptographicHash::Sha1 );
    hash.addData( data );
    _queryInsertMail.bindValue( 0, hash.result() );
    _queryInsertMail.bindValue( 1, dateTime );
    _queryInsertMail.bindValue( 2, subject );
    _queryInsertMail.bindValue( 3, plainBody );
    _queryInsertMail.bindValue( 4, data );

    if ( ! _queryInsertMail.exec() ) {
        QString errorMessage = _queryInsertMail.lastError().databaseText();
        if ( errorMessage.contains( QLatin1String("duplicate") ) && errorMessage.contains( QLatin1String("eml_eml_hash_key") )) {
            result = RESULT_DUPLICATE;
            return QVariant();
        } else {
            result = RESULT_ERROR;
            _fail( "Query _queryInsertMail failed", _queryInsertMail );
            return QVariant();
        }
    }

    if ( _queryInsertMail.first() ) {
        result = RESULT_OK;
        return _queryInsertMail.value( 0 );
    } else {
        result = RESULT_ERROR;
        qWarning() << "Query returned no data...";
        return QVariant();
    }
}

void SqlStorage::_fail( const QString &message, const QSqlQuery &query )
{
    qWarning() << QString::fromAscii("SqlStorage: Query Error: %1: %2").arg( message, query.lastError().text() );
}

void SqlStorage::_fail( const QString &message, const QSqlDatabase &database )
{
    qWarning() << QString::fromAscii("SqlStorage: Query Error: %1: %2").arg( message, database.lastError().text() );
}

void SqlStorage::fail( const QString &message )
{
    _fail( message, db );
}

Common::SqlTransactionAutoAborter SqlStorage::transactionGuard()
{
    return Common::SqlTransactionAutoAborter( &db );
}

void SqlStorage::insertAddress( const quint64 emlId, const QString &name, const QString &address, const QLatin1String kind, ResultType &result )
{
    _queryInsertAddress.bindValue( 0, emlId );
    _queryInsertAddress.bindValue( 1, kind );
    _queryInsertAddress.bindValue( 2, address );
    _queryInsertAddress.bindValue( 3, name );

    if ( ! _queryInsertAddress.exec() ) {
        result = SqlStorage::RESULT_ERROR;
        _fail( "Query _queryInsertAddress failed", _queryInsertAddress );
    }
}

}
