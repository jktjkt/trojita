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

#ifndef PASSWORD_INTERFACE
#define PASSWORD_INTERFACE

#include <QObject>
#include <QString>

#include "PluginJob.h"

namespace Plugins
{

class PLUGINS_EXPORT PasswordJob : public PluginJob
{
    Q_OBJECT

public:
    /** @short Error emitted by error signal */
    enum Error {
        UnknownError, /**< Unknown error */
        Stopped, /**< Emitted when job stopped */
        NoSuchPassword /**< Emitted when password is not available */
    };

signals:
    /** @short Emitted when password is available */
    void passwordAvailable(const QString &password);

    /** @short Emitted when job finish storing password */
    void passwordStored();

    /** @short Emitted when job finish deleting password */
    void passwordDeleted();

    /** @short Emitted when job finish unsuccessful with error */
    void error(Plugins::PasswordJob::Error error);

protected:
    PasswordJob(QObject *parent);
};

class PLUGINS_EXPORT PasswordPlugin : public QObject
{
    Q_OBJECT

public:
    /** @short Features returned by method features */
    enum Feature {
        FeatureEncryptedStorage = 1 << 0 /**< Plugin using encrypted storage */
    };

    Q_DECLARE_FLAGS(Features, Feature);

    /** @short Return implementation features */
    virtual Features features() const = 0;

public slots:
    /** @short Request password associated with accountId and accountType and return PasswordJob */
    virtual PasswordJob *requestPassword(const QString &accountId, const QString &accountType) = 0;

    /** @short Save password for accountId and accountType and return PasswordJob */
    virtual PasswordJob *storePassword(const QString &accountId, const QString &accountType, const QString &password) = 0;

    /** @short Delete password for accountId and accountType and return PasswordJob */
    virtual PasswordJob *deletePassword(const QString &accountId, const QString &accountType) = 0;

protected:
    PasswordPlugin(QObject *parent);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(PasswordPlugin::Features)

}

#endif //PASSWORD_INTERFACE

// vim: set et ts=4 sts=4 sw=4
