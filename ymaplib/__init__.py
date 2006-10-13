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
__revision__ = "$Id$"

def get_revisions():
    import sys, types
    revisions = {}
    for _module in sys.modules.values():
        if type(_module) == types.ModuleType and _module.__name__ not in revisions and _module.__name__.startswith('ymaplib.'):
            revisions[_module.__name__] = _module.__revision__
    return revisions

def banner():
    buf = ["ymaplib version %s:" % __version__, " ymaplib:\t\t%s" % __revision__]
    for (name, rev) in get_revisions().items():
        buf.append(" %s:\t%s" % (name, rev))
    return "\n".join(buf)

if __name__ == "__main__":
    print banner()
