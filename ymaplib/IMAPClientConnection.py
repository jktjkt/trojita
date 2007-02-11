# -*- coding: utf-8
"""Connection to an IMAP server"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

import inspect

__revision__ = '$Id$'

class IMAPClientConnection:
    """Wrapper around IMAPParser and Stream objects"""

    def __init__(self, stream_type, stream_args, auth_type, auth_args,
                  capabilities_mask=(), debug=0):
        stream = stream_type(*stream_args)
        self.parser = IMAPParser(stream, debug, capabilities_mask)
        self.parser.start_worker()
        if auth_type is not None:
            auth = auth_type(*auth_args)
            self.parser.cmd_authenticate(auth)
	# publish all public methods from IMAPParser
        items = inspect.getmembers(self.parser, inspect.ismethod)
        for method in items:
            if method[0].startswith('cmd_') or \
               method[0] in ('start_worker', 'stop_worker', 'get'):
                setattr(self, method[0], method[1])
        self.mailbox = None
