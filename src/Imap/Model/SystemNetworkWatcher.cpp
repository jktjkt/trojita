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

#include "SystemNetworkWatcher.h"
#include <QNetworkConfigurationManager>
#include <QNetworkSession>
#include "Model.h"

namespace Imap {
namespace Mailbox {

QString policyToString(const NetworkPolicy policy)
{
    switch (policy) {
    case NETWORK_OFFLINE:
        return QStringLiteral("NETWORK_OFFLINE");
    case NETWORK_EXPENSIVE:
        return QStringLiteral("NETWORK_EXPENSIVE");
    case NETWORK_ONLINE:
        return QStringLiteral("NETWORK_ONLINE");
    }
    Q_ASSERT(false);
    return QString();
}

SystemNetworkWatcher::SystemNetworkWatcher(ImapAccess *parent, Model *model):
    NetworkWatcher(parent, model), m_netConfManager(0), m_session(0)
{
    m_netConfManager = new QNetworkConfigurationManager(this);
    resetSession();
    connect(m_netConfManager, &QNetworkConfigurationManager::onlineStateChanged, this, &SystemNetworkWatcher::onGlobalOnlineStateChanged);
    connect(m_netConfManager, &QNetworkConfigurationManager::configurationChanged, this, &SystemNetworkWatcher::networkConfigurationChanged);
}

void SystemNetworkWatcher::onGlobalOnlineStateChanged(const bool online)
{
    if (online) {
        m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"), QStringLiteral("System is back online"));
        auto currentConfig = sessionsActiveConfiguration();
        if (!currentConfig.isValid() && currentConfig != m_netConfManager->defaultConfiguration()) {
            m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"),
                              QStringLiteral("A new network configuration is available"));
            networkConfigurationChanged(m_netConfManager->defaultConfiguration());
        }
        setDesiredNetworkPolicy(m_desiredPolicy);
    } else {
        m_model->setNetworkPolicy(NETWORK_OFFLINE);
        // The session remains open, so that we indicate our intention to reconnect after the connectivity is restored
        // (or when a configured AP comes back to range, etc).
    }
}

NetworkPolicy SystemNetworkWatcher::effectiveNetworkPolicy() const
{
    return m_model->networkPolicy();
}

void SystemNetworkWatcher::setDesiredNetworkPolicy(const NetworkPolicy policy)
{
    if (policy != m_desiredPolicy) {
        m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"),
                          QStringLiteral("User's preference changed: %1").arg(policyToString(policy)));
        m_desiredPolicy = policy;
    }
    if (m_model->networkPolicy() == NETWORK_OFFLINE && policy != NETWORK_OFFLINE) {
        // We are asked to connect, the model is not connected yet
        if (isOnline()) {
            if (m_netConfManager->allConfigurations().isEmpty()) {
                m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"),
                                  // Yes, this is quite deliberate call to tr(). We absolutely want users to be able to read this
                                  // (but no so much as to bother them with a popup for now, I guess -- or not?)
                                  tr("Qt does not recognize any network session. Please be sure that qtbearer package "
                                     "(or similar) is installed. Assuming that network is actually already available."));
            }

            m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"), QStringLiteral("Network is online -> connecting"));
            reconnectModelNetwork();
        } else {
            // Chances are that our previously valid session is not valid anymore
            resetSession();
            // We aren't online yet, but we will become online at some point. When that happens, reconnectModelNetwork() will
            // be called, so there is nothing to do from this place.
            m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"), QStringLiteral("Opening network session"));
            m_session->open();
        }
    } else if (m_model->networkPolicy() != NETWORK_OFFLINE && policy == NETWORK_OFFLINE) {
        // This is pretty easy -- just disconnect the model
        m_model->setNetworkPolicy(NETWORK_OFFLINE);
        m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"), QStringLiteral("Closing network session"));
        m_session->close();
    } else {
        // No need to connect/disconnect/reconnect, yay!
        m_model->setNetworkPolicy(m_desiredPolicy);
    }
}

void SystemNetworkWatcher::reconnectModelNetwork()
{
    m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"),
                      QStringLiteral("Session is %1 (configuration %2), online state: %3").arg(
                          QLatin1String(m_session->isOpen() ? "open" : "not open"),
                          m_session->configuration().name(),
                          QLatin1String(m_netConfManager->isOnline() ? "online" : "offline")
                          ));
    if (!isOnline()) {
        m_model->setNetworkPolicy(NETWORK_OFFLINE);
        return;
    }

    m_model->setNetworkPolicy(m_desiredPolicy);
}

bool SystemNetworkWatcher::isOnline() const
{
    if (m_netConfManager->allConfigurations().isEmpty()) {
        // This means that Qt has no idea about the network. Let's pretend that we are connected.
        // This happens e.g. when users do not have the qt-bearer package installed.
        return true;
    }

    return m_netConfManager->isOnline() && m_session->isOpen();
}

/** @short Some of the configuration profiles have changed

This can be some completely harmelss change, like user editting an inactive profile of some random WiFi network.
Unfortunately, this is also the only method in which the Qt's NetworkManager plugin informs us about a switch
from eth0 to wlan0.

There's apparently no better signal, see http://lists.qt-project.org/pipermail/interest/2013-December/010374.html

*/
void SystemNetworkWatcher::networkConfigurationChanged(const QNetworkConfiguration &conf)
{
    bool reconnect = false;

    if (conf == sessionsActiveConfiguration() && !conf.state().testFlag(QNetworkConfiguration::Active) &&
            conf != m_netConfManager->defaultConfiguration() && m_netConfManager->defaultConfiguration().isValid()) {
        // Change of the "session's own configuration" which is not a default config of the system (anymore?), and the new default
        // is something valid.
        // I'm seeing (Qt 5.5-git, Linux, NetworkManager,...) quite a few of these as false positives on a random hotel WiFi.
        // Let's prevent a ton of useless reconnects here by only handling this if the system now believes that a default session
        // is something else.
        m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"),
                          QStringLiteral("Change of configuration of the current session (%1); current default session is %2")
                          .arg(conf.name(), m_netConfManager->defaultConfiguration().name()));
        reconnect = true;
    } else if (conf.state().testFlag(QNetworkConfiguration::Active) && conf.type() == QNetworkConfiguration::InternetAccessPoint &&
               conf != sessionsActiveConfiguration() && conf == m_netConfManager->defaultConfiguration()) {
        // We are going to interpret this as a subtle hint for switching to another session

        if (m_session->configuration().type() == QNetworkConfiguration::UserChoice && !sessionsActiveConfiguration().isValid()) {
            // No configuration has been assigned yet, just ignore this event. This happens on Harmattan when we get a change
            // of e.g. an office WiFi connection in reply to us trying to open a session with the system's default configuration.
            m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"),
                              QStringLiteral("No configuration has been assigned yet, let's wait for it"));
            return;
        }

        m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"),
                          m_session->configuration().type() == QNetworkConfiguration::InternetAccessPoint ?
                              QStringLiteral("Change of system's default configuration: %1. Currently using %2.")
                              .arg(conf.name(), m_session->configuration().name())
                            :
                              QStringLiteral("Change of system's default configuration: %1. Currently using %2 (active: %3).")
                              .arg(conf.name(), m_session->configuration().name(), sessionsActiveConfiguration().name()));
        reconnect = true;
    }

    if (reconnect) {
        m_model->setNetworkPolicy(NETWORK_OFFLINE);
        resetSession();
        if (m_session->configuration().isValid()) {
            m_session->open();
        } else {
            m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"),
                              QStringLiteral("Waiting for network to become available..."));
        }
    }
}

/** @short Make sure that we switch over to whatever session which is now the default one */
void SystemNetworkWatcher::resetSession()
{
    delete m_session;
    const auto conf = m_netConfManager->defaultConfiguration();
    m_session = new QNetworkSession(conf, this);

    QString buf;
    QTextStream ss(&buf);
    ss << "Switched to network configuration " << conf.name() << " (" << conf.bearerTypeName() << ", " << conf.identifier() << ")";
    m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"), buf);
    connect(m_session, &QNetworkSession::opened, this, &SystemNetworkWatcher::reconnectModelNetwork);
    connect(m_session, static_cast<void (QNetworkSession::*)(QNetworkSession::SessionError)>(&QNetworkSession::error), this, &SystemNetworkWatcher::networkSessionError);
}

QNetworkConfiguration SystemNetworkWatcher::sessionsActiveConfiguration() const
{
    auto conf = m_session->configuration();
    if (m_session->configuration().type() == QNetworkConfiguration::InternetAccessPoint) {
        return conf;
    } else {
        QString activeConfId = m_session->sessionProperty(QStringLiteral("ActiveConfiguration")).toString();
        // yes, this can return an invalid session
        return m_netConfManager->configurationFromIdentifier(activeConfId);
    }
}

void SystemNetworkWatcher::networkSessionError()
{
    m_model->logTrace(0, Common::LOG_OTHER, QStringLiteral("Network Session"),
                      QStringLiteral("Session error: %1").arg(m_session->errorString()));

    if (!m_reconnectTimer->isActive()) {
        attemptReconnect();
    }
}

}
}
