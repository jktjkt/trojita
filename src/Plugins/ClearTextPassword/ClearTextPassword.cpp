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
#include <QSettings>

#include "ClearTextPassword.h"

struct Settings
{
static QString imapPassKey, smtpPassKey;
};
QString Settings::imapPassKey = QStringLiteral("imap.auth.pass");
QString Settings::smtpPassKey = QStringLiteral("msa.smtp.auth.pass");

ClearTextPasswordJob::ClearTextPasswordJob(const QString &accountId, const QString &accountType, const QString &password,
    enum Type type, QObject *parent, QSettings *settings) :
    PasswordJob(parent), m_accountId(accountId), m_accountType(accountType), m_password(password), m_type(type), m_settings(settings)
{
}

void ClearTextPasswordJob::doStart()
{
    QVariant password;
    switch (m_type) {
    case Request:
        if (m_accountType == QLatin1String("imap")) {
            password = m_settings->value(Settings::imapPassKey);
        } else if (m_accountType == QLatin1String("smtp")) {
            password = m_settings->value(Settings::smtpPassKey);
        } else {
            emit error(PasswordJob::UnknownError, tr("This plugin only supports storing of IMAP and SMTP passwords"));
            break;
        }
        if (password.type() != QVariant::String || password.toString().isEmpty()) {
            emit error(PasswordJob::NoSuchPassword, QString());
        } else {
            emit passwordAvailable(password.toString());
        }
        break;

    case Store:
        if (m_accountType == QLatin1String("imap")) {
            m_settings->setValue(Settings::imapPassKey, m_password);
        } else if (m_accountType == QLatin1String("smtp")) {
            m_settings->setValue(Settings::smtpPassKey, m_password);
        } else {
            emit error(PasswordJob::UnknownError, tr("This plugin only supports storing of IMAP and SMTP passwords"));
            break;
        }
        emit passwordStored();
        break;

    case Delete:
        if (m_accountType == QLatin1String("imap")) {
            m_settings->remove(Settings::imapPassKey);
        } else if (m_accountType == QLatin1String("smtp")) {
            m_settings->remove(Settings::smtpPassKey);
        } else {
            emit error(PasswordJob::UnknownError, tr("This plugin only supports storing of IMAP and SMTP passwords"));
            break;
        }
        emit passwordDeleted();
        break;
    }
}

void ClearTextPasswordJob::doStop()
{
    emit error(PasswordJob::Stopped, QString());
}

ClearTextPassword::ClearTextPassword(QObject *parent, QSettings *settings) : PasswordPlugin(parent), m_settings(settings)
{
}

PasswordPlugin::Features ClearTextPassword::features() const
{
    return 0;
}

PasswordJob *ClearTextPassword::requestPassword(const QString &accountId, const QString &accountType)
{
    return new ClearTextPasswordJob(accountId, accountType, QString(), ClearTextPasswordJob::Request, this, m_settings);
}

PasswordJob *ClearTextPassword::storePassword(const QString &accountId, const QString &accountType, const QString &password)
{
    return new ClearTextPasswordJob(accountId, accountType, password, ClearTextPasswordJob::Store, this, m_settings);
}

PasswordJob *ClearTextPassword::deletePassword(const QString &accountId, const QString &accountType)
{
    return new ClearTextPasswordJob(accountId, accountType, QString(), ClearTextPasswordJob::Delete, this, m_settings);
}

QString trojita_plugin_ClearTextPasswordPlugin::name() const
{
    return QStringLiteral("cleartextpassword");
}

QString trojita_plugin_ClearTextPasswordPlugin::description() const
{
    return trUtf8("Trojitá's settings");
}

Plugins::PasswordPlugin *trojita_plugin_ClearTextPasswordPlugin::create(QObject *parent, QSettings *settings)
{
    Q_ASSERT(settings);
    return new ClearTextPassword(parent, settings);
}

// vim: set et ts=4 sts=4 sw=4
