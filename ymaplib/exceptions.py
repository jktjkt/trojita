# -*- coding: utf-8
"""Common exceptions for ymaplib"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'

# Already defined...
#class NotImplementedError(Exception):
#    """Something is not yet implemented"""
#    pass

class InvalidResponseError(Exception):
    """Invalid, unexpected, malformed or unparsable response.

    Possible reasons might be YMAPlib bug, IMAP server error or connection
    borkage.
    """
    pass

class ParseError(InvalidResponseError):
    """Unable to parse server's response"""
    pass

class UnknownResponseError(InvalidResponseError):
    """Unknown response from server"""
    pass

class TimeoutError(Exception):
    """Socket timed out"""
    pass

class DisconnectedError(Exception):
    """Disconnected from server"""
    pass

class CommandContinuationRequest(Exception):
    """Command Continuation Request"""
    pass

class AttrReadOnlyError(TypeError):
    """Object doesn't support assignment of this property"""
    pass
