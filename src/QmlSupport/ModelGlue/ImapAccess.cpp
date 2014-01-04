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
#include "Common/Paths.h"
#include "Common/SettingsNames.h"
#include "Imap/Model/Utils.h"
#include "Imap/Model/SQLCache.h"
#include "Imap/Network/MsgPartNetAccessManager.h"
#include "Streams/SocketFactory.h"

ImapAccess::ImapAccess(QObject *parent) :
    QObject(parent), m_imapModel(0), cache(0), m_mailboxModel(0), m_mailboxSubtreeModel(0), m_msgListModel(0),
    m_visibleTasksModel(0), m_oneMessageModel(0), m_netWatcher(0), m_msgQNAM(0), m_port(0)
{
    QSettings s;
    Imap::migrateSettings(&s);
    m_server = s.value(Common::SettingsNames::imapHostKey).toString();
    m_username = s.value(Common::SettingsNames::imapUserKey).toString();
    if (s.value(Common::SettingsNames::imapMethodKey).toString() == Common::SettingsNames::methodSSL) {
        m_sslMode = QLatin1String("SSL");
    } else if (QSettings().value(Common::SettingsNames::imapStartTlsKey).toBool()) {
        m_sslMode = QLatin1String("StartTLS");
    } else {
        m_sslMode = QLatin1String("TCP");
    }
    m_port = s.value(Common::SettingsNames::imapPortKey, QVariant(0)).toInt();
    if (!m_port) {
        m_port = m_sslMode == QLatin1String("SSL") ? 993 : 143;
    }

    QString cacheDir = Common::writablePath(Common::LOCATION_CACHE) + QLatin1String("defaultAccount/");
    m_cacheError = !QDir().mkpath(cacheDir);
    QFile::Permissions expectedPerms = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
    if (QFileInfo(cacheDir).permissions() != expectedPerms) {
        m_cacheError = !QFile::setPermissions(cacheDir, expectedPerms) || m_cacheError;
    }
    m_cacheFile = cacheDir + QLatin1String("/imap.cache.sqlite");
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
    QSettings().setValue(Common::SettingsNames::imapHostKey, m_server);
    emit serverChanged();
}

QString ImapAccess::username() const
{
    return m_username;
}

void ImapAccess::setUsername(const QString &username)
{
    m_username = username;
    QSettings().setValue(Common::SettingsNames::imapUserKey, m_username);
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
    QSettings().setValue(Common::SettingsNames::imapPortKey, m_port);
}

QString ImapAccess::sslMode() const
{
    return m_sslMode;
}

void ImapAccess::setSslMode(const QString &sslMode)
{
    m_sslMode = sslMode;
    Q_ASSERT(!m_imapModel);

    Imap::Mailbox::SocketFactoryPtr factory;
    Imap::Mailbox::TaskFactoryPtr taskFactory(new Imap::Mailbox::TaskFactory());

    if (m_sslMode == QLatin1String("SSL")) {
        QSettings().setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
        factory.reset(new Streams::SslSocketFactory(server(), port()));
    } else if (m_sslMode == QLatin1String("StartTLS")) {
        QSettings().setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodTCP);
        QSettings().setValue(Common::SettingsNames::imapStartTlsKey, true);
        factory.reset(new Streams::TlsAbleSocketFactory(server(), port()));
        factory->setStartTlsRequired(true);
    } else {
        QSettings().setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodTCP);
        QSettings().setValue(Common::SettingsNames::imapStartTlsKey, false);
        factory.reset(new Streams::TlsAbleSocketFactory(server(), port()));
    }

    cache = new Imap::Mailbox::SQLCache(this);
    bool ok = static_cast<Imap::Mailbox::SQLCache*>(cache)->open(QLatin1String("trojita-imap-cache"), m_cacheFile);

    m_imapModel = new Imap::Mailbox::Model(this, cache, std::move(factory), std::move(taskFactory));
    m_imapModel->setProperty("trojita-imap-enable-id", true);
    connect(m_imapModel, SIGNAL(alertReceived(QString)), this, SLOT(alertReceived(QString)));
    connect(m_imapModel, SIGNAL(connectionError(QString)), this, SLOT(connectionError(QString)));
    //connect(m_imapModel, SIGNAL(logged(uint,Common::LogMessage)), this, SLOT(slotLogged(uint,Common::LogMessage)));
    connect(m_imapModel, SIGNAL(needsSslDecision(QList<QSslCertificate>,QList<QSslError>)),
            this, SLOT(slotSslErrors(QList<QSslCertificate>,QList<QSslError>)));

    if (!ok || m_cacheError) {
        // Yes, this is a hack -- but it's better than adding yet another channel for error reporting, IMNSHO.
        QMetaObject::invokeMethod(m_imapModel, "connectionError", Qt::QueuedConnection,
                                  Q_ARG(QString, tr("Cache initialization failed")));
        QMetaObject::invokeMethod(m_imapModel, "setNetworkOffline", Qt::QueuedConnection);
    } else {
        m_netWatcher = new Imap::Mailbox::NetworkWatcher(this, m_imapModel);
        QMetaObject::invokeMethod(m_netWatcher, "setNetworkOnline", Qt::QueuedConnection);
    }

    m_imapModel->setImapUser(username());
    if (!m_password.isNull()) {
        // Really; the idea is to wait before it has been set for the first time
        m_imapModel->setImapPassword(password());
    }

    m_mailboxModel = new Imap::Mailbox::MailboxModel(this, m_imapModel);
    m_mailboxSubtreeModel = new Imap::Mailbox::SubtreeModelOfMailboxModel(this);
    m_mailboxSubtreeModel->setSourceModel(m_mailboxModel);
    m_mailboxSubtreeModel->setOriginalRoot();
    m_msgListModel = new Imap::Mailbox::MsgListModel(this, m_imapModel);
    m_visibleTasksModel = new Imap::Mailbox::VisibleTasksModel(this, m_imapModel->taskModel());
    m_oneMessageModel = new Imap::Mailbox::OneMessageModel(m_imapModel);
    m_msgQNAM = new Imap::Network::MsgPartNetAccessManager(this);
    emit modelsChanged();
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
    QSettings s;

    QByteArray lastKnownPubKey = s.value(Common::SettingsNames::imapSslPemPubKey).toByteArray();
    if (!m_sslChain.isEmpty() && !lastKnownPubKey.isEmpty() && lastKnownPubKey == m_sslChain[0].publicKey().toPem()) {
        // This certificate chain contains the same public keys as the last time; we should accept that
        m_imapModel->setSslPolicy(m_sslChain, m_sslErrors, true);
    } else {
        Imap::Mailbox::CertificateUtils::IconType icon;
        Imap::Mailbox::CertificateUtils::formatSslState(
                    m_sslChain, lastKnownPubKey, m_sslErrors, &m_sslInfoTitle, &m_sslInfoMessage, &icon);
        emit checkSslPolicy();
    }
}

void ImapAccess::setSslPolicy(bool accept)
{
    if (accept && !m_sslChain.isEmpty()) {
        QSettings s;
        s.setValue(Common::SettingsNames::imapSslPemPubKey, m_sslChain[0].publicKey().toPem());
    }
    m_imapModel->setSslPolicy(m_sslChain, m_sslErrors, accept);
}

void ImapAccess::forgetSslCertificate()
{
    QSettings().remove(Common::SettingsNames::imapSslPemPubKey);
}

QString ImapAccess::sslInfoTitle() const
{
    return m_sslInfoTitle;
}

QString ImapAccess::sslInfoMessage() const
{
    return m_sslInfoMessage;
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
    QFile::remove(m_cacheFile);
}
