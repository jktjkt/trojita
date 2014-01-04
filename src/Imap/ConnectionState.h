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
#ifndef IMAP_CONNECTIONSTATE_H
#define IMAP_CONNECTIONSTATE_H

#include <QString>

namespace Imap
{

/** @short A human-readable state of the connection to the IMAP server */
typedef enum {
    CONN_STATE_NONE, /**< @short Initial state */
    CONN_STATE_HOST_LOOKUP, /**< @short Resolving hostname */
    CONN_STATE_CONNECTING, /**< @short Connecting to the remote host or starting the process */
    CONN_STATE_SSL_HANDSHAKE, /**< @short The SSL encryption is starting */
    CONN_STATE_SSL_VERIFYING, /**< @short The SSL connection processing is waiting for policy decision about whether to proceed or not */
    CONN_STATE_CONNECTED_PRETLS_PRECAPS, /**< @short Connection has been established but there's been no CAPABILITY yet */
    CONN_STATE_CONNECTED_PRETLS, /**< @short Connection has been established and capabilities are known but STARTTLS remains to be issued */
    CONN_STATE_STARTTLS_ISSUED, /**< @short The STARTTLS command has been sent */
    CONN_STATE_STARTTLS_HANDSHAKE, /**< @short The socket is starting encryption */
    CONN_STATE_STARTTLS_VERIFYING, /** @short The STARTTLS processing is waiting for policy decision about whether to proceed or not */
    CONN_STATE_ESTABLISHED_PRECAPS, /**< @short Waiting for capabilities after the encryption has been set up */
    CONN_STATE_LOGIN, /**< @short Performing login */
    CONN_STATE_POSTAUTH_PRECAPS, /**< @short Authenticated, but capabilities weren't refreshed yet */
    CONN_STATE_COMPRESS_DEFLATE, /**< @short Activating COMPRESS DEFLATE */
    CONN_STATE_AUTHENTICATED, /**< @short Logged in */
    CONN_STATE_SELECTING, /**< @short Selecting a mailbox -- initial state */
    CONN_STATE_SYNCING, /**< @short Selecting a mailbox -- performing synchronization */
    CONN_STATE_SELECTED, /**< @short Mailbox is selected and synchronized */
    CONN_STATE_FETCHING_PART, /** @short Downloading an actual body part */
    CONN_STATE_FETCHING_MSG_METADATA, /** @short Retrieving message metadata */
    CONN_STATE_LOGOUT, /**< @short Logging out */
} ConnectionState;

QString connectionStateToString(const ConnectionState state);

}

#endif // IMAP_CONNECTIONSTATE_H
