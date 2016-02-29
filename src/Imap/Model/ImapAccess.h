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

#ifndef TROJITA_IMAPACCESS_H
#define TROJITA_IMAPACCESS_H

#include <QObject>
#include <QSslError>

#include "Common/ConnectionMethod.h"
#include "Common/Logging.h"
#include "Imap/Model/NetworkPolicy.h"
#include "UiUtils/Formatting.h"

class QModelIndex;
class QNetworkAccessManager;
class QSettings;

namespace Plugins {
class PluginManager;
}

namespace UiUtils {
class PasswordWatcher;
}

namespace Imap {

namespace Mailbox {
class MailboxModel;
class Model;
class MsgListModel;
class NetworkWatcher;
class OneMessageModel;
class SubtreeModelOfMailboxModel;
class ThreadingMsgListModel;
class VisibleTasksModel;
}

class ImapAccess : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isConfigured READ isConfigured NOTIFY modelsChanged)
    Q_PROPERTY(QObject *imapModel READ imapModel NOTIFY modelsChanged)
    Q_PROPERTY(QObject *mailboxModel READ mailboxModel NOTIFY modelsChanged)
    Q_PROPERTY(QObject *msgListModel READ msgListModel NOTIFY modelsChanged)
    Q_PROPERTY(QObject *threadingMsgListModel READ threadingMsgListModel NOTIFY modelsChanged)
    Q_PROPERTY(QObject *visibleTasksModel READ visibleTasksModel NOTIFY modelsChanged)
    Q_PROPERTY(QObject *oneMessageModel READ oneMessageModel NOTIFY modelsChanged)
    Q_PROPERTY(QObject *networkWatcher READ networkWatcher NOTIFY modelsChanged)
    Q_PROPERTY(QObject *msgQNAM READ msgQNAM NOTIFY modelsChanged)
    Q_PROPERTY(UiUtils::PasswordWatcher *passwordWatcher READ passwordWatcher NOTIFY modelsChanged)
    Q_PROPERTY(QString server READ server WRITE setServer NOTIFY serverChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString sslMode READ sslMode WRITE setSslMode NOTIFY connMethodChanged)
    Q_PROPERTY(QString sslInfoTitle READ sslInfoTitle NOTIFY checkSslPolicy)
    Q_PROPERTY(QString sslInfoMessage READ sslInfoMessage NOTIFY checkSslPolicy)
    Q_PROPERTY(int numberRefreshInterval READ numberRefreshInterval WRITE setNumberRefreshInterval NOTIFY numberRefreshIntervalChanged)
    Q_ENUMS(Imap::ImapAccess::ConnectionMethod)

public:
    ImapAccess(QObject *parent, QSettings *settings, Plugins::PluginManager *pluginManager, const QString &accountName);

    bool isConfigured() const;
    QAbstractItemModel *imapModel() const;
    QAbstractItemModel *mailboxModel() const;
    QAbstractItemModel *msgListModel() const;
    QAbstractItemModel *visibleTasksModel() const;
    QObject *oneMessageModel() const;
    QObject *networkWatcher() const;
    QAbstractItemModel *threadingMsgListModel() const;
    QObject *msgQNAM() const;
    UiUtils::PasswordWatcher *passwordWatcher() const;

    QString server() const;
    void setServer(const QString &server);
    int port() const;
    void setPort(const int port);
    QString username() const;
    void setUsername(const QString &username);
    QString password() const;
    void setPassword(const QString &password);
    QString sslMode() const;
    void setSslMode(const QString &sslMode);
    int numberRefreshInterval() const;
    void setNumberRefreshInterval(const int interval);

    QString accountName() const;

    Common::ConnectionMethod connectionMethod() const;
    void setConnectionMethod(const Common::ConnectionMethod mode);

    Imap::Mailbox::NetworkPolicy preferredNetworkPolicy() const;

    Q_INVOKABLE void doConnect();

    QString sslInfoTitle() const;
    QString sslInfoMessage() const;
    UiUtils::Formatting::IconType sslInfoIcon() const;

    Q_INVOKABLE void openMessage(const QString &mailbox, const uint uid);
    Q_INVOKABLE void setSslPolicy(bool accept);
    Q_INVOKABLE void forgetSslCertificate();

    Q_INVOKABLE void nukeCache();

    Q_INVOKABLE QString mailboxListShortMailboxName() const;
    Q_INVOKABLE QString mailboxListMailboxName() const;

    Q_INVOKABLE QModelIndex deproxifiedIndex(const QModelIndex index);
    Q_INVOKABLE void markMessageDeleted(const QModelIndex &message, bool marked);
signals:
    void serverChanged();
    void portChanged();
    void usernameChanged();
    void connMethodChanged();
    void modelsChanged();
    void checkSslPolicy();
    void cacheError(const QString &message);
    void numberRefreshIntervalChanged();

public slots:
    void alertReceived(const QString &message);
    void imapError(const QString &message);
    void networkError(const QString &message);
    void onCacheError(const QString &message);
    void slotLogged(uint parserId, const Common::LogMessage &message);
    void slotSslErrors(const QList<QSslCertificate> &sslCertificateChain, const QList<QSslError> &sslErrors);
    void reloadConfiguration();

private slots:
    void onRequireStartTlsInFuture();
    void desiredNetworkPolicyChanged(const Imap::Mailbox::NetworkPolicy policy);

private:
    QSettings *m_settings;
    Imap::Mailbox::Model *m_imapModel;
    Imap::Mailbox::MailboxModel *m_mailboxModel;
    Imap::Mailbox::SubtreeModelOfMailboxModel *m_mailboxSubtreeModel;
    Imap::Mailbox::MsgListModel *m_msgListModel;
    Imap::Mailbox::ThreadingMsgListModel *m_threadingMsgListModel;
    Imap::Mailbox::VisibleTasksModel *m_visibleTasksModel;
    Imap::Mailbox::OneMessageModel *m_oneMessageModel;
    Imap::Mailbox::NetworkWatcher *m_netWatcher;
    QNetworkAccessManager *m_msgQNAM;
    Plugins::PluginManager *m_pluginManager;
    UiUtils::PasswordWatcher *m_passwordWatcher;

    QString m_server;
    int m_port;
    QString m_username;
    QString m_password;
    Common::ConnectionMethod m_connectionMethod;

    QList<QSslCertificate> m_sslChain;
    QList<QSslError> m_sslErrors;
    QString m_sslInfoTitle;
    QString m_sslInfoMessage;
    UiUtils::Formatting::IconType m_sslInfoIcon;

    QString m_accountName;
    QString m_cacheDir;
};

}

#endif // TROJITA_IMAPACCESS_H
