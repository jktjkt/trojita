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

#ifndef IMAP_MODEL_H
#define IMAP_MODEL_H

#include <QAbstractItemModel>
#include "Imap/Cache.h"
#include "Imap/Authenticator.h"
#include "Imap/Parser.h"

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

/** @short A model implementing view of the whole IMAP server */
class Model: public QAbstractItemModel {
    Q_OBJECT
    Q_PROPERTY( ThreadAlgorithm threadSorting READ threadSorting WRITE setThreadSorting )

    CachePtr _cache;
    AuthenticatorPtr _authenticator;
    ParserPtr _parser;

public:
    Model( QObject* parent, CachePtr cache, AuthenticatorPtr authenticator,
            ParserPtr parser );

    virtual QModelIndex index(int, int, const QModelIndex&) const;
    virtual QModelIndex parent(const QModelIndex&) const;
    virtual int rowCount(const QModelIndex&) const;
    virtual int columnCount(const QModelIndex&) const;
    virtual QVariant data(const QModelIndex&, int) const;

};


}

}

#endif /* IMAP_MODEL_H */
