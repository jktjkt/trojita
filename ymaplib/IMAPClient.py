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

class _IMAPClientPoolItem:
    """"Tuple" of Stream, Parser, Mailbox mailbox and State"""

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
        max_conn_mailbox -- maximal number of connections pointing to a given
                            mailbox
        capabilities_mask -- ignore listed IMAP capabilities
        debug -- debugging verbosity"""

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

    def _conn_add(self):
        """Create a new connection to server"""

        stream = self._stream_type(*self._stream_args)
        parser = trojita.ymaplib.IMAPParser(stream, self._debug,
        self.capabilities_mask)
        parser.start_worker()
        # FIXME: method for authenticating, but only after a secure channel is
        # established
        # -> move the start_tls invocation to the respective Authenticator?
        connection = _IMAPClientConnectionPoolItem(stream, parser)
        self._pool.append(connection)
        return connection

    # begin of public API declarations
    # manipulation of mailboxes

    def namespace_get_broken_default(self):
        """Probably broken attempt to guess dumb namespace

        This is only useful for crappy servers that don't support the NAMESPACE
        extension. Users are likely to specify they own namespace in addition to
        this guessed default."""
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
        me to namespace_get_broken_default()."""
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
        """Returns (structure) of mailboxes mathing given criteria

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

    def mbox_open(self, namespace, mailbox, mode=O_RDWR):
        """Express our interest in accessing this mailbox

        Before we can access a mailbox, the underlying IMAP protocol must
        SELECT/EXAMINE it.

        namespace -- which namespace this mailbox belongs to
        mailbox -- name of the mailbox to open
        mode -- (O_RDONLY, O_RDWR) whether to do a SELECT or EXAMINE
        """
        raise FIXME

    def mbox_close(self, namespace, mailbox):
        """We don't expect to access this mailbox anymore

        At least for some time, we don't expect we will access this mailbox.
        """
        raise FIXME

    def mbox_stat(self, namespace, mailbox):
        """Get some information about a mailbox

        Feel free to abuse this function in any way you like it; it's smart
        enough to cache data.

        FIXME: return values...
        """
        raise FIXME

    def mbox_message_store(self, namespace, mailbox, message, flags=None,
                            date=None):
        """Store a message into the specified mailbox

        namespace, mailbox -- which mailbox
        message -- FIXME: what type?
        flags -- IMAP Flags
        date -- IMAP Internal Date
        """
        raise FIXME

    def mbox_expunge(self, namespace, mailbox):
        """Expunge all deleted messages from a mailbox"""
        raise FIXME

    def mbox_search(self, namespace, mailbox, criteria, charset=None):
        """Search mailbox using specified search terms

        namespace, mailbox -- usual mailbox specification
        criteria -- Unicode string to be passed to the server, see RFC 3501
                    page 49 for details

        By default, searches are done either in UTF-8 or in US-ASCII encoding.
        Other encodings will be tried it the reply from the server suggests that
        we're talking to a dumb machine that can't speak UTF-8.
        """
        raise FIXME

    def mbox_message(self, namespace, mailbox, message):
        """Returns an IMAPMessage instance (email.Message on steroids)"""
        raise FIXME

    def message_copy(self, old_namespace, old_mailbox, old_message,
                     new_namespace, new_mailbox):
        """Copy message to another mailbox

        old_namespace, old_mailbox -- source mailbox
        old_message -- which message to copy
        new_namespace, new_mailbox -- where to put it
        """
        raise FIXME

    def message_get(self, namespace, mailbox, message):
        """Get an IMAPMessage from UID/UIDVALIDITY combo

        namespace, mailbox -- mailbox specification
        message -- tuple UIDVALIDITY/UID
        """
        raise FIXME

