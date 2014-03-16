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

#include "NetworkWatcher.h"
#include "Model.h"

namespace Imap {
namespace Mailbox {

NetworkWatcher::NetworkWatcher(QObject *parent, Model *model):
    QObject(parent), m_model(model), m_desiredPolicy(NETWORK_OFFLINE)
{
    Q_ASSERT(m_model);
    connect(model, SIGNAL(networkPolicyChanged()), this, SIGNAL(effectiveNetworkPolicyChanged()));
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

}
}
