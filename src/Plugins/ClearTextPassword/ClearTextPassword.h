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

#ifndef CLEAR_TEXT_PASSWORD_H
#define CLEAR_TEXT_PASSWORD_H

#include <QString>

#include "Plugins/PasswordPlugin.h"
#include "Plugins/PluginInterface.h"

class QSettings;

using namespace Plugins;

class ClearTextPasswordJob : public PasswordJob
{
    Q_OBJECT

public:
    enum Type {
        Request,
        Store,
        Delete
    };

    ClearTextPasswordJob(const QString &accountId, const QString &accountType, const QString &password, enum Type type, QObject *parent, QSettings *settings);

public slots:
    virtual void doStart();
    virtual void doStop();

private:
    QString m_accountId, m_accountType, m_password;
    enum Type m_type;
    QSettings *m_settings;
};

class ClearTextPassword : public PasswordPlugin
{
    Q_OBJECT

public:
    ClearTextPassword(QObject *parent, QSettings *settings);
    virtual Features features() const;

public slots:
    virtual PasswordJob *requestPassword(const QString &accountId, const QString &accountType);
    virtual PasswordJob *storePassword(const QString &accountId, const QString &accountType, const QString &password);
    virtual PasswordJob *deletePassword(const QString &accountId, const QString &accountType);

private:
    QSettings *m_settings;
};

class trojita_plugin_ClearTextPasswordPlugin : public QObject, public PasswordPluginInterface
{
    Q_OBJECT
    Q_INTERFACES(Plugins::PasswordPluginInterface)
    Q_PLUGIN_METADATA(IID "net.flaska.trojita.plugins.password.cleartext")

public:
    virtual QString name() const override;
    virtual QString description() const override;
    virtual PasswordPlugin *create(QObject *parent, QSettings *settings) override;
private:
};

#endif //CLEAR_TEXT_PASSWORD_H

// vim: set et ts=4 sts=4 sw=4
