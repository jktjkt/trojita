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

#include <QObject>
#include "ConnectionState.h"

namespace Imap
{

QString connectionStateToString(const ConnectionState state)
{
    switch (state) {
    case CONN_STATE_NONE:
        return QString();
    case CONN_STATE_HOST_LOOKUP:
        return QObject::tr("Resolving hostname...");
    case CONN_STATE_CONNECTING:
        return QObject::tr("Connecting to the IMAP server...");
    case CONN_STATE_SSL_HANDSHAKE:
        return QObject::tr("Starting encryption (SSL)...");
    case CONN_STATE_SSL_VERIFYING:
        return QObject::tr("Checking certificates (SSL)...");
    case CONN_STATE_CONNECTED_PRETLS_PRECAPS:
        return QObject::tr("Checking capabilities...");
    case CONN_STATE_CONNECTED_PRETLS:
        return QObject::tr("Waiting for encryption...");
    case CONN_STATE_STARTTLS_ISSUED:
        return QObject::tr("Asking for encryption...");
    case CONN_STATE_STARTTLS_HANDSHAKE:
        return QObject::tr("Starting encryption (STARTTLS)...");
    case CONN_STATE_STARTTLS_VERIFYING:
        return QObject::tr("Checking certificates (STARTTLS)...");
    case CONN_STATE_ESTABLISHED_PRECAPS:
        return QObject::tr("Checking capabilities (after STARTTLS)...");
    case CONN_STATE_LOGIN:
        return QObject::tr("Logging in...");
    case CONN_STATE_POSTAUTH_PRECAPS:
        return QObject::tr("Checking capabilities (after login)...");
    case CONN_STATE_COMPRESS_DEFLATE:
        return QObject::tr("Activating compression...");
    case CONN_STATE_AUTHENTICATED:
        return QObject::tr("Logged in.");
    case CONN_STATE_SELECTING:
        return QObject::tr("Opening mailbox...");
    case CONN_STATE_SYNCING:
        return QObject::tr("Synchronizing mailbox...");
    case CONN_STATE_SELECTED:
        return QObject::tr("Mailbox opened.");
    case CONN_STATE_FETCHING_PART:
        return QObject::tr("Downloading message...");
    case CONN_STATE_FETCHING_MSG_METADATA:
        return QObject::tr("Downloading message structure...");
    case CONN_STATE_LOGOUT:
        return QObject::tr("Logged out.");
    }
    Q_ASSERT(false);
    return QString();
}

}
