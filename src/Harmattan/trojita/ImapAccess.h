/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@gentoo.org>

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

#ifndef IMAPACCESS_H
#define IMAPACCESS_H

#include <QObject>

#include "Imap/Model/MailboxModel.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MsgListModel.h"

class ImapAccess : public QObject
{
    Q_OBJECT
public:
    explicit ImapAccess(QObject *parent = 0);

    Imap::Mailbox::Model *model;
    Imap::Mailbox::AbstractCache *cache;
    Imap::Mailbox::MailboxModel *mailboxModel;
    Imap::Mailbox::MsgListModel *msgListModel;

public slots:
    void alertReceived(const QString &message);
    void connectionError(const QString &message);
    void authenticationRequested(QAuthenticator *auth);
    void authenticationFailed(const QString &message);
    void slotLogged(uint parserId, const Imap::Mailbox::LogMessage &message);
};

#endif // IMAPACCESS_H
