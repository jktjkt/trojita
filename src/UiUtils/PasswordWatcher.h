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

#ifndef TROJITA_UIUTILS_PASSWORDWATCHER_H
#define TROJITA_UIUTILS_PASSWORDWATCHER_H

#include <QObject>
#include "Plugins/PasswordPlugin.h"

namespace Plugins {
class PluginManager;
}

namespace UiUtils {

/** @short Process password requests from plugins */
class PasswordWatcher: public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isStorageEncrypted READ isStorageEncrypted NOTIFY backendMaybeChanged)
    Q_PROPERTY(bool isWaitingForPlugin READ isWaitingForPlugin NOTIFY stateChanged)
    Q_PROPERTY(bool isPluginAvailable READ isPluginAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString progressMessage READ progressMessage NOTIFY stateChanged)
    Q_PROPERTY(QString password READ password NOTIFY stateChanged)
    Q_PROPERTY(bool didReadOk READ didReadOk NOTIFY stateChanged)
    Q_PROPERTY(bool didWriteOk READ didWriteOk NOTIFY stateChanged)

public:
    PasswordWatcher(QObject *parent, Plugins::PluginManager *manager, const QString &accountName, const QString &accountType);

    bool isStorageEncrypted() const;
    bool isWaitingForPlugin() const;
    bool isPluginAvailable() const;
    bool didReadOk() const;
    bool didWriteOk() const;
    QString progressMessage() const;
    QString password() const;

public slots:
    void reloadPassword();
    void setPassword(const QString &password);

private slots:
    void passwordRetrieved(const QString &password);
    void passwordReadingFailed(const Plugins::PasswordJob::Error errorCode, const QString &errorMessage);
    void passwordWritten();
    void passwordWritingFailed(const Plugins::PasswordJob::Error errorCode, const QString &errorMessage);

signals:
    void backendMaybeChanged();
    void stateChanged();
    void readingFailed(const QString &message);
    void savingFailed(const QString &message);
    void readingDone();
    void savingDone();

private:
    Plugins::PluginManager *m_manager;
    bool m_isStorageEncrypted;
    int m_pendingActions;
    bool m_didReadOk;
    bool m_didWriteOk;


    QString m_accountName;
    QString m_accountType;
    QString m_progressMessage;
    QString m_password;

};

}

#endif // TROJITA_UIUTILS_PASSWORDWATCHER_H
