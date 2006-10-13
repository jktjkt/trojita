# -*- coding: utf-8
"""A client to an IMAP server"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006

__revision__ = '$Id$'


class IMAPClient:
    """A client to IMAP server"""

    def __init__(self, stream_type, stream_args, auth_type, auth_args,
                  max_connections=None, capabilities_mask=(), debug=0):
        self._stream_type = stream_type
        self._stream_args = stream_args
        self._auth_type = auth_type
        self._auth_args = auth_args
        self._connections = []
        self.max_connections = max_connections
        self.capabilities_mask = capabilities_mask
        self._debug = debug
