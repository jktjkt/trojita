# -*- coding: utf-8
"""RFC822 message stored on an IMAP server"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'

class IMAPMessage:
    """RFC822 message stored on an IMAP server"""

    def __init__(self, flags=None, internaldate=None, size=None,
                 envelope=None, body=None, text=None):
        self.flags = flags # list (set?)
        self.internaldate = internaldate # date?
        self.size = size # int
        self.envelope = envelope # IMAPEnvelope
        self.body = body # email.Message
        # note - we don't cache partially fetched data
