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

#include <QSqlRecord>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QSqlDriver>
#include <QSqlResult>
#include <QCursor>
#include <QMap>

#include "xsqlquery.h"

class XSqlResultHelper : QSqlResult
{
  friend class XSqlQuery;

  protected:
    XSqlResultHelper(const QSqlDriver * db) : QSqlResult(db) {}

    void setLastError(const QSqlError& e) { QSqlResult::setLastError(e); }
    bool exec() { return QSqlResult::exec(); }
    void setActive(bool a) { QSqlResult::setActive(a); }
    void setAt(int at) { QSqlResult::setAt(at); }
    bool savePrepare(const QString& sqlquery) { clear(); return QSqlResult::prepare(sqlquery); }
};

class XSqlQueryPrivate {
public:
  XSqlQueryPrivate(XSqlQuery * parent)
  {
    _emulatePrepare = false;
    if(parent->driver())
    {
      QVariant v = parent->driver()->handle();
      if(qstrcmp(v.typeName(), "PGconn*")==0)
        _emulatePrepare = true;
    }
    _keepTotals = false;
  }
  XSqlQueryPrivate(const XSqlQueryPrivate & p)
  {
    *this = p;
  }
  virtual ~XSqlQueryPrivate() {}

  XSqlQueryPrivate & operator=(const XSqlQueryPrivate & p)
  {
    _emulatePrepare = p._emulatePrepare;
    _fieldTotals = p._fieldTotals;
    _fieldSubTotals = p._fieldSubTotals;
    _currRecord = p._currRecord;
    _keepTotals = p._keepTotals;
    return *this;
  }

  bool _emulatePrepare;

  QMap<QString, double> _fieldTotals;
  QMap<QString, double> _fieldSubTotals;

  QSqlRecord    _currRecord;
  bool          _keepTotals;
};

static QList<XSqlQueryErrorListener*> _errorListeners;

static void notifyErrorListeners(XSqlQuery * source)
{
  if(!source)
    return;

  XSqlQueryErrorListener * listener = 0;
  for(int i = 0; i < _errorListeners.size(); i++)
  {
    listener = _errorListeners.at(i);
    if(listener)
      listener->error(source->executedQuery(), source->lastError());
  }
}


XSqlQueryErrorListener::XSqlQueryErrorListener() {}
XSqlQueryErrorListener::~XSqlQueryErrorListener()
{
  XSqlQuery::removeErrorListener(this);
}


static QString _nameErrorValue;

void XSqlQuery::setNameErrorValue(QString v)
{
    _nameErrorValue = v;
}


XSqlQuery::XSqlQuery() :
  QSqlQuery()
{
  _data = new XSqlQueryPrivate(this);
}

XSqlQuery::XSqlQuery(QSqlDatabase db) :
  QSqlQuery(db)
{
  _data = new XSqlQueryPrivate(this);
}

XSqlQuery::XSqlQuery(QSqlResult * r) :
  QSqlQuery(r)
{
  _data = new XSqlQueryPrivate(this);
}

XSqlQuery::XSqlQuery(const QString &pSql, QSqlDatabase db) :
  QSqlQuery(QString(), db)
{
  _data = new XSqlQueryPrivate(this);
  exec(pSql.toAscii().data());
}

XSqlQuery::XSqlQuery(const QSqlQuery & other) :
  QSqlQuery(other)
{
  _data = new XSqlQueryPrivate(this);
}

XSqlQuery::XSqlQuery(const XSqlQuery & other) :
  QSqlQuery(other)
{
  _data = new XSqlQueryPrivate(this);
  if (other._data)
    _data = new XSqlQueryPrivate(*other._data);
}

XSqlQuery::~XSqlQuery()
{
  if (_data)
    delete _data;
  _data = 0;
}

XSqlQuery & XSqlQuery::operator=(const XSqlQuery & other)
{
  if (other._data)
  {
    if (_data)
      *_data = *other._data;
    else
      _data = new XSqlQueryPrivate(*other._data);
  }
  else
  {
    if (_data)
      delete _data;
    _data = new XSqlQueryPrivate(this);
  }
  
  QSqlQuery::operator=(other);
  return *this;
}

QVariant XSqlQuery::value(int i) const
{
  return QSqlQuery::value(i);
}

QVariant XSqlQuery::value(const QString & name) const
{
    if (name.isEmpty())
        return QVariant();

    if (_data && !_data->_currRecord.isEmpty())
    {
        int i = _data->_currRecord.indexOf(name);
        if(i<0)
        {
            QString err = "Column " + name + " not found in record";
            qWarning("%s", err.toLocal8Bit().constData());
            return QVariant(_nameErrorValue);
        }
        return value(_data->_currRecord.indexOf(name));
    }

    return QVariant();
}

int XSqlQuery::count()
{
  QSqlRecord rec = record();

  if (!rec.isEmpty())
    return rec.count();
  else
    return 0;
}

bool XSqlQuery::exec()
{
  bool returnValue = false;

  if(_data && _data->_emulatePrepare)
  {
// In 4.4.1 Qt started supporting true prepared queries on the PostgreSQL driver and this
// caused several problems with all our code and the way it worked so this is a modified copy
// of their code to use the implemented prepare if we have that option set so we can use the method
// that works best in the case we are using it for.
    if (lastError().isValid())
      ((XSqlResultHelper*)result())->setLastError(QSqlError());

    returnValue = ((XSqlResultHelper*)result())->XSqlResultHelper::exec();
  }
  else
    returnValue = QSqlQuery::exec();

  if (_data)
    _data->_currRecord = record();

  if(false == returnValue)
    notifyErrorListeners(this);

  return returnValue;
}

bool XSqlQuery::exec(const QString &pSql)
{
  bool returnValue = QSqlQuery::exec(pSql);

  if (_data)
    _data->_currRecord = record();

  if(false == returnValue)
    notifyErrorListeners(this);

  return returnValue;
}

bool XSqlQuery::prepare(const QString &pSql)
{
  bool ret;
  if(_data && _data->_emulatePrepare)
  {
// In 4.4.1 Qt started supporting true prepared queries on the PostgreSQL driver and this
// caused several problems with all our code and the way it worked so this is a modified copy
// of their code to use the implemented prepare if we have that option set so we can use the method
// that works best in the case we are using it for.
    ((XSqlResultHelper*)result())->setActive(false);
    ((XSqlResultHelper*)result())->setLastError(QSqlError());
    ((XSqlResultHelper*)result())->setAt(QSql::BeforeFirstRow);
    if (!driver()) {
      qWarning("XSqlQuery::prepare: no driver");
      return false;
    }
    if (!driver()->isOpen() || driver()->isOpenError()) {
      qWarning("XSqlQuery::prepare: database not open");
      return false;
    }
    if (pSql.isEmpty()) {
      qWarning("XSqlQuery::prepare: empty query");
      return false;
    }
#ifdef QT_DEBUG_SQL
    qDebug("\n XSqlQuery::prepare: %s", query.toLocal8Bit().constData());
#endif
    ret = ((XSqlResultHelper*)result())->XSqlResultHelper::savePrepare(pSql);
  }
  else
    ret = QSqlQuery::prepare(pSql);
  if(ret && ((driver() && !driver()->hasFeature(QSqlDriver::PreparedQueries)) || (_data && _data->_emulatePrepare)))
    bindValue(":firstnullfix", QVariant());
  return ret;
}

bool XSqlQuery::first()
{
  if (QSqlQuery::first())
  {
    if (_data)
    {
      if (_data->_keepTotals)
      {
        // initial all our values
        resetSubTotals();
        QMapIterator<QString,double> mit(_data->_fieldTotals);
        while(mit.hasNext())
        {
          mit.next();
          _data->_fieldTotals[mit.key()] = value(mit.key()).toDouble();
          _data->_fieldSubTotals[mit.key()] = value(mit.key()).toDouble();
        }
      }
      _data->_currRecord = record();
    }
    return true;
  }
  return false;
}

bool XSqlQuery::next()
{
  if (QSqlQuery::next())
  {
    if (_data)
    {
      if (_data->_keepTotals)
      {
        // increment all our values
        QMapIterator<QString,double> mit(_data->_fieldTotals);
        while(mit.hasNext())
        {
          mit.next();
          _data->_fieldTotals[mit.key()] += value(mit.key()).toDouble();
          _data->_fieldSubTotals[mit.key()] += value(mit.key()).toDouble();
        }
      }
      _data->_currRecord = record();
    }
    return true;
  }
  return false;
}

bool XSqlQuery::previous()
{
  if (!_data)
    return QSqlQuery::previous();

  bool returnVal = false;

  if (_data->_keepTotals && isValid())
  {
    QMap<QString,double> delta;
    QMapIterator<QString,double> mit(_data->_fieldTotals);
    while(mit.hasNext())
    {
      mit.next();
      delta[mit.key()] = value(mit.key()).toDouble();
    }
    returnVal = QSqlQuery::previous();
    if (returnVal)
    {
      mit = delta;
      while(mit.hasNext())
      {
        mit.next();
        _data->_fieldTotals[mit.key()] -= mit.value();
        _data->_fieldSubTotals[mit.key()] -= mit.value();
      }
    }
  }
  else
    returnVal = QSqlQuery::previous();

  _data->_currRecord = record();
  return returnVal;
}

bool XSqlQuery::prev()
{
  return previous();
}

void XSqlQuery::trackFieldTotal(QString & fld)
{
  if (!_data)
    _data = new XSqlQueryPrivate(this);

  _data->_keepTotals = true;

  if (!_data->_fieldTotals.contains(fld))
      _data->_fieldTotals[fld] = 0.0;

  if (!_data->_fieldSubTotals.contains(fld))
      _data->_fieldSubTotals[fld] = 0.0;
}

double XSqlQuery::getFieldTotal(QString & fld)
{
  if (_data)
  {
    if (_data->_fieldTotals.contains(fld))
      return _data->_fieldTotals[fld];
  }
  return 0.0;
}

double XSqlQuery::getFieldSubTotal(QString & fld)
{
  if (_data)
    if (_data->_fieldSubTotals.contains(fld))
      return _data->_fieldSubTotals[fld];

  return 0.0;
}

void XSqlQuery::resetSubTotals()
{
  if (_data)
  {
    // initial all our values to 0.0
    QMapIterator<QString,double> mit(_data->_fieldSubTotals);
    while(mit.hasNext())
    {
      mit.next();
      _data->_fieldSubTotals[mit.key()] = 0.0;
    }
  }
}

void XSqlQuery::resetSubTotalsCurrent()
{
  if (_data)
  {
    // initial all our values to the absolute value of the current record
    QMapIterator<QString,double> mit(_data->_fieldTotals);
    while(mit.hasNext())
    {
      mit.next();
      _data->_fieldSubTotals[mit.key()] = value(mit.key()).toDouble();
    }
  }
}

int XSqlQuery::findFirst(int pField, int pTarget)
{
  if (first())
  {
    do
    {
      if (value(pField).toInt() == pTarget)
        return at();
    }
    while (next());
  }

  return -1;
}

int XSqlQuery::findFirst(const QString &pField, int pTarget)
{
  if (first())
  {
    do
    {
      if (value(pField).toInt() == pTarget)
        return at();
    }
    while (next());
  }

  return -1;
}

int XSqlQuery::findFirst(const QString &pField, const QString &pTarget)
{
  if (first())
  {
    do
    {
      if (value(pField).toString() == pTarget)
        return at();
    }
    while (next());
  }

  return -1;
}

void XSqlQuery::addErrorListener(XSqlQueryErrorListener* listener)
{
  if(!_errorListeners.contains(listener))
    _errorListeners.append(listener);
}

void XSqlQuery::removeErrorListener(XSqlQueryErrorListener* listener)
{
  int i = _errorListeners.indexOf(listener);
  while(-1 != i)
  {
    _errorListeners.removeAt(i);
    i = _errorListeners.indexOf(listener);
  }
}

bool XSqlQuery::emulatePrepare() const
{
  if(_data)
    return _data->_emulatePrepare;
  return false;
}

void XSqlQuery::setEmulatePrepare(bool emulate)
{
  if(_data)
    _data->_emulatePrepare = emulate;
}
