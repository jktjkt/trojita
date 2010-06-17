/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef IMAP_MODELUPDATERS_H
#define IMAP_MODELUPDATERS_H

#include <QObject>

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

class Model;
class TreeItem;
class TreeItemMailbox;

/** @short Helper class for delayed updates of the list of child mailboxes */
class _MailboxListUpdater: public QObject {
    Q_OBJECT
public:
    _MailboxListUpdater( Model* model, TreeItemMailbox* mailbox, const QList<TreeItem*>& children );
public slots:
    void perform();
private:
    Model* _model;
    TreeItemMailbox* _mailbox;
    QList<TreeItem*> _children;
};

/** @short Helper class for delayed updates of the number of messages in a mailbox */
class _NumberOfMessagesUpdater: public QObject {
    Q_OBJECT
public:
    _NumberOfMessagesUpdater( Model* model, TreeItemMailbox* mailbox );
public slots:
    void perform();
private:
    Model* _model;
    TreeItemMailbox* _mailbox;
};

}

}

#endif /* IMAP_MODELUPDATERS_H */
