#ifndef IMAP_CONNECTIONSTATE_H
#define IMAP_CONNECTIONSTATE_H

namespace Imap {

    /** @short A human-readable state of the connection to the IMAP server */
    typedef enum {
        CONN_STATE_NONE, /**< @short Initial state */
        CONN_STATE_HOST_LOOKUP, /**< @short Resolving hostname */
        CONN_STATE_CONNECTING, /**< @short Connecting to the remote host or starting the process */
        CONN_STATE_STARTTLS, /**< @short Negotiating authentication */
        CONN_STATE_ESTABLISHED, /**< @short The connection is ready, including optional encryption */
        CONN_STATE_LOGIN, /**< @short Performing login */
        CONN_STATE_AUTHENTICATED, /**< @short Logged in */
        CONN_STATE_SYNCING, /**< @short Selecting a mailbox -- performing synchronization */
        CONN_STATE_SELECTED, /**< @short Mailbox is selected and synchronized */
        CONN_STATE_LOGOUT, /**< @short Logging out */
    } ConnectionState;


}

#endif // IMAP_CONNECTIONSTATE_H
