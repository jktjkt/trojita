# -*- coding: utf-8
"""IMAP4rev1 client library

Written with aim to be as much RFC3501 compliant as possible and wise :)

References: IMAP4rev1 - RFC3501

Author: Jan Kundrát <jkt@flaska.net>
Inspired by the Python's imaplib library.
"""

from IMAPResponse import IMAPResponse
from IMAPNIL import IMAPNIL
from IMAPThreadItem import IMAPThreadItem
from IMAPParser import IMAPParser
from IMAPEnvelope import IMAPEnvelope
from IMAPMessage import IMAPMessage
from exceptions import (InvalidResponseError, ParseError, UnknownResponseError,
                        TimeoutError, DisconnectedError,
                        CommandContinuationRequest)
from IMAPClient import IMAPClient
from IMAPClientConnection import IMAPClientConnection
from IMAPMailbox import IMAPMailbox
from authenticators import (Authenticator, PLAINAuthenticator)

__version__ = "0.1"
__revision__ = '$Id$'
#__all__ = ["IMAPResponse", "IMAPNIL", "IMAPThreadItem", "IMAPParser", "IMAPEnvelope", "IMAPMessage"]
#
#__all__ = ["ProcessStream", "TCPStream", "IMAPResponse", "IMAPNIL",
#           "IMAPThreadItem", "IMAPParser", "IMAPEnvelope", "IMAPMessage",
#           "IMAPMailbox"]

if __name__ == "__main__":
    print "ymaplib version %s (SVN %s)" % (__version__, __revision__)
