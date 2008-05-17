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

#ifndef IMAP_MAILBOXMODEL_H
#define IMAP_MAILBOXMODEL_H

#include <tr1/memory>
#include <QAbstractItemModel>
#include "Imap/Cache.h"
#include "Imap/ParserPool.h"

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

/** @short A model component for the model-view architecture */
class MailboxModel: public QAbstractItemModel {
    Q_OBJECT
    Q_PROPERTY( ThreadSorting threadSorting READ threadSorting WRITE setThreadSorting )
    Q_ENUMS( ThreadSorting )

public:
    /** @short Define how threads should be sorted */
    enum ThreadSorting { ThreadNone, ThreadSubject, ThreadReferences };

    MailboxModel( QObject* parent, CachePtr cache, ParserPtr parser,
            const QString& mailbox, const bool readWrite = true,
            const ThreadSorting sorting = ThreadNone );
    ThreadSorting threadSorting();
    void setThreadSorting( const ThreadSorting value );

private:
    CachePtr _cache;
    ParserPtr _parser;
    QString _mailbox;
    ThreadSorting _threadSorting;
    bool _readWrite;
};

}

}

#endif /* IMAP_MAILBOXMODEL_H */
