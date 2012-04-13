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

#include "ImapAccess.h"
#include <QAuthenticator>
#include "Imap/Model/MemoryCache.h"

ImapAccess::ImapAccess(QObject *parent) :
    QObject(parent), model(0), cache(0), mailboxModel(0), msgListModel(0)
{
    Imap::Mailbox::SocketFactoryPtr factory;
    Imap::Mailbox::TaskFactoryPtr taskFactory(new Imap::Mailbox::TaskFactory());
    // FIXME: respect the settings about where to connect
    factory.reset(new Imap::Mailbox::TlsAbleSocketFactory(QLatin1String("192.168.2.14"), 143));

    // FIXME: respect the settings about the cache
    cache = new Imap::Mailbox::MemoryCache(this, QString());

    model = new Imap::Mailbox::Model(this, cache, factory, taskFactory, false);
    model->setProperty("trojita-imap-enable-id", true);
    connect(model, SIGNAL(alertReceived(QString)), this, SLOT(alertReceived(QString)));
    connect(model, SIGNAL(connectionError(QString)), this, SLOT(connectionError(QString)));
    connect(model, SIGNAL(logged(uint,Imap::Mailbox::LogMessage)), this, SLOT(slotLogged(uint,Imap::Mailbox::LogMessage)));

    mailboxModel = new Imap::Mailbox::MailboxModel(this, model);
    msgListModel = new Imap::Mailbox::MsgListModel(this, model);
}

void ImapAccess::alertReceived(const QString &message)
{
    qDebug() << "alertReceived" << message;
}

void ImapAccess::connectionError(const QString &message)
{
    qDebug() << "connectionError" << message;
}

void ImapAccess::slotLogged(uint parserId, const Imap::Mailbox::LogMessage &message)
{
    if (message.kind != Imap::Mailbox::LOG_IO_READ) {
        qDebug() << "LOG" << parserId << message.timestamp << message.kind << message.source << message.message;
    }
}
