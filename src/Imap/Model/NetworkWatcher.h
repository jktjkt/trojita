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

#ifndef TROJITA_IMAP_NETWORKWATCHER_H
#define TROJITA_IMAP_NETWORKWATCHER_H

#include <QObject>
#include "Imap/Model/NetworkPolicy.h"
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
#define TROJITA_HAS_QNETWORKSESSION
#endif

class QNetworkConfigurationManager;
class QNetworkConfiguration;
class QNetworkSession;

namespace Imap {
namespace Mailbox {
class Model;

/** @short Provide network session management to the Model */
class NetworkWatcher: public QObject
{
    Q_OBJECT
public:
    NetworkWatcher(QObject *parent, Model *model);

    NetworkPolicy desiredNetworkPolicy() const;
    NetworkPolicy effectiveNetworkPolicy() const;

public slots:
    void setNetworkOffline();
    void setNetworkExpensive();
    void setNetworkOnline();

    void reconnectModelNetwork();
    void onGlobalOnlineStateChanged(const bool online);
    void networkConfigurationChanged(const QNetworkConfiguration& conf);
    void networkSessionError();

signals:
    void effectiveNetworkPolicyChanged();

private:
    Model *m_model;
    NetworkPolicy m_desiredPolicy;
    QNetworkConfigurationManager *m_netConfManager;
    QNetworkSession *m_session;

    void setDesiredNetworkPolicy(const NetworkPolicy policy);
    bool isOnline() const;
#ifdef TROJITA_HAS_QNETWORKSESSION
    void resetSession();
    QNetworkConfiguration sessionsActiveConfiguration() const;
#endif
};

}
}

#endif
