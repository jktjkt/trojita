/*
 * OpenRPT report writer and rendering engine
 * Copyright (C) 2001-2010 by OpenMFG, LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * Please contact info@openmfg.com with any questions on this license.
 */

#ifndef __XSQLQUERY_H__
#define __XSQLQUERY_H__

#include <QSqlQuery>

class XSqlQueryPrivate;
class QSqlError;


class XSqlQueryErrorListener {
  public:
    XSqlQueryErrorListener();
    virtual ~XSqlQueryErrorListener();
    virtual void error(const QString &, const QSqlError&) = 0;
};

class XSqlQuery : public QSqlQuery
{
  public:
    XSqlQuery();
    explicit XSqlQuery(QSqlDatabase);
    explicit XSqlQuery(QSqlResult *);
    explicit XSqlQuery(const QString &, QSqlDatabase = QSqlDatabase());
    explicit XSqlQuery(const QSqlQuery &);
    XSqlQuery(const XSqlQuery &);
    virtual ~XSqlQuery();
    XSqlQuery & operator=(const XSqlQuery &);

    virtual QVariant value(int i) const;
    virtual QVariant value(const QString &) const;

    virtual bool first();
    virtual bool next();
    virtual bool previous();
    virtual bool prev();

    virtual int count();

    virtual bool prepare(const QString &);

    virtual bool exec();
    bool exec(const QString &);

    virtual int findFirst(int, int);
    virtual int findFirst(const QString &, int);
    virtual int findFirst(const QString &, const QString &);

    void trackFieldTotal(QString &);
    double getFieldTotal(QString &);
    double getFieldSubTotal(QString &);
    void resetSubTotals();
    void resetSubTotalsCurrent();

    bool emulatePrepare() const;
    void setEmulatePrepare(bool);

    static void addErrorListener(XSqlQueryErrorListener*);
    static void removeErrorListener(XSqlQueryErrorListener*);
    static void setNameErrorValue(QString v);

  private:
    XSqlQueryPrivate * _data;
};

#endif
