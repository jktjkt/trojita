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
#include <iostream>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QSqlError>
#include <QStringList>
#include <QTimer>
#include <QVariant>

namespace XtConnect {

SqlStorage::SqlStorage(QObject *parent) :
    QObject(parent)
{
    reconnect = new QTimer( this );
    reconnect->setSingleShot( true );
    reconnect->setInterval( 10 * 1000 );
    connect( reconnect, SIGNAL(timeout()), this, SLOT(slotReconnect()) );
}

void SqlStorage::open()
{
    QString host;
    int     port = -1;
    QString dbname;
    QString username;
    QString password;
    bool    readstdin = false;

    QStringList args = QCoreApplication::arguments();
    for ( int i = 1; i < args.length(); i++ ) {
        if (args.at(i) == "-h" && args.length() > i)
            host = args.at(++i);
        else if (args.at(i) == "-d" && args.length() > i)
            dbname = args.at(++i);
        else if (args.at(i) == "-p" && args.length() > i)
            port = args.at(++i).toInt();
        else if (args.at(i) == "-U" && args.length() > i)
            username = args.at(++i);
        else if (args.at(i) == "-W")
            readstdin = true;
    }

    for ( int i = 0; i < 3 && username.isEmpty() && readstdin; i++ ) {
        std::string rawinput;
        std::cout << "User: ";
        std::cin >> rawinput;
        username = QString(rawinput.c_str()).trimmed();
    }

    for ( int i = 0; i < 3 && password.isEmpty() && readstdin; i++ ) {
        std::string rawinput;
        std::cout << "Password: ";
        std::cin >> rawinput;
        password = QString(rawinput.c_str()).trimmed();
    }

    db = QSqlDatabase::addDatabase( QLatin1String("QPSQL"), QLatin1String("xtconnect-sqlstorage") );
    if ( ! host.isEmpty() )
        db.setHostName(host);

    if ( port != -1 )
        db.setPort(port);

    if ( ! dbname.isEmpty() )
        db.setDatabaseName( dbname );

    if ( ! username.isEmpty() )
        db.setUserName( username );

    if ( ! password.isEmpty() )
        db.setPassword( password );

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
                                                   "SELECT ?, ?, ?, ?, ?, 'I' WHERE NOT EXISTS "
                                                   " ( SELECT eml_id FROM xtbatch.eml WHERE eml_hash = ? ) "
                                                   "RETURNING eml_id") ) )
        _fail( "Failed to prepare query _queryInsertMail", _queryInsertMail );

    _queryInsertAddress = QSqlQuery(db);
    if ( ! _queryInsertAddress.prepare( QLatin1String("INSERT INTO xtbatch.emladdr "
                                                      "(emladdr_eml_id, emladdr_type, emladdr_addr, emladdr_name) "
                                                      "VALUES (?, ?, ?, ?)") ) )
        _fail( "Failed to prepare query _queryInsertAddress", _queryInsertAddress );

    _queryMarkMailReady = QSqlQuery(db);
    if ( ! _queryMarkMailReady.prepare( QLatin1String("UPDATE xtbatch.eml SET eml_status = 'O' WHERE eml_id = ?") ) )
        _fail( "Failed to prepare query _queryMarkMailReady", _queryMarkMailReady );
}

SqlStorage::ResultType SqlStorage::insertMail( const QDateTime &dateTime, const QString &subject, const QString &readableText, const QByteArray &headers, const QByteArray &body, quint64 &emlId )
{
    QCryptographicHash hash( QCryptographicHash::Sha1 );
    hash.addData( body );
    QByteArray hashValue = hash.result();
    _queryInsertMail.bindValue( 0, hashValue );
    _queryInsertMail.bindValue( 1, dateTime );
    _queryInsertMail.bindValue( 2, subject );
    _queryInsertMail.bindValue( 3, readableText );
    _queryInsertMail.bindValue( 4, headers + body );
    _queryInsertMail.bindValue( 5, hashValue );

    if ( ! _queryInsertMail.exec() ) {
        _fail( "Query _queryInsertMail failed", _queryInsertMail );
        return RESULT_ERROR;
    }

    if ( _queryInsertMail.first() ) {
        emlId = _queryInsertMail.value( 0 ).toULongLong();
        return RESULT_OK;
    } else {
        return RESULT_DUPLICATE;
    }
}

void SqlStorage::_fail( const QString &message, const QSqlQuery &query )
{
    if ( ! db.isOpen() )
        reconnect->start();
    qWarning() << QString::fromAscii("SqlStorage: Query Error: %1: %2").arg( message, query.lastError().text() );
}

void SqlStorage::_fail( const QString &message, const QSqlDatabase &database )
{
    if ( ! db.isOpen() )
        reconnect->start();
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

SqlStorage::ResultType SqlStorage::insertAddress( const quint64 emlId, const QString &name, const QString &address, const QLatin1String kind )
{
    _queryInsertAddress.bindValue( 0, emlId );
    _queryInsertAddress.bindValue( 1, kind );
    _queryInsertAddress.bindValue( 2, address );
    _queryInsertAddress.bindValue( 3, name );

    if ( ! _queryInsertAddress.exec() ) {
        _fail( "Query _queryInsertAddress failed", _queryInsertAddress );
        return RESULT_ERROR;
    }

    return RESULT_OK;
}

SqlStorage::ResultType SqlStorage::markMailReady( const quint64 emlId )
{
    _queryMarkMailReady.bindValue( 0, emlId );

    if ( ! _queryMarkMailReady.exec() ) {
        _fail( "Query _queryMarkMailReady failed", _queryMarkMailReady );
        return RESULT_ERROR;
    }

    return RESULT_OK;
}

void SqlStorage::slotReconnect()
{
    qDebug() << "Trying to reconnect to the database...";
    // Release all DB resources
    _queryInsertAddress.clear();
    _queryInsertMail.clear();
    _queryMarkMailReady.clear();
    db.close();

    // Unregister the DB
    QSqlDatabase::removeDatabase( QLatin1String("xtconnect-sqlstorage") );

    open();
}

}
