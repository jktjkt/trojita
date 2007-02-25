# -*- coding: utf-8
"""A client to an IMAP server

You usually have one IMAPClient for each IMAP server you want to access. When
creating, you pass it all the shiny details like how to authenticate to this
server, what capabilities to ban etc, how many connections per server/mailbox
you can afford etc. IMAPClient is clever enough to maintain a pool of existing
IMAPParsers and work with them.

Or, at least, should be when it's done :)


* Mailbox specification:

Several methods expects a specification of mailbox which you want to oeprate
with. As IMAP supports nested mailboxes and their textual representation can
contain multiple differring delimiters, we don't accept an IMAP name, but rather
a tuple/list specifying the "path" to mailbox, this: ("foo", "bar", "baz") can
be translated to, say, "foo.bar/baz".

FIXME: is that really required?
"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'

import types

class _IMAPClientPoolItem:
    """"Tuple" of Stream, Parser, Mailbox name and State"""

    def __init__(self, stream, parser):
        self.stream = stream
        self.parser = parser


class IMAPClient:
    """A client to IMAP server"""

    def __init__(self, stream_type, stream_args, completion_callback=None,
                 auth_type=None, auth_args=None, max_conn_server=1,
                 max_conn_mailbox=1, capabilities_mask=(), debug=0):
        """IMAPClient contructor
        
        stream_type -- class name of the stream to create
        stream_args -- what to use for its creation
        callback -- function that is called when some command is completed
        auth_type -- class name of the Authenticator
        auth_args -- what args to pass to the authenticator
        max_conn_server -- maximal number of connections to server
        max_conn_mailbox -- maximal number of connections pointing to a given mailbox
        capabilities_mask -- ignore listed IMAP capabilities
        debug -- debugging verbosity"""

        self._stream_type = stream_type
        self._stream_args = stream_args
        self._completion_callback = completion_callback
        self._auth_type = auth_type
        self._auth_args = auth_args
        self._connections = []
        self.max_connections = max_connections
        # FIXME: capabilities_mask runtime changes
        self.capabilities_mask = capabilities_mask
        self._debug = debug

        self._pool = []

    def _conn_add(self):
        """Create a new connection to server"""

        stream = self._stream_type(*self._stream_args)
        parser = trojita.ymaplib.IMAPParser(stream, self._debug, self.capabilities_mask)
        parser.start_worker()
        # FIXME: method for authenticating, but only after a secure channel is
        # established
        # -> move the start_tls invocation to the respective Authenticator?
        connection = _IMAPClientConnectionPoolItem(stream, parser)
        self._pool.append(connection)
        return connection

    # begin of public API declarations
    # manipulation of mailboxes

    def namespace_get(self):
        """Return namespaces (IMAPNamespace) as specified by server"""
        raise FIXME


    def mbox_get_delimiter(self, level, force_update=False):
        """Return a hiererarchy delimiter that is valid at the given level"""
        raise FIXME

    def mbox_get_names_all(self, level, search, force_update=False):
        """List all mailboxes matching given pattern

        Try not to use this function as it really returns everything and thus
        might be very slow and bandwidth-hungry. A good replacement is poking
        only for the level you're interested in."""
        raise FIXME

    def mbox_get_names(self, level, search, force_update=False):
        """List mailboxes in current level matching given search criteria"""
        raise FIXME

    def mbox_get_subscribed_all(self, level, search, force_update=False):
        """List all subscribed mailboxes matching given criteria"""
        raise FIXME

    def mbox_create(self, mailbox):
        """Create mailbox specified by the mailbox struct"""
        raise FIXME

    def mbox_delete(self, mailbox):
        """Delete mailbox specified by the mailbox struct"""
        raise FIXME

    def mbox_rename(self, old, new):
        """Rename specified mailbox"""
        raise FIXME


        
