/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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

#include "PasswordWatcher.h"
#include "Plugins/PluginManager.h"

namespace UiUtils {

PasswordWatcher::PasswordWatcher(QObject *parent, Plugins::PluginManager *manager, const QString &accountName, const QString &accountType) :
    QObject(parent), m_manager(manager), m_isStorageEncrypted(false), m_pendingActions(0), m_didReadOk(false), m_didWriteOk(false),
    m_accountName(accountName), m_accountType(accountType)
{
    connect(m_manager, &Plugins::PluginManager::pluginsChanged, this, &PasswordWatcher::backendMaybeChanged);
}

QString PasswordWatcher::progressMessage() const
{
    return m_progressMessage;
}

bool PasswordWatcher::isStorageEncrypted() const
{
    return m_isStorageEncrypted;
}

bool PasswordWatcher::isWaitingForPlugin() const
{
    return m_pendingActions > 0;
}

bool PasswordWatcher::isPluginAvailable() const
{
    return m_manager->password();
}

QString PasswordWatcher::password() const
{
    return m_password;
}

bool PasswordWatcher::didReadOk() const
{
    return m_didReadOk;
}

bool PasswordWatcher::didWriteOk() const
{
    return m_didWriteOk;
}

void PasswordWatcher::reloadPassword()
{
    m_didReadOk = false;
    m_password = QString();
    ++m_pendingActions;
    if (Plugins::PasswordPlugin *plugin = m_manager->password()) {
        if (Plugins::PasswordJob *job = plugin->requestPassword(m_accountName, m_accountType)) {
            m_isStorageEncrypted = plugin->features() & Plugins::PasswordPlugin::FeatureEncryptedStorage;
            connect(job, &Plugins::PasswordJob::passwordAvailable, this, &PasswordWatcher::passwordRetrieved);
            connect(job, &Plugins::PasswordJob::error, this, &PasswordWatcher::passwordReadingFailed);
            job->setAutoDelete(true);
            job->start();
            m_progressMessage = tr("Loading password from plugin %1...").arg(m_manager->passwordPlugin());
            emit stateChanged();
            return;
        } else {
            m_progressMessage = tr("Cannot read password via plugin %1").arg(m_manager->passwordPlugin());
        }
    } else {
        m_progressMessage = tr("Password plugin is not available.");
    }

    // error handling
    // There's actually no pending action now
    --m_pendingActions;
    emit stateChanged();
    emit readingFailed(m_progressMessage);
}

void PasswordWatcher::passwordRetrieved(const QString &password)
{
    m_password = password;
    m_progressMessage.clear();
    --m_pendingActions;
    m_didReadOk = true;
    emit stateChanged();
    emit readingDone();
}

void PasswordWatcher::passwordReadingFailed(const Plugins::PasswordJob::Error errorCode, const QString &errorMessage)
{
    --m_pendingActions;
    m_didReadOk = false;
    switch (errorCode) {
    case Plugins::PasswordJob::NoSuchPassword:
        m_progressMessage.clear();
        break;
    case Plugins::PasswordJob::Stopped:
    case Plugins::PasswordJob::UnknownError:
    {
        QString msg = tr("Password reading failed: %1").arg(errorMessage);
        m_progressMessage = msg;
        emit readingFailed(msg);
        break;
    }
    }
    emit stateChanged();
}

void PasswordWatcher::passwordWritten()
{
    --m_pendingActions;
    m_progressMessage.clear();
    m_didWriteOk = true;
    emit stateChanged();
    emit savingDone();
}

void PasswordWatcher::passwordWritingFailed(const Plugins::PasswordJob::Error errorCode, const QString &errorMessage)
{
    --m_pendingActions;
    m_didWriteOk = false;
    switch (errorCode) {
    case Plugins::PasswordJob::NoSuchPassword:
        Q_ASSERT(false);
        break;
    case Plugins::PasswordJob::Stopped:
    case Plugins::PasswordJob::UnknownError:
    {
        QString msg = tr("Password writing failed: %1").arg(errorMessage);
        m_progressMessage = msg;
        emit savingFailed(msg);
        break;
    }
    }
    emit stateChanged();
}

void PasswordWatcher::setPassword(const QString &password)
{
    m_didWriteOk = false;
    ++m_pendingActions;
    if (Plugins::PasswordPlugin *plugin = m_manager->password()) {
        if (Plugins::PasswordJob *job = plugin->storePassword(m_accountName, m_accountType, password)) {
            connect(job, &Plugins::PasswordJob::passwordStored, this, &PasswordWatcher::passwordWritten);
            connect(job, &Plugins::PasswordJob::error, this, &PasswordWatcher::passwordWritingFailed);
            job->setAutoDelete(true);
            job->start();
            m_progressMessage = tr("Saving password from plugin %1...").arg(m_manager->passwordPlugin());
            emit stateChanged();
            return;
        } else {
            m_progressMessage = tr("Cannot save password via plugin %1").arg(m_manager->passwordPlugin());
        }
    } else {
        m_progressMessage = tr("Password plugin is not available.");
    }

    // error handling
    --m_pendingActions;
    emit stateChanged();
    emit savingFailed(m_progressMessage);
}

}
