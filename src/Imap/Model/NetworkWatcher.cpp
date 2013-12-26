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
    m_desiredPolicy = policy;
    if (m_model->networkPolicy() == NETWORK_OFFLINE && policy != NETWORK_OFFLINE) {
        // We are asked to connect, the model is not connected yet
        if (isOnline()) {
            reconnectModelNetwork();
        } else {
            // We aren't online yet, but we will become online at some point. When that happens, reconnectModelNetwork() will
            // be called, so there is nothing to do from this place.
#ifdef TROJITA_HAS_QNETWORKSESSION
            m_session->open();
#endif
        }
    } else if (m_model->networkPolicy() != NETWORK_OFFLINE && policy == NETWORK_OFFLINE) {
        // This is pretty easy -- just disconnect the model
        m_model->setNetworkPolicy(NETWORK_OFFLINE);
    } else {
        // No need to connect/disconnect/reconnect, yay!
        m_model->setNetworkPolicy(m_desiredPolicy);
    }
}

void NetworkWatcher::reconnectModelNetwork()
{
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
*/
void NetworkWatcher::networkConfigurationChanged(const QNetworkConfiguration &conf)
{
    if (conf.state().testFlag(QNetworkConfiguration::Active) || conf == m_session->configuration()) {
        // We shall react to both changes of the "session's own configuration" (perhaps it is no longer available)
        // as well as to changes of session which is now set to be the system's default one.
        // There's apparently no better signal, see http://lists.qt-project.org/pipermail/interest/2013-December/010374.html
        m_model->setNetworkPolicy(NETWORK_OFFLINE);
        resetSession();
        reconnectModelNetwork();
    }
}

/** @short Make sure that we switch over to whatever session which is now the default one */
void NetworkWatcher::resetSession()
{
    delete m_session;
    m_session = new QNetworkSession(m_netConfManager->defaultConfiguration(), this);
    connect(m_session, SIGNAL(opened()), this, SLOT(reconnectModelNetwork()));
    m_session->open();
}
#endif

}
}
