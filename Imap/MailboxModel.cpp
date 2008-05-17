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

#include "Imap/MailboxModel.h"
#include <QDebug>

namespace Imap {
namespace Mailbox {

MailboxModel::MailboxModel( QObject* parent, CachePtr cache,
        ParserPtr parser, const QString& mailbox, const bool readWrite,
        const ThreadSorting sorting ):
    QAbstractItemModel( parent ), _cache(cache), _parser(parser), 
    _mailbox(mailbox), _threadSorting(sorting), _readWrite(readWrite)
{
    // FIXME :)
    // setup up parser, connect to correct mailbox,...
    // start listening for events, react to them, make signals,... (another thread?)
}

MailboxModel::ThreadSorting MailboxModel::threadSorting()
{
    return _threadSorting;
}

void MailboxModel::setThreadSorting( const ThreadSorting value )
{
    _threadSorting = value;
}


}
}
