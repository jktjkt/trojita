# -*- coding: utf-8
"""A client to an IMAP server

You usually have one IMAPClient for each IMAP server you want to access. When
creating, you pass it all the shiny details like how to authenticate to this
server, what capabilities to ban etc, how many connections per server/mailbox
you can afford etc. IMAPClient is clever enough to maintain a pool of existing
IMAPParsers and work with them.

Or, at least, should be when it's done :)


* Specifying a mailbox

Most of IMAPClient's methods wants you to "specify mailbox". To make things more
interesting, this option is represented by two variables, usually "namespace"
and "mailbox", the former being an IMAPNamespace instance and the latter a
list/tuple of (Unicode) strings where each item represents a mailbox name in the
hierarchy, or a None if we want to specify a hypotetical mailbox that is parent
for all mailboxes in that namespace. List of valid namespaces can be found by
the namespace_get() function.


* Specifying a message

Message identification is a bit tougher. IMAP has two numbers for each message
in a mailbox, its Sequence number and the UID. Sequence number is just an index
from the beginning of a mailbox, so it can't be considered as an "identifier" of
a message. The UIDs are, on the other hand, typically valid for a long time, and
get invalidated only when the UIDVALIDITY of a mailbox changes. Therefore we use
a tuple of (UIDVALIDITY, UID) for message identification.

"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'

from os import O_RDONLY, O_RDWR
import types

# constants for message identification
UIDVALIDITY=0
UID=1

# IMAPClient._pool
_POOL_NAMESPACE=0
_POOL_MAILBOX=1
_POOL_MODE=2
_POOL_PARSER=3

class IMAPClient:
    """A client to IMAP server"""

    def __init__(self, stream_type, stream_args, completion_callback=None,
                 auth_type=None, auth_args=None, max_connections=1,
                 capabilities_mask=(), debug=0):
        """IMAPClient contructor

        stream_type -- class name of the stream to create
        stream_args -- what to use for its creation
        completion_callback -- function that is called when a command is
                               completed
        auth_type -- class name of the Authenticator
        auth_args -- what args to pass to the authenticator
        max_conn_server -- maximal number of connections to server
        max_conn_mailbox -- maximal number of connections pointing to a given
                            mailbox
        capabilities_mask -- will pretend that IMAP server doesn't support
                             listed capabilities
        debug -- debugging verbosity
        """

        self._stream_type = stream_type
        self._stream_args = stream_args
        self._completion_callback = completion_callback
        self._auth_type = auth_type
        self._auth_args = auth_args
        # FIXME: requiring TLS
        self._connections = []
        self.max_connections = max_connections
        # FIXME: capabilities_mask runtime changes
        self.capabilities_mask = capabilities_mask
        self._debug = debug

        self._pool = []

    def _parser_create(self, namespace=None, mailbox=None, mode=O_RDONLY):
        """Create a new connection to server

        namespace, mailbox -- where to point it (or just don't switch to any
                              mailbox at all if both are None
        mode -- whether to SELECT or EXAMINE
        """

        stream = self._stream_type(*self._stream_args)
        parser = trojita.ymaplib.IMAPParser(stream, self._debug,
                                            self.capabilities_mask)
        #parser.start_worker()
        # FIXME: method for authenticating, but only after a secure channel is
        # established
        # -> move the start_tls invocation to the respective Authenticator?
        if namespace is not None and mailbox is not None:
            # switch to the specified mailbox
            # FIXME: determine the mode
            name = ''
            if mode == O_RDWR:
                tag = parser.cmd_select(name)
            else:
                tag = parser.cmd_examine(name)
            # FIXME: wait for completion
        self._pool.append([namespace, mailbox, mode, parser])
        return parser

    def _get_parser(self, namespace=None, mailbox=None, mode=O_RDONLY):
        """Returns an internally registered IMAPParser for particular mailbox

        Functions usually want to operate on some mailbox. This helper tries to
        find one that has it already SELECTed/EXAMINEd or creates another one.
        """

        if len(self._pool):
            # FIXME :)
            pass
        else:
            # no existing parsers -> just create one
            return self._parser_create(namespace, mailbox, mode)

        if len(candidates):
            # there are parsers that we can use
            # switch first of them to that mailbox and be done with it
            raise FIXME
        else:
            # all available slots are used, let's enter the killing spree
            raise FIXME

    def namespace_get_broken_default(self):
        """Probably broken attempt to guess dumb namespace

        This is only useful for crappy servers that don't support the NAMESPACE
        extension. Users are likely to specify their own namespace in addition
        to this guessed default.
        """
        # FIXME: see namespace_get() about how to do it
        raise FIXME

    def namespace_get(self):
        """Return supported namespaces, be it server defaults or a forced state

        We have to handle the case when server doesn't support the NAMESPACE
        extension (RFC 2342). If the namespace_supported() returns False, user
        should feed us with emaningfull data. As a temporary workaround, we
        initialize the list of supported namespaces to this:

        namespace = IMAPNamespace(IMAPNamespaceItem("", delimiter), (), ())

        where the delimiter is determined by running a 'LIST "" ""' command.
        This sucks but it's the beast we can handle such a situation.
        """
        raise FIXME

    def namespace_set(self, namespaces):
        """Force the namespace to the passed specification

        To use a default namespace (fex when dealing with shitty servers), set
        this to namespace_get_broken_default()."""
        raise FIXME

    def namespace_supported(self):
        """Checks whether server supports the NAMESPACE extension properly

        Returns IMAPNamespace list of supported namespaces or False when this
        extension is unsupported. In such a case, you are urged to feed us with
        correct namespace specification. See namespace_get() for details.
        """
        raise FIXME

    def mbox_get_tree(self, namespace, mailbox, show_nested=False,
                      subscribed_only=False):
        """Returns (structure) of mailboxes matching given criteria

        For a given namespace, we query the server for a list of mailboxes
        matching given name, or only the subscribed ones.

        namespace -- which namespace to operate on
        mailbox -- List of strings specifying the mailbox name. For each level,
                we replace "*" with "%" when the show_nested flag is set
        show_nested -- if set, allow wildcards to match even the separator
        subscribed_only -- invoke LSUB instead of LIST
        """
        raise FIXME

    def mbox_create(self, namespace, parent, mailbox):
        """Create new mailbox

        namespace and parent specify where to create at; parent==() means a
        top-level mailbox in the given namespace
        """
        raise FIXME

    def mbox_delete(self, namespace, mailbox):
        """Delete mailbox specified by the mailbox struct"""
        raise FIXME

    def mbox_rename(self, old_namespace, old_mailbox, new_namespace,
                    new_mailbox):
        """Rename specified mailbox"""
        raise FIXME

    def mailbox(self, namespace, mailbox, mode=O_RDWR):
        """Returns an IMAPMailbox instance

        namespace, mailbox -- which mailbox to open
        mode -- O_RDONLY, O_RDWR do we need write access?
        """
        raise FIXME

    def logout(self):
        """Close all connections to given server"""
        raise FIXME

IMAPClient.__doc__ = __doc__
