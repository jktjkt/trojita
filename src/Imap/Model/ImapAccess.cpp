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

#include "ImapAccess.h"
#include <QDir>
#include <QFileInfo>
#include <QSslKey>
#include <QSettings>
#include "Common/MetaTypes.h"
#include "Common/Paths.h"
#include "Common/PortNumbers.h"
#include "Common/SettingsNames.h"
#include "Imap/Model/CombinedCache.h"
#include "Imap/Model/DummyNetworkWatcher.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "Imap/Model/Utils.h"
#include "Imap/Network/MsgPartNetAccessManager.h"
#include "Streams/SocketFactory.h"

namespace Imap {

ImapAccess::ImapAccess(QObject *parent, QSettings *settings, const QString &accountName) :
    QObject(parent), m_settings(settings), m_imapModel(0), m_mailboxModel(0), m_mailboxSubtreeModel(0), m_msgListModel(0),
    m_visibleTasksModel(0), m_oneMessageModel(0), m_netWatcher(0), m_msgQNAM(0), m_port(0),
    m_connectionMethod(Common::ConnectionMethod::Invalid),
    m_sslInfoIcon(Imap::Mailbox::CertificateUtils::NoIcon),
    m_accountName(accountName)
{
    Imap::migrateSettings(m_settings);
    m_server = m_settings->value(Common::SettingsNames::imapHostKey).toString();
    m_username = m_settings->value(Common::SettingsNames::imapUserKey).toString();
    if (m_settings->value(Common::SettingsNames::imapMethodKey).toString() == Common::SettingsNames::methodSSL) {
        m_connectionMethod = Common::ConnectionMethod::NetDedicatedTls;
    } else if (m_settings->value(Common::SettingsNames::imapMethodKey).toString() == Common::SettingsNames::methodTCP) {
        m_connectionMethod = m_settings->value(Common::SettingsNames::imapStartTlsKey).toBool() ?
                    Common::ConnectionMethod::NetStartTls : Common::ConnectionMethod::NetCleartext;
    } else if (m_settings->value(Common::SettingsNames::imapMethodKey).toString() == Common::SettingsNames::methodProcess) {
        m_connectionMethod = Common::ConnectionMethod::Process;
    }
    m_port = m_settings->value(Common::SettingsNames::imapPortKey, QVariant(0)).toInt();
    if (!m_port) {
        switch (m_connectionMethod) {
        case Common::ConnectionMethod::NetCleartext:
        case Common::ConnectionMethod::NetStartTls:
            m_port = Common::PORT_IMAP;
            break;
        case Common::ConnectionMethod::NetDedicatedTls:
            m_port = Common::PORT_IMAPS;
            break;
        case Common::ConnectionMethod::Process:
        case Common::ConnectionMethod::Invalid:
            // do nothing
            break;
        }
    }

    m_cacheDir = Common::writablePath(Common::LOCATION_CACHE) + m_accountName + QLatin1Char('/');;
}

void ImapAccess::alertReceived(const QString &message)
{
    qDebug() << "alertReceived" << message;
}

void ImapAccess::connectionError(const QString &message)
{
    qDebug() << "connectionError" << message;
}

void ImapAccess::slotLogged(uint parserId, const Common::LogMessage &message)
{
    if (message.kind != Common::LOG_IO_READ) {
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
    m_settings->setValue(Common::SettingsNames::imapHostKey, m_server);
    emit serverChanged();
}

QString ImapAccess::username() const
{
    return m_username;
}

void ImapAccess::setUsername(const QString &username)
{
    m_username = username;
    m_settings->setValue(Common::SettingsNames::imapUserKey, m_username);
    emit usernameChanged();;
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
    m_settings->setValue(Common::SettingsNames::imapPortKey, m_port);
    emit portChanged();
}

QString ImapAccess::sslMode() const
{
    switch (m_connectionMethod) {
    case Common::ConnectionMethod::NetCleartext:
        return QLatin1String("No");
    case Common::ConnectionMethod::NetStartTls:
        return QLatin1String("StartTLS");
    case Common::ConnectionMethod::NetDedicatedTls:
        return QLatin1String("SSL");
    case Common::ConnectionMethod::Invalid:
    case Common::ConnectionMethod::Process:
        return QString();
    }

    Q_ASSERT(false);
    return QString();
}

void ImapAccess::setSslMode(const QString &sslMode)
{
    if (sslMode == QLatin1String("No")) {
        setConnectionMethod(Common::ConnectionMethod::NetCleartext);
    } else if (sslMode == QLatin1String("SSL")) {
        setConnectionMethod(Common::ConnectionMethod::NetDedicatedTls);
    } else if (sslMode == QLatin1String("StartTLS")) {
        setConnectionMethod(Common::ConnectionMethod::NetStartTls);
    } else {
        Q_ASSERT(false);
    }
}

Common::ConnectionMethod ImapAccess::connectionMethod() const
{
    return m_connectionMethod;
}

void ImapAccess::setConnectionMethod(const Common::ConnectionMethod mode)
{
    m_connectionMethod = mode;
    switch (m_connectionMethod) {
    case Common::ConnectionMethod::Invalid:
        break;
    case Common::ConnectionMethod::NetCleartext:
    case Common::ConnectionMethod::NetStartTls:
        m_settings->setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodTCP);
        m_settings->setValue(Common::SettingsNames::imapStartTlsKey, m_connectionMethod == Common::ConnectionMethod::NetStartTls);
        break;
    case Common::ConnectionMethod::NetDedicatedTls:
        m_settings->setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
        // Trying to communicate the fact that this is going to be an encrypted connection, even though
        // that settings bit is not actually used
        m_settings->setValue(Common::SettingsNames::imapStartTlsKey, true);
        break;
    case Common::ConnectionMethod::Process:
        m_settings->setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodProcess);
        break;
    }
    emit connMethodChanged();
}

void ImapAccess::doConnect()
{
    Q_ASSERT(!m_imapModel);

    Imap::Mailbox::SocketFactoryPtr factory;
    Imap::Mailbox::TaskFactoryPtr taskFactory(new Imap::Mailbox::TaskFactory());

    Streams::ProxySettings proxySettings = m_settings->value(Common::SettingsNames::imapUseSystemProxy, true).toBool() ?
                Streams::ProxySettings::RespectSystemProxy : Streams::ProxySettings::DirectConnect;

    switch (m_connectionMethod) {
    case Common::ConnectionMethod::Invalid:
        factory.reset(new Streams::FakeSocketFactory(Imap::CONN_STATE_LOGOUT));
        break;
    case Common::ConnectionMethod::NetCleartext:
    case Common::ConnectionMethod::NetStartTls:
        factory.reset(new Streams::TlsAbleSocketFactory(server(), port()));
        factory->setStartTlsRequired(m_connectionMethod == Common::ConnectionMethod::NetStartTls);
        factory->setProxySettings(proxySettings, QLatin1String("imap"));
        break;
    case Common::ConnectionMethod::NetDedicatedTls:
        factory.reset(new Streams::SslSocketFactory(server(), port()));
        factory->setProxySettings(proxySettings, QLatin1String("imap"));
        break;
    case Common::ConnectionMethod::Process:
        QStringList args = m_settings->value(Common::SettingsNames::imapProcessKey).toString().split(QLatin1Char(' '));
        if (args.isEmpty()) {
            // it's going to fail anyway
            args << QLatin1String("");
        }
        QString appName = args.takeFirst();
        factory.reset(new Streams::ProcessSocketFactory(appName, args));
        break;
    }

    bool shouldUsePersistentCache =
            m_settings->value(Common::SettingsNames::cacheOfflineKey).toString() != Common::SettingsNames::cacheOfflineNone;

    if (shouldUsePersistentCache && !QDir().mkpath(m_cacheDir)) {
        onCacheError(tr("Failed to create directory %1").arg(m_cacheDir));
        shouldUsePersistentCache = false;
    }

    if (shouldUsePersistentCache) {
        QFile::Permissions expectedPerms = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
        if (QFileInfo(m_cacheDir).permissions() != expectedPerms) {
            if (!QFile::setPermissions(m_cacheDir, expectedPerms)) {
#ifndef Q_OS_WIN32
                onCacheError(tr("Failed to set safe permissions on cache directory %1").arg(m_cacheDir));
                shouldUsePersistentCache = false;
#endif
            }
        }
    }

    Imap::Mailbox::AbstractCache *cache = 0;

    if (!shouldUsePersistentCache) {
        cache = new Imap::Mailbox::MemoryCache(this);
    } else {
        cache = new Imap::Mailbox::CombinedCache(this, QLatin1String("trojita-imap-cache"), m_cacheDir);
        connect(cache, SIGNAL(error(QString)), this, SLOT(onCacheError(QString)));
        if (! static_cast<Imap::Mailbox::CombinedCache *>(cache)->open()) {
            // Error message was already shown by the cacheError() slot
            cache->deleteLater();
            cache = new Imap::Mailbox::MemoryCache(this);
        } else {
            if (m_settings->value(Common::SettingsNames::cacheOfflineKey).toString() == Common::SettingsNames::cacheOfflineAll) {
                cache->setRenewalThreshold(0);
            } else {
                const int defaultCacheLifetime = 30;
                bool ok;
                int num = m_settings->value(Common::SettingsNames::cacheOfflineNumberDaysKey, defaultCacheLifetime).toInt(&ok);
                if (!ok)
                    num = defaultCacheLifetime;
                cache->setRenewalThreshold(num);
            }
        }
    }

    m_imapModel = new Imap::Mailbox::Model(this, cache, std::move(factory), std::move(taskFactory));
    m_imapModel->setObjectName(QString::fromUtf8("imapModel-%1").arg(m_accountName));
    m_imapModel->setCapabilitiesBlacklist(m_settings->value(Common::SettingsNames::imapBlacklistedCapabilities).toStringList());
    m_imapModel->setProperty("trojita-imap-enable-id", m_settings->value(Common::SettingsNames::imapEnableId, true).toBool());
    connect(m_imapModel, SIGNAL(alertReceived(QString)), this, SLOT(alertReceived(QString)));
    connect(m_imapModel, SIGNAL(connectionError(QString)), this, SLOT(connectionError(QString)));
    //connect(m_imapModel, SIGNAL(logged(uint,Common::LogMessage)), this, SLOT(slotLogged(uint,Common::LogMessage)));
    connect(m_imapModel, SIGNAL(needsSslDecision(QList<QSslCertificate>,QList<QSslError>)),
            this, SLOT(slotSslErrors(QList<QSslCertificate>,QList<QSslError>)));

    if (m_settings->value(Common::SettingsNames::imapNeedsNetwork, true).toBool()) {
        m_netWatcher = new Imap::Mailbox::SystemNetworkWatcher(this, m_imapModel);
    } else {
        m_netWatcher = new Imap::Mailbox::DummyNetworkWatcher(this, m_imapModel);
    }
    QMetaObject::invokeMethod(m_netWatcher,
                              m_settings->value(Common::SettingsNames::imapStartOffline).toBool() ?
                                  "setNetworkOffline" : "setNetworkOnline",
                              Qt::QueuedConnection);

    m_imapModel->setImapUser(username());
    if (!m_password.isNull()) {
        // Really; the idea is to wait before it has been set for the first time
        m_imapModel->setImapPassword(password());
    }

    m_mailboxModel = new Imap::Mailbox::MailboxModel(this, m_imapModel);
    m_mailboxModel->setObjectName(QString::fromUtf8("mailboxModel-%1").arg(m_accountName));
    m_mailboxSubtreeModel = new Imap::Mailbox::SubtreeModelOfMailboxModel(this);
    m_mailboxSubtreeModel->setObjectName(QString::fromUtf8("mailboxSubtreeModel-%1").arg(m_accountName));
    m_mailboxSubtreeModel->setSourceModel(m_mailboxModel);
    m_mailboxSubtreeModel->setOriginalRoot();
    m_msgListModel = new Imap::Mailbox::MsgListModel(this, m_imapModel);
    m_msgListModel->setObjectName(QString::fromUtf8("msgListModel-%1").arg(m_accountName));
    m_visibleTasksModel = new Imap::Mailbox::VisibleTasksModel(this, m_imapModel->taskModel());
    m_visibleTasksModel->setObjectName(QString::fromUtf8("visibleTasksModel-%1").arg(m_accountName));
    m_oneMessageModel = new Imap::Mailbox::OneMessageModel(m_imapModel);
    m_oneMessageModel->setObjectName(QString::fromUtf8("oneMessageModel-%1").arg(m_accountName));
    m_msgQNAM = new Imap::Network::MsgPartNetAccessManager(this);
    m_msgQNAM->setObjectName(QString::fromUtf8("m_msgQNAM-%1").arg(m_accountName));
    emit modelsChanged();
}

void ImapAccess::onCacheError(const QString &message)
{
    if (m_imapModel) {
        m_imapModel->setCache(new Imap::Mailbox::MemoryCache(m_imapModel));
    }
    emit cacheError(message);
}

QObject *ImapAccess::imapModel() const
{
    return m_imapModel;
}

QObject *ImapAccess::mailboxModel() const
{
    return m_mailboxSubtreeModel;
}

QObject *ImapAccess::msgListModel() const
{
    return m_msgListModel;
}

QObject *ImapAccess::visibleTasksModel() const
{
    return m_visibleTasksModel;
}

QObject *ImapAccess::oneMessageModel() const
{
    return m_oneMessageModel;
}

QObject *ImapAccess::networkWatcher() const
{
    return m_netWatcher;
}

QNetworkAccessManager *ImapAccess::msgQNAM() const
{
    return m_msgQNAM;
}

void ImapAccess::openMessage(const QString &mailboxName, const uint uid)
{
    QModelIndex msgIndex = m_imapModel->messageIndexByUid(mailboxName, uid);
    m_oneMessageModel->setMessage(msgIndex);
    static_cast<Imap::Network::MsgPartNetAccessManager*>(m_msgQNAM)->setModelMessage(msgIndex);
}

QString ImapAccess::prettySize(const uint bytes) const
{
    return Imap::Mailbox::PrettySize::prettySize(bytes, Imap::Mailbox::PrettySize::WITH_BYTES_SUFFIX);
}

void ImapAccess::slotSslErrors(const QList<QSslCertificate> &sslCertificateChain, const QList<QSslError> &sslErrors)
{
    m_sslChain = sslCertificateChain;
    m_sslErrors = sslErrors;

    QByteArray lastKnownPubKey = m_settings->value(Common::SettingsNames::imapSslPemPubKey).toByteArray();
    if (!m_sslChain.isEmpty() && !lastKnownPubKey.isEmpty() && lastKnownPubKey == m_sslChain[0].publicKey().toPem()) {
        // This certificate chain contains the same public keys as the last time; we should accept that
        m_imapModel->setSslPolicy(m_sslChain, m_sslErrors, true);
    } else {
        Imap::Mailbox::CertificateUtils::formatSslState(
                    m_sslChain, lastKnownPubKey, m_sslErrors, &m_sslInfoTitle, &m_sslInfoMessage, &m_sslInfoIcon);
        emit checkSslPolicy();
    }
}

void ImapAccess::setSslPolicy(bool accept)
{
    if (accept && !m_sslChain.isEmpty()) {
        m_settings->setValue(Common::SettingsNames::imapSslPemPubKey, m_sslChain[0].publicKey().toPem());
    }
    m_imapModel->setSslPolicy(m_sslChain, m_sslErrors, accept);
}

void ImapAccess::forgetSslCertificate()
{
    m_settings->remove(Common::SettingsNames::imapSslPemPubKey);
}

QString ImapAccess::sslInfoTitle() const
{
    return m_sslInfoTitle;
}

QString ImapAccess::sslInfoMessage() const
{
    return m_sslInfoMessage;
}

Imap::Mailbox::CertificateUtils::IconType ImapAccess::sslInfoIcon() const
{
    return m_sslInfoIcon;
}

QString ImapAccess::mailboxListMailboxName() const
{
    return m_mailboxSubtreeModel->rootIndex().data(Imap::Mailbox::RoleMailboxName).toString();
}

QString ImapAccess::mailboxListShortMailboxName() const
{
    return m_mailboxSubtreeModel->rootIndex().data(Imap::Mailbox::RoleShortMailboxName).toString();
}

/** @short Persistently remove the local cache of IMAP data

This method should be called by the UI when the user changes its connection details, i.e. when there's a big chance that we are
connecting to a completely different server since the last time.
*/
void ImapAccess::nukeCache()
{
    QFile::remove(m_cacheDir);
}

}
