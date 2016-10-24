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

#include <QTimer>
#include "NetworkWatcher.h"
#include "ImapAccess.h"
#include "Model.h"

namespace Imap {
namespace Mailbox {

NetworkWatcher::NetworkWatcher(ImapAccess *parent, Model *model):
    QObject(parent), m_imapAccess(parent), m_model(model), m_desiredPolicy(NETWORK_OFFLINE)
{
    // No assert for m_imapAccess. The test suite doesn't create one.
    Q_ASSERT(m_model);
    connect(m_model, &Model::networkPolicyChanged, this, &NetworkWatcher::effectiveNetworkPolicyChanged);
    connect(m_model, &Model::networkError, this, &NetworkWatcher::attemptReconnect);
    connect(m_model, &Model::connectionStateChanged, this, &NetworkWatcher::handleConnectionStateChanged);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(MIN_RECONNECT_TIMEOUT/2);
    connect(m_reconnectTimer, &QTimer::timeout, this, &NetworkWatcher::tryReconnect);
}

NetworkPolicy NetworkWatcher::effectiveNetworkPolicy() const
{
    // Do we have any connection which is really alive?
    auto parser = std::find_if(m_model->m_parsers.cbegin(), m_model->m_parsers.cend(),
                 [](const Imap::Mailbox::ParserState & state) {
        return state.connState != CONN_STATE_LOGOUT && state.connState >= CONN_STATE_AUTHENTICATED;
    });
    return parser == m_model->m_parsers.cend() ? NETWORK_OFFLINE : m_model->networkPolicy();
}

/** @short Start the reconnect attempt cycle */
void NetworkWatcher::attemptReconnect()
{
    // Update the reconnect time-out value. Double it up to a max value of MAX_RECONNECT_TIMEOUT secs.
    m_reconnectTimer->setInterval(qMin(MAX_RECONNECT_TIMEOUT, m_reconnectTimer->interval()*2));
    m_reconnectTimer->start();
    m_model->logTrace(0, Common::LogKind::LOG_OTHER, QStringLiteral("Network"),
                      tr("Attempting to reconnect in %n secs", 0, m_reconnectTimer->interval()/1000));
    emit reconnectAttemptScheduled(m_reconnectTimer->interval());
}

void NetworkWatcher::resetReconnectTimer()
{
    m_reconnectTimer->stop();
    m_reconnectTimer->setInterval(MIN_RECONNECT_TIMEOUT/2);
    emit resetReconnectState();
}

void NetworkWatcher::handleConnectionStateChanged(uint parserId, Imap::ConnectionState state)
{
    Q_UNUSED(parserId);
    // Only treat a real, 100%-successful connection as a succeeding one. This is because
    // some of our socket backends (QProcess, for example) do not really have any possibility of signaling
    // "hey, network is available" to the other side.
    //
    // How come? Because "a process has started" could very well mean "yeha, SSH has launched, but we
    // don't know anything about the net yet". We *could* hack the QProcess backend to wait for a first byte
    // and signal "ok, connected" only afterwards, but that sounds a bit ugly because it uses "magic" knowledge
    // that in case IMAP, a server always greets us with something, which would be a bit layer-violating.
    // Not that the current enum values of the ConnectionState are free of IMAP idioms, but...
    if (state >= CONN_STATE_AUTHENTICATED && state < CONN_STATE_LOGOUT) {
        resetReconnectTimer();
    }
}

/** @short Set model's network policy to the desiredPolicy */
void NetworkWatcher::tryReconnect()
{
    m_model->setNetworkPolicy(m_desiredPolicy);
}

/** @short Set the network access policy to "no access allowed" */
void NetworkWatcher::setNetworkOffline()
{
    resetReconnectTimer();
    setDesiredNetworkPolicy(NETWORK_OFFLINE);
    emit desiredNetworkPolicyChanged(NETWORK_OFFLINE);
}

/** @short Set the network access policy to "possible, but expensive" */
void NetworkWatcher::setNetworkExpensive()
{
    if (!m_imapAccess || !m_imapAccess->isConfigured())
        return;

    resetReconnectTimer();
    setDesiredNetworkPolicy(NETWORK_EXPENSIVE);
    emit desiredNetworkPolicyChanged(NETWORK_EXPENSIVE);
}

/** @short Set the network access policy to "it's cheap to use it" */
void NetworkWatcher::setNetworkOnline()
{
    if (!m_imapAccess || !m_imapAccess->isConfigured())
        return;

    resetReconnectTimer();
    setDesiredNetworkPolicy(NETWORK_ONLINE);
    emit desiredNetworkPolicyChanged(NETWORK_ONLINE);
}

NetworkPolicy NetworkWatcher::desiredNetworkPolicy() const
{
    return m_desiredPolicy;
}

}
}
