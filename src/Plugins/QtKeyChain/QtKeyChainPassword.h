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

#ifndef QTKEYCHAINPASSWORD_H
#define QTKEYCHAINPASSWORD_H

#include <QString>

#include "Plugins/PasswordPlugin.h"
#include "Plugins/PluginInterface.h"

namespace QKeychain
{
    class Job;
}

namespace Plugins
{

class QtKeyChainPasswordJob : public PasswordJob
{
    Q_OBJECT

public:
    enum Type {
        Request,
        Store,
        Delete
    };

    QtKeyChainPasswordJob(const QString &accountId, const QString &accountType, const QString &password, enum Type type, QObject *parent);

public slots:
    virtual void doStart();
    virtual void doStop();

private slots:
    void result();

private:
    QString m_accountId, m_accountType, m_password;
    enum Type m_type;
    QKeychain::Job *m_job;
};

class QtKeyChainPassword : public PasswordPlugin
{
    Q_OBJECT

public:
    QtKeyChainPassword(QObject *parent);
    virtual Features features() const;

public slots:
    virtual PasswordJob *requestPassword(const QString &accountId, const QString &accountType);
    virtual PasswordJob *storePassword(const QString &accountId, const QString &accountType, const QString &password);
    virtual PasswordJob *deletePassword(const QString &accountId, const QString &accountType);
};

}

class trojita_plugin_QtKeyChainPasswordPlugin : public QObject, public Plugins::PasswordPluginInterface
{
    Q_OBJECT
    Q_INTERFACES(Plugins::PasswordPluginInterface)
    Q_PLUGIN_METADATA(IID "net.flaska.trojita.plugins.password.qtkeychain")

public:
    virtual QString name() const override;
    virtual QString description() const override;
    virtual Plugins::PasswordPlugin *create(QObject *parent, QSettings *) override;
};

#endif //QTKEYCHAINPASSWORD_H

// vim: set et ts=4 sts=4 sw=4
