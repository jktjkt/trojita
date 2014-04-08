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
static QSettings settings;
static QString imapPassKey, smtpPassKey;
};
QSettings Settings::settings(QLatin1String("flaska.net"), QLatin1String("trojita"));
QString Settings::imapPassKey = QLatin1String("imap.auth.pass");
QString Settings::smtpPassKey = QLatin1String("msa.smtp.auth.pass");

ClearTextPasswordJob::ClearTextPasswordJob(const QString &accountId, const QString &accountType, const QString &password,
    enum Type type, QObject *parent) :
    PasswordJob(parent), m_accountId(accountId), m_accountType(accountType), m_password(password), m_type(type)
{
}

void ClearTextPasswordJob::doStart()
{
    QVariant password;
    switch (m_type) {
    case Request:
        if (m_accountType == QLatin1String("imap")) {
            password = Settings::settings.value(Settings::imapPassKey);
        } else if (m_accountType == QLatin1String("smtp")) {
            password = Settings::settings.value(Settings::smtpPassKey);
        } else {
            emit error(PasswordJob::UnknownError);
            break;
        }
        if (password.type() != QVariant::String) {
            emit error(PasswordJob::NoSuchPassword);
        } else {
            emit passwordAvailable(password.toString());
        }
        break;

    case Store:
        if (m_accountType == QLatin1String("imap")) {
            Settings::settings.setValue(Settings::imapPassKey, m_password);
        } else if (m_accountType == QLatin1String("smtp")) {
            Settings::settings.setValue(Settings::smtpPassKey, m_password);
        } else {
            emit error(PasswordJob::UnknownError);
            break;
        }
        emit passwordStored();
        break;

    case Delete:
        if (m_accountType == QLatin1String("imap")) {
            Settings::settings.remove(Settings::imapPassKey);
        } else if (m_accountType == QLatin1String("smtp")) {
            Settings::settings.remove(Settings::smtpPassKey);
        } else {
            emit error(PasswordJob::UnknownError);
            break;
        }
        emit passwordDeleted();
        break;
    }
    finished();
}

void ClearTextPasswordJob::doStop()
{
    emit error(PasswordJob::Stopped);
    finished();
}

ClearTextPassword::ClearTextPassword(QObject *parent) : PasswordPlugin(parent)
{
}

PasswordPlugin::Features ClearTextPassword::features() const
{
    return 0;
}

PasswordJob *ClearTextPassword::requestPassword(const QString &accountId, const QString &accountType)
{
    return new ClearTextPasswordJob(accountId, accountType, QString(), ClearTextPasswordJob::Request, this);
}

PasswordJob *ClearTextPassword::storePassword(const QString &accountId, const QString &accountType, const QString &password)
{
    return new ClearTextPasswordJob(accountId, accountType, password, ClearTextPasswordJob::Store, this);
}

PasswordJob *ClearTextPassword::deletePassword(const QString &accountId, const QString &accountType)
{
    return new ClearTextPasswordJob(accountId, accountType, QString(), ClearTextPasswordJob::Delete, this);
}

QString trojita_plugin_ClearTextPasswordPlugin::name() const
{
    return QLatin1String("cleartextpassword");
}

QString trojita_plugin_ClearTextPasswordPlugin::description() const
{
    return trUtf8("Trojitá config file");
}

QObject *trojita_plugin_ClearTextPasswordPlugin::create(QObject *parent)
{
    return new ClearTextPassword(parent);
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Q_EXPORT_PLUGIN2(trojita_plugin_ClearTextPasswordPlugin, trojita_plugin_ClearTextPasswordPlugin)
#endif

// vim: set et ts=4 sts=4 sw=4
