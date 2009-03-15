/* Copyright (C) 2007 - 2008 Jan Kundrát <jkt@gentoo.org>

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

#ifndef IMAP_MESSAGEXMODEL_H
#define IMAP_MESSAGEMODEL_H

#include <QAbstractItemModel>
#include "Model.h"

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

/** @short A model implementing view of the whole IMAP server */
class MessageModel: public QAbstractItemModel {
    Q_OBJECT

public:
    MessageModel( QObject* parent, Model* model );

private:
    MessageModel& operator=( const Model& ); // don't implement
    MessageModel( const Model& ); // don't implement
};

}

}

#endif /* IMAP_MAILBOXMODEL_H */
