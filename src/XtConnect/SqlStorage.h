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


#ifndef SQLSTORAGE_H
#define SQLSTORAGE_H

#include <QDateTime>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "Common/SqlTransactionAutoAborter.h"
#include "xsqlquery.h"

class QTimer;

namespace XtConnect {

/** @short Access to the XtConnect database for e-mails */
class SqlStorage : public QObject
{
    Q_OBJECT
public:

    typedef enum { RESULT_OK, RESULT_DUPLICATE, RESULT_ERROR } ResultType;

    explicit SqlStorage(QObject *parent, const QString &host, const int port, const QString &dbname, const QString &username, const QString &password);
    void open();

    /** @short Save mail data to the "eml" table */
    ResultType insertMail( const QDateTime &dateTime, const QString &subject, const QString &readableText, const QByteArray &headers, const QByteArray &body, quint64 &emlId );
    /** @short Insert an e-mail address into the eml_emladdr table */
    ResultType insertAddress( const quint64 emlId, const QString &name, const QString &address, const QLatin1String kind );
    /** @short Mark the row in the eml table as "ready for processing" */
    ResultType markMailReady( const quint64 emlId );

    /** @short Return an object which aborts the transaction upon its destruction (RIAA-like approach to transactions) */
    Common::SqlTransactionAutoAborter transactionGuard();
    /** @short Log a message saying that something talking to the DB failed */
    void fail( const QString &message );

signals:
    void encounteredError(const QString &message);

private slots:
    /** @short Record a failure and optionally reconnect if too many errors happened since last reconnect */
    void slotReconnect();

private:
    void _prepareStatements();
    void _fail( const QString &message, const QSqlQuery &query );
    void _fail( const QString &message, const QSqlDatabase &database );

    QSqlDatabase db;
    XSqlQuery _queryValidateMail;
    XSqlQuery _queryInsertMail;
    XSqlQuery _queryInsertAddress;
    QSqlQuery _queryMarkMailReady;

    QTimer *reconnect;

    QString _host;
    int _port;
    QString _dbname;
    QString _username;
    QString _password;
};

}

#endif // SQLSTORAGE_H
