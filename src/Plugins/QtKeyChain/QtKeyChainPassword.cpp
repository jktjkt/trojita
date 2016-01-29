/* Copyright (C) 2013 Pali Rohár <pali.rohar@gmail.com>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QString>
#include <qt5keychain/keychain.h>
#include "QtKeyChainPassword.h"

namespace Plugins {

QtKeyChainPasswordJob::QtKeyChainPasswordJob(const QString &accountId, const QString &accountType, const QString &password,
    enum Type type, QObject *parent) :
    PasswordJob(parent), m_accountId(accountId), m_accountType(accountType), m_password(password), m_type(type), m_job(0)
{
}

void QtKeyChainPasswordJob::doStart()
{
    switch (m_type) {
    case Request:
        m_job = new QKeychain::ReadPasswordJob(QStringLiteral("Trojita"), this);
        static_cast<QKeychain::ReadPasswordJob *>(m_job)->setKey(m_accountId + QLatin1Char('-') + m_accountType);
        break;
    case Store:
        m_job = new QKeychain::WritePasswordJob(QStringLiteral("Trojita"), this);
        static_cast<QKeychain::WritePasswordJob *>(m_job)->setKey(m_accountId + QLatin1Char('-') + m_accountType);
        static_cast<QKeychain::WritePasswordJob *>(m_job)->setTextData(m_password);
        break;
    case Delete:
        m_job = new QKeychain::DeletePasswordJob(QStringLiteral("Trojita"), this);
        static_cast<QKeychain::DeletePasswordJob *>(m_job)->setKey(m_accountId + QLatin1Char('-') + m_accountType);
        break;
    }
    m_job->setAutoDelete(true);
    connect(m_job, &QKeychain::Job::finished, this, &QtKeyChainPasswordJob::result);
    m_job->start();
}

void QtKeyChainPasswordJob::result()
{
    switch (m_job->error()) {
    case QKeychain::NoError:
        break;
    case QKeychain::EntryNotFound:
        emit error(PasswordJob::NoSuchPassword, QString());
        return;
    default:
        emit error(PasswordJob::UnknownError, m_job->errorString());
        return;
    }

    switch (m_type) {
    case Request:
        m_password = qobject_cast<QKeychain::ReadPasswordJob *>(m_job)->textData();
        emit passwordAvailable(m_password);
        return;
    case Store:
        emit passwordStored();
        return;
    case Delete:
        emit passwordDeleted();
        return;
    }
}

void QtKeyChainPasswordJob::doStop()
{
    if (m_job) {
        disconnect(m_job, nullptr, this, nullptr);
        m_job->deleteLater();
        m_job = 0;
    }
    emit error(PasswordJob::Stopped, QString());
}

QtKeyChainPassword::QtKeyChainPassword(QObject *parent) : PasswordPlugin(parent)
{
}

PasswordPlugin::Features QtKeyChainPassword::features() const
{
    return PasswordPlugin::FeatureEncryptedStorage;
}

PasswordJob *QtKeyChainPassword::requestPassword(const QString &accountId, const QString &accountType)
{
    return new QtKeyChainPasswordJob(accountId, accountType, QString(), QtKeyChainPasswordJob::Request, this);
}

PasswordJob *QtKeyChainPassword::storePassword(const QString &accountId, const QString &accountType, const QString &password)
{
    return new QtKeyChainPasswordJob(accountId, accountType, password, QtKeyChainPasswordJob::Store, this);
}

PasswordJob *QtKeyChainPassword::deletePassword(const QString &accountId, const QString &accountType)
{
    return new QtKeyChainPasswordJob(accountId, accountType, QString(), QtKeyChainPasswordJob::Delete, this);
}

}

QString trojita_plugin_QtKeyChainPasswordPlugin::name() const
{
    return QStringLiteral("qtkeychainpassword");
}

QString trojita_plugin_QtKeyChainPasswordPlugin::description() const
{
    return tr("Secure storage via QtKeychain");
}

Plugins::PasswordPlugin *trojita_plugin_QtKeyChainPasswordPlugin::create(QObject *parent, QSettings *)
{
    return new Plugins::QtKeyChainPassword(parent);
}

// vim: set et ts=4 sts=4 sw=4
