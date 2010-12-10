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

namespace XtConnect {

class SqlStorage : public QObject
{
    Q_OBJECT
public:

    typedef enum { RESULT_OK, RESULT_DUPLICATE, RESULT_ERROR } ResultType;

    explicit SqlStorage(QObject *parent = 0);
    void open();

    QVariant insertMail( const QDateTime &dateTime, const QString &subject, const QString &plainBody, const QByteArray &data, ResultType &result );
    void insertAddress( const quint64 emlId, const QString &name, const QString &address, const QLatin1String kind, ResultType &result );

    Common::SqlTransactionAutoAborter transactionGuard();
    void fail( const QString &message );

private:
    void _prepareStatements();
    void _fail( const QString &message, const QSqlQuery &query );
    void _fail( const QString &message, const QSqlDatabase &database );

    QSqlDatabase db;
    QSqlQuery _queryInsertMail;
    QSqlQuery _queryInsertAddress;
};

}

#endif // SQLSTORAGE_H
