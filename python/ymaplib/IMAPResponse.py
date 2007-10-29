# -*- coding: utf-8
"""A simple container to hold a response from an IMAP server"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'

class IMAPResponse:
    """Simple container to hold a response from IMAP server.

    Storage only, don't expect to get any usable method here :)
    """
    def __init__(self):
        self.tag = False
        # response tag or None if untagged
        self.kind = None
        # which "kind" of response is it? (PREAUTH, CAPABILITY, BYE, EXISTS,...)
        self.response_code = (None, None)
        # optional "response code" - first item is kind of message,
        # second either tuple of parsed items, string, number or None
        self.data = ()
        # string with human readable text or tuple with parsed items

    def __repr__(self):
        s = "<trojita.ymaplib.IMAPResponse - "
        if self.tag is None:
            s += "untagged"
        else:
            s += "tag %s" % self.tag
        return s + ", kind: " + unicode(self.kind) + ', response_code: ' + \
               unicode(self.response_code) + ", data: " + unicode(self.data) + \
               ">"

    def __eq__(self, other):
        return self.tag == other.tag and self.kind == other.kind and \
          self.response_code == other.response_code and self.data == other.data

    def __ne__(self, other):
        return not self.__eq__(other)
