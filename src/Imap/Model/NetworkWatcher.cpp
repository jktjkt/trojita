/* Copyright (C) 2006 - 2013 Jan Kundrát <jkt@flaska.net>

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

#include "NetworkWatcher.h"
#ifdef TROJITA_HAS_QNETWORKSESSION
#include <QNetworkConfigurationManager>
#include <QNetworkSession>
#endif
#include <QNetworkConfigurationManager>
#include "Model.h"

namespace Imap {
namespace Mailbox {

QString policyToString(const NetworkPolicy policy)
{
    switch (policy) {
    case NETWORK_OFFLINE:
        return QLatin1String("NETWORK_OFFLINE");
    case NETWORK_EXPENSIVE:
        return QLatin1String("NETWORK_EXPENSIVE");
    case NETWORK_ONLINE:
        return QLatin1String("NETWORK_ONLINE");
    }
    Q_ASSERT(false);
    return QString();
}

NetworkWatcher::NetworkWatcher(QObject *parent, Model *model):
    QObject(parent), m_model(model), m_desiredPolicy(NETWORK_OFFLINE), m_netConfManager(0), m_session(0)
{
    Q_ASSERT(m_model);
    connect(model, SIGNAL(networkPolicyChanged()), this, SIGNAL(effectiveNetworkPolicyChanged()));

#ifdef TROJITA_HAS_QNETWORKSESSION
    m_netConfManager = new QNetworkConfigurationManager(this);
    resetSession();
    connect(m_netConfManager, SIGNAL(onlineStateChanged(bool)), this, SLOT(reconnectModelNetwork()));
    connect(m_netConfManager, SIGNAL(configurationChanged(QNetworkConfiguration)),
            this, SLOT(networkConfigurationChanged(QNetworkConfiguration)));
#endif
}

/** @short Set the network access policy to "no access allowed" */
void NetworkWatcher::setNetworkOffline()
{
    setDesiredNetworkPolicy(NETWORK_OFFLINE);
}

/** @short Set the network access policy to "possible, but expensive" */
void NetworkWatcher::setNetworkExpensive()
{
    setDesiredNetworkPolicy(NETWORK_EXPENSIVE);
}

/** @short Set the network access policy to "it's cheap to use it" */
void NetworkWatcher::setNetworkOnline()
{
    setDesiredNetworkPolicy(NETWORK_ONLINE);
}

NetworkPolicy NetworkWatcher::desiredNetworkPolicy() const
{
    return m_desiredPolicy;
}

NetworkPolicy NetworkWatcher::effectiveNetworkPolicy() const
{
    return m_model->networkPolicy();
}

void NetworkWatcher::setDesiredNetworkPolicy(const NetworkPolicy policy)
{
    m_model->logTrace(0, Common::LOG_OTHER, tr("Network Session"),
                      QString::fromUtf8("User's preference changed: %1").arg(policyToString(policy)));
    m_desiredPolicy = policy;
    if (m_model->networkPolicy() == NETWORK_OFFLINE && policy != NETWORK_OFFLINE) {
        // We are asked to connect, the model is not connected yet
        if (isOnline()) {
            m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("Network Session"), QLatin1String("Network is online -> connecting"));
            reconnectModelNetwork();
        } else {
            // We aren't online yet, but we will become online at some point. When that happens, reconnectModelNetwork() will
            // be called, so there is nothing to do from this place.
            m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("Network Session"), QLatin1String("Opening network session"));
#ifdef TROJITA_HAS_QNETWORKSESSION
            m_session->open();
#endif
        }
    } else if (m_model->networkPolicy() != NETWORK_OFFLINE && policy == NETWORK_OFFLINE) {
        // This is pretty easy -- just disconnect the model
        m_model->setNetworkPolicy(NETWORK_OFFLINE);
#ifdef TROJITA_HAS_QNETWORKSESSION
        m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("Network Session"), QLatin1String("Closing network session"));
        m_session->close();
#endif
    } else {
        // No need to connect/disconnect/reconnect, yay!
        m_model->setNetworkPolicy(m_desiredPolicy);
    }
}

void NetworkWatcher::reconnectModelNetwork()
{
#ifdef TROJITA_HAS_QNETWORKSESSION
    m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("Network Session"),
                      QString::fromUtf8("Session is %1 (configuration %2), online state: %3").arg(
                          QLatin1String(m_session->isOpen() ? "open" : "not open"),
                          m_session->configuration().name(),
                          QLatin1String(m_netConfManager->isOnline() ? "online" : "offline")
                          ));
#endif
    if (!isOnline()) {
        m_model->setNetworkPolicy(NETWORK_OFFLINE);
        return;
    }

    m_model->setNetworkPolicy(m_desiredPolicy);
}

bool NetworkWatcher::isOnline() const
{
#ifdef TROJITA_HAS_QNETWORKSESSION
    return m_netConfManager->isOnline() && m_session->isOpen();
#else
    // We actually don't know, so let's pretend that the network is here. The alternative is that we would never connect :).
    return true;
#endif
}

#ifdef TROJITA_HAS_QNETWORKSESSION
/** @short Some of the configuration profiles have changed

This can be some completely harmelss change, like user editting an inactive profile of some random WiFi network.
Unfortunately, this is also the only method in which the Qt's NetworkManager plugin informs us about a switch
from eth0 to wlan0.

There's apparently no better signal, see http://lists.qt-project.org/pipermail/interest/2013-December/010374.html

*/
void NetworkWatcher::networkConfigurationChanged(const QNetworkConfiguration &conf)
{
    bool reconnect = false;

    if (conf == sessionsActiveConfiguration() && !conf.state().testFlag(QNetworkConfiguration::Active)) {
        // Change of the "session's own configuration" -- perhaps it is no longer available
        m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("Network Session"),
                          QString::fromUtf8("Change of configuration of the current session"));
        reconnect = true;
    } else if (conf.state().testFlag(QNetworkConfiguration::Active) && conf.type() == QNetworkConfiguration::InternetAccessPoint &&
               conf != sessionsActiveConfiguration()) {
        // We are going to interpret this as a subtle hint for switching to another session

        if (m_session->configuration().type() == QNetworkConfiguration::UserChoice && !sessionsActiveConfiguration().isValid()) {
            // No configuration has been assigned yet, just ignore this event. This happens on Harmattan when we get a change
            // of e.g. an office WiFi connection in reply to us trying to open a session with the system's default configuration.
            return;
        }

        m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("Network Session"),
                          m_session->configuration().type() == QNetworkConfiguration::InternetAccessPoint ?
                              QString::fromUtf8("Change of system's default configuration: %1. Currently using %2.")
                              .arg(conf.name(), m_session->configuration().name())
                            :
                              QString::fromUtf8("Change of system's default configuration: %1. Currently using %2 (active: %3).")
                              .arg(conf.name(), m_session->configuration().name(), sessionsActiveConfiguration().name()));
        reconnect = true;
    }

    if (reconnect) {
        m_model->setNetworkPolicy(NETWORK_OFFLINE);
        resetSession();
    }
}

/** @short Make sure that we switch over to whatever session which is now the default one */
void NetworkWatcher::resetSession()
{
    delete m_session;
    const auto conf = m_netConfManager->defaultConfiguration();
    m_session = new QNetworkSession(conf, this);

    QString buf;
    QTextStream ss(&buf);
    ss << "Switched to network configuration " << conf.name() << " (" << conf.bearerTypeName() << ", " << conf.identifier() << ")";
    m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("Network Session"), buf);
    connect(m_session, SIGNAL(opened()), this, SLOT(reconnectModelNetwork()));
    m_session->open();
}

QNetworkConfiguration NetworkWatcher::sessionsActiveConfiguration() const
{
    auto conf = m_session->configuration();
    if (m_session->configuration().type() == QNetworkConfiguration::InternetAccessPoint) {
        return conf;
    } else {
        QString activeConfId = m_session->sessionProperty(QLatin1String("ActiveConfiguration")).toString();
        // yes, this can return an invalid session
        return m_netConfManager->configurationFromIdentifier(activeConfId);
    }
}
#endif

}
}
