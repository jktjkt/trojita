# -*- coding: utf-8
"""IMAP4rev1 client library

Written with aim to be as much RFC3501 compliant as possible and wise :)

References: IMAP4rev1 - RFC3501

Author: Jan Kundrát <jkt@flaska.net>
Inspired by the Python's imaplib library.


* How to use:

>>> import trojita.ymaplib
>>> import streams
>>> client = trojita.ymaplib.IMAPClient(
        trojita.streams.ProcessStream, ('dovecot --exec-mail imap',))
>>> if not client.namespace_supported():
...     client.namespace_set(client.namespace_get_broken_default())
>>> for namespace in client.namespace_get():
...     print client.mbox_get_tree(namespace, "*", True)
>>> inbox = client.mailbox(client.namespace_get[0], "INBOX")
>>> client.logout()

"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__version__ = "0.1"
__revision__ = "$Id$"
__all__ = ["IMAPResponse", "IMAPNIL", "IMAPThreadItem", "IMAPParser",
    "IMAPEnvelope", "IMAPMessage", "InvalidResponseError", "ParseError",
    "UnknownResponseError", "TimeoutError, DisconnectedError",
    "CommandContinuationRequest", "AttrReadOnlyError"]

from IMAPResponse import IMAPResponse
from IMAPNIL import IMAPNIL
from IMAPThreadItem import IMAPThreadItem
from IMAPParser import IMAPParser
from IMAPEnvelope import IMAPEnvelope
from IMAPMessage import IMAPMessage
from IMAPMailbox import IMAPMailbox
from IMAPNamespace import IMAPNamespace, IMAPNamespaceItem
from IMAPMailboxItem import IMAPMailboxItem
from exceptions import (InvalidResponseError, ParseError, UnknownResponseError,
                        TimeoutError, DisconnectedError,
                        CommandContinuationRequest, AttrReadOnlyError)
from IMAPClient import IMAPClient
from authenticators import (Authenticator, PLAINAuthenticator)
