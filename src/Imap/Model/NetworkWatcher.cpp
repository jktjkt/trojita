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
#include "Model.h"

namespace Imap {
namespace Mailbox {

NetworkWatcher::NetworkWatcher(QObject *parent, Model *model):
    QObject(parent), m_model(model), m_desiredPolicy(NETWORK_OFFLINE)
{
    Q_ASSERT(m_model);
    connect(model, SIGNAL(networkPolicyChanged()), this, SIGNAL(effectiveNetworkPolicyChanged()));
    connect(model, SIGNAL(networkError(const QString &)), this, SLOT(attemptReconnect()));
    connect(model, SIGNAL(connectionStateChanged(QObject*, Imap::ConnectionState)),
            this, SLOT(handleConnectionStateChanged(QObject*, Imap::ConnectionState)));

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(MIN_RECONNECT_TIMEOUT/2);
    connect(m_reconnectTimer, SIGNAL(timeout()), this, SLOT(tryReconnect()));
}

/** @short Start the reconnect attempt cycle */
void NetworkWatcher::attemptReconnect()
{
    // Update the reconnect time-out value. Double it up to a max value of MAX_RECONNECT_TIMEOUT secs.
    m_reconnectTimer->setInterval(qMin(MAX_RECONNECT_TIMEOUT, m_reconnectTimer->interval()*2));
    m_reconnectTimer->start();
    m_model->logTrace(0, Common::LogKind::LOG_OTHER, QLatin1String("Network"),
                      tr("Attempting to reconnect in %1 secs").arg(m_reconnectTimer->interval()/1000));
    emit reconnectAttemptScheduled(m_reconnectTimer->interval());
}

void NetworkWatcher::resetReconnectTimer()
{
    m_reconnectTimer->stop();
    m_reconnectTimer->setInterval(MIN_RECONNECT_TIMEOUT/2);
    emit resetReconnectState();
}

void NetworkWatcher::handleConnectionStateChanged(QObject *parser, Imap::ConnectionState state)
{
    Q_UNUSED(parser);
    // These states signify that the socket is in QAbstractSocket::ConnectedState and (maybe) mark success after
    // a series of reconnect attempts. Timers for reconnection should be reset at this point.
    if (state == CONN_STATE_CONNECTED_PRETLS_PRECAPS || state == CONN_STATE_SSL_HANDSHAKE) {
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
}

/** @short Set the network access policy to "possible, but expensive" */
void NetworkWatcher::setNetworkExpensive()
{
    resetReconnectTimer();
    setDesiredNetworkPolicy(NETWORK_EXPENSIVE);
}

/** @short Set the network access policy to "it's cheap to use it" */
void NetworkWatcher::setNetworkOnline()
{
    resetReconnectTimer();
    setDesiredNetworkPolicy(NETWORK_ONLINE);
}

NetworkPolicy NetworkWatcher::desiredNetworkPolicy() const
{
    return m_desiredPolicy;
}

}
}
