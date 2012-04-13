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
    QObject(parent), m_imapModel(0), cache(0), m_mailboxModel(0), m_msgListModel(0), m_port(0)
{
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

QString ImapAccess::server() const
{
    return m_server;
}

void ImapAccess::setServer(const QString &server)
{
    m_server = server;
}

QString ImapAccess::username() const
{
    return m_username;
}

void ImapAccess::setUsername(const QString &username)
{
    m_username = username;
}

QString ImapAccess::password() const
{
    return m_password;
}

void ImapAccess::setPassword(const QString &password)
{
    m_password = password;
}

int ImapAccess::port() const
{
    return m_port;
}

void ImapAccess::setPort(const int port)
{
    m_port = port;
}

QString ImapAccess::sslMode() const
{
    return m_sslMode;
}

void ImapAccess::setSslMode(const QString &sslMode)
{
    m_sslMode = sslMode;
    Q_ASSERT(!m_imapModel);

    Imap::Mailbox::SocketFactoryPtr factory;
    Imap::Mailbox::TaskFactoryPtr taskFactory(new Imap::Mailbox::TaskFactory());

    if (m_sslMode == QLatin1String("SSL")) {
        factory.reset(new Imap::Mailbox::SslSocketFactory(server(), port()));
    } else if (m_sslMode == QLatin1String("StartTLS")) {
        factory.reset(new Imap::Mailbox::TlsAbleSocketFactory(server(), port()));
        factory->setStartTlsRequired(true);
    } else {
        factory.reset(new Imap::Mailbox::TlsAbleSocketFactory(server(), port()));
    }

    // FIXME: respect the settings about the cache
    cache = new Imap::Mailbox::MemoryCache(this, QString());

    m_imapModel = new Imap::Mailbox::Model(this, cache, factory, taskFactory, false);
    m_imapModel->setProperty("trojita-imap-enable-id", true);
    connect(m_imapModel, SIGNAL(alertReceived(QString)), this, SLOT(alertReceived(QString)));
    connect(m_imapModel, SIGNAL(connectionError(QString)), this, SLOT(connectionError(QString)));
    connect(m_imapModel, SIGNAL(logged(uint,Imap::Mailbox::LogMessage)), this, SLOT(slotLogged(uint,Imap::Mailbox::LogMessage)));

    m_imapModel->setImapUser(username());
    m_imapModel->setImapPassword(password());

    m_mailboxModel = new Imap::Mailbox::MailboxModel(this, m_imapModel);
    m_msgListModel = new Imap::Mailbox::MsgListModel(this, m_imapModel);
}

QObject *ImapAccess::imapModel() const
{
    return m_imapModel;
}

QObject *ImapAccess::mailboxModel() const
{
    return m_mailboxModel;
}

QObject *ImapAccess::msgListModel() const
{
    return m_msgListModel;
}
