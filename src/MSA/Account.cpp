/* Copyright (C) 2014 Dan Chapman <dpniel@ubuntu.com>
   Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

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
#include <QDebug>
#include <QObject>
#include <QSettings>
#include "Common/InvokeMethod.h"
#include "Common/PortNumbers.h"
#include "Common/SettingsNames.h"
#include "MSA/Account.h"

namespace MSA {

Account::Account(QObject *parent, QSettings *settings, const QString &accountName) :
    QObject(parent), m_settings(settings), m_accountName(accountName),
    m_port(0), m_authenticateEnabled(false), m_reuseImapAuth(false), m_saveToImap(false), m_useBurl(false)
{
    restoreSettings();
}

QString Account::server() const
{
    return m_server;
}

void Account::setServer(const QString &server)
{
    if (server == m_server) {
        return;
    }
    m_server = server;
    emit serverChanged();
}

int Account::port() const
{
    return m_port;
}

void Account::setPort(const quint16 port)
{
    if (m_port == port) {
        return;
    }
    m_port = port;
    maybeShowPortWarning();
    emit portChanged();

}

QString Account::username() const
{
    return m_username;
}

void Account::setUsername(const QString &username)
{
    if (username == m_username) {
        return;
    }
    m_username = username;
    emit usernameChanged();
}

bool Account::authenticateEnabled() const
{
    return m_authenticateEnabled;
}

void Account::setAuthenticateEnabled(const bool auth)
{
    if (auth == m_authenticateEnabled) {
        return;
    }
    m_authenticateEnabled = auth;
    emit authenticateEnabledChanged();
}

bool Account::reuseImapAuthentication() const
{
    return m_reuseImapAuth;
}

void Account::setReuseImapAuthentication(const bool reuseImapAuth)
{
    if (reuseImapAuth == m_reuseImapAuth) {
        return;
    }
    m_reuseImapAuth = reuseImapAuth;
    emit reuseImapAuthenticationChanged();
}

QString Account::pathToSendmail() const
{
    return m_pathToSendmail;
}

void Account::setPathToSendmail(const QString &sendmail)
{
    if (sendmail == m_pathToSendmail) {
        return;
    }
    m_pathToSendmail = sendmail;
    emit pathToSendmailChanged();
}

bool Account::saveToImap() const
{
    return m_saveToImap;
}

void Account::setSaveToImap(const bool selected)
{
    if (selected == m_saveToImap) {
        return;
    }
    m_saveToImap = selected;
    emit saveToImapChanged();
}

QString Account::sentMailboxName() const
{
    return m_sentMailboxName;
}

void Account::setSentMailboxName(const QString &location)
{
    if (location == m_sentMailboxName) {
        return;
    }
    m_sentMailboxName = location;
    emit sentMailboxNameChanged();
}

bool Account::useBurl() const
{
    return m_useBurl;
}

void Account::setUseBurl(const bool selected)
{
    if (selected == m_useBurl) {
        return;
    }
    m_useBurl = selected;
    emit useBurlChanged();
}

quint16 Account::defaultPort(const Method method)
{
    switch (method) {
    case Method::SMTP:
    case Method::SMTP_STARTTLS:
        return Common::PORT_SMTP_SUBMISSION;
        break;
    case Method::SSMTP:
        return Common::PORT_SMTP_SSL;
        break;
    case Method::SENDMAIL:
    case Method::IMAP_SENDMAIL:
        break;
    }
    Q_ASSERT(false);
    return 0;
}

Account::Method Account::submissionMethod() const
{
    return m_msaSubmissionMethod;
}

void Account::setSubmissionMethod(const Method method)
{
    if (method == m_msaSubmissionMethod) {
        return;
    }
    switch (method) {
    case Method::SMTP:
    case Method::SSMTP:
    case Method::SMTP_STARTTLS:
        m_msaSubmissionMethod = method;
        setPort(defaultPort(m_msaSubmissionMethod));
        break;
    case Method::SENDMAIL:
    case Method::IMAP_SENDMAIL:
        m_msaSubmissionMethod = method;
        break;
    }
    emit submissionMethodChanged();

}

void Account::maybeShowPortWarning()
{
    switch (m_msaSubmissionMethod) {
    case Method::SENDMAIL:
    case Method::IMAP_SENDMAIL:
        // this isn't a direct network connection -> ignore this
        return;
    case Method::SMTP:
    case Method::SMTP_STARTTLS:
    case Method::SSMTP:
        // we're connecting through the network -> let's check the port
        break;
    }
    QString portWarn;
    const int defPort = defaultPort(m_msaSubmissionMethod);
    if (m_port != defPort) {
        portWarn = tr("This port is nonstandard. The default port is %1").arg(defPort);
    }

    emit showPortWarning(portWarn);
}

/** @short Persistently save the SMTP settings

This method should be called by the UI when the user wishes to save current settings
*/
void Account::saveSettings()
{
    switch (m_msaSubmissionMethod) {
    case Method::SMTP:
    case Method::SMTP_STARTTLS:
        m_settings->setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSMTP);
        m_settings->setValue(Common::SettingsNames::smtpStartTlsKey,
                             m_msaSubmissionMethod == Method::SMTP_STARTTLS); // unconditionally
        m_settings->setValue(Common::SettingsNames::smtpAuthKey, m_authenticateEnabled);
        m_settings->setValue(Common::SettingsNames::smtpAuthReuseImapCredsKey, m_reuseImapAuth);
        m_settings->setValue(Common::SettingsNames::smtpUserKey, m_username);
        m_settings->setValue(Common::SettingsNames::smtpHostKey, m_server);
        m_settings->setValue(Common::SettingsNames::smtpPortKey, m_port);
        break;
    case Method::SSMTP:
        m_settings->setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSSMTP);
        m_settings->setValue(Common::SettingsNames::smtpAuthKey, m_authenticateEnabled);
        m_settings->setValue(Common::SettingsNames::smtpAuthReuseImapCredsKey, m_reuseImapAuth);
        m_settings->setValue(Common::SettingsNames::smtpUserKey, m_username);
        m_settings->setValue(Common::SettingsNames::smtpHostKey, m_server);
        m_settings->setValue(Common::SettingsNames::smtpPortKey, m_port);
        break;
    case Method::SENDMAIL:
        m_settings->setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSENDMAIL);
        m_settings->setValue(Common::SettingsNames::sendmailKey, m_pathToSendmail);
        break;
    case Method::IMAP_SENDMAIL:
        m_settings->setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodImapSendmail);
        break;
    }

    m_settings->setValue(Common::SettingsNames::composerSaveToImapKey, m_saveToImap);
    if (m_saveToImap) {
        m_settings->setValue(Common::SettingsNames::composerImapSentKey, m_sentMailboxName);
        m_settings->setValue(Common::SettingsNames::smtpUseBurlKey, m_useBurl);
    } else {
        m_settings->setValue(Common::SettingsNames::smtpUseBurlKey, false);
    }

    emit settingsSaved();
}

/** @short Restore SMTP settings from file

This method restores a users persistently stored settings. This should be called from the UI,
usually when an action to cancel any changes that may have been made.
*/
void Account::restoreSettings()
{
    m_server = m_settings->value(Common::SettingsNames::smtpHostKey).toString();
    m_username = m_settings->value(Common::SettingsNames::smtpUserKey).toString();
    QString connMethod = m_settings->value(Common::SettingsNames::msaMethodKey,
                                           Common::SettingsNames::methodSMTP).toString();
    if (connMethod == Common::SettingsNames::methodSMTP) {
        m_msaSubmissionMethod = m_settings->value(Common::SettingsNames::smtpStartTlsKey).toBool() ?
                            Method::SMTP_STARTTLS : Method::SMTP;
    } else if (connMethod == Common::SettingsNames::methodSSMTP) {
        m_msaSubmissionMethod = Method::SSMTP;
    } else if (connMethod == Common::SettingsNames::methodSSMTP) {
        m_msaSubmissionMethod = Method::SSMTP;
    } else if (connMethod == Common::SettingsNames::methodSENDMAIL) {
        m_msaSubmissionMethod = Method::SENDMAIL;
    } else if (connMethod == Common::SettingsNames::methodImapSendmail) {
        m_msaSubmissionMethod = Method::IMAP_SENDMAIL;
    } else {
        // unknown submission type, lets default to SMTP
        m_msaSubmissionMethod = Method::SMTP;
    }
    m_port = m_settings->value(Common::SettingsNames::smtpPortKey, QVariant(0)).toInt();
    if (!m_port) {
        switch (m_msaSubmissionMethod) {
        case Method::SMTP:
        case Method::SMTP_STARTTLS:
        case Method::SSMTP:
            m_port = defaultPort(m_msaSubmissionMethod);
            break;
        case Method::SENDMAIL:
        case Method::IMAP_SENDMAIL:
            break;
        }
    }
    m_authenticateEnabled = m_settings->value(Common::SettingsNames::smtpAuthKey, false).toBool();
    m_reuseImapAuth = m_settings->value(Common::SettingsNames::smtpAuthReuseImapCredsKey, false).toBool();
    m_pathToSendmail = m_settings->value(Common::SettingsNames::sendmailKey,
                                         Common::SettingsNames::sendmailDefaultCmd).toString();
    m_saveToImap = m_settings->value(Common::SettingsNames::composerSaveToImapKey, true).toBool();
    m_sentMailboxName = m_settings->value(Common::SettingsNames::composerImapSentKey, QLatin1String("Sent")).toString();
    m_useBurl = m_settings->value(Common::SettingsNames::smtpUseBurlKey, false).toBool();

    // Be sure the GUI has time to react to the port warning status
    EMIT_LATER_NOARG(this, maybeShowPortWarning);
}

}
