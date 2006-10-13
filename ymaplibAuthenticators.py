# -*- coding: utf-8

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006

__revision__ = '$Id$'

# FIXME: more authenticators...

class Authenticator:
    """Helper for authentications.

Derived subclasses should override the _chat() method.
"""
# FIXME: auth mechanism *might* negotiate a switch to some encryption...

    def __todo(self):
        raise NotImplementedError()

    mechanism = None

    def __repr__(self):
        return "<ymaplib.Authenticator>"

    def chat(self, input):
        """Do the chat :)

Expects one line of server's respone. returns data to send back 
or None if we changed our mind."""
        return self._chat(input)

    _chat = __todo


class PLAINAuthenticator(Authenticator):
    """Implements PLAIN SASL authentication"""

    mechanism = 'PLAIN'

    def __init__(self, username, password):
        self.username = username
        self.password = password

    def _chat(self, input):
        return "\x00%s\x00%s" % (self.username.encode("utf-8"), 
                                 self.password.encode("utf-8"))
