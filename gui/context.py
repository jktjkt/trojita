# -*- coding: utf-8
"""Context for spcifying which account/mailbox/message/part are we talking about"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'

class Context:
    """Specifying a context for various Generic methods"""

    def __init__(self, account=None, mailbox=None, msg_offset=None, part=None):
        self.account = account
        self.mailbox = mailbox
        self.msg_offset = msg_offset
        self.part = part

    def __repr__(self):
        return "<trojita.gui.generic.Context: account %s, mailbox %s, msg_offset %s, part %s>" \
            % (str(self.account), str(self.mailbox), str(self.msg_offset),
               str(self.part))

    def check(self, *args):
        """Check a "context" for required items"""
        for item in args:
            if item == "account":
                if self.account is None:
                    raise ValueError("Context lacks account specification")
            elif item == "mailbox":
                if self.mailbox is None:
                    raise ValueError("Context lacks mailbox specification")
            elif item == "msg_offset":
                if self.msg_offset is None:
                    raise ValueError("Context lacks msg_offset specification")
            elif item == "part":
                if self.part is None:
                    raise ValueError("Context lacks part specification")
            else:
                raise ValueError("Unknown item to check: '%s'" % str(item))
