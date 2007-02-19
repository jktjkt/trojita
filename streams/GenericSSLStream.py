# -*- coding: utf-8
"""A generic SSL stream template"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = "$Id$"

from common import default_timeout
from TCPStream import TCPStream

class GenericSSLStream(TCPStream):
    """Generic SSL Stream template

Subclasses should override _ssl_close(), _ssl_has_data(), _ssl_read(), 
_ssl_redline(), _ssl_starttls() and _ssl_write().
"""

    def __todo(self):
        """Dummy _ssl_*() function"""
        raise NotImplementedError()

    _ssl_close = __todo
    _ssl_has_data = __todo
    _ssl_read = __todo
    _ssl_readline = __todo
    _ssl_starttls = __todo
    _ssl_write = __todo

    def __init__(self, host, port, timeout=default_timeout):
        TCPStream.__init__(self, host, port, timeout)
        self.ssl = False

    def _close(self):
        if self.ssl:
            return self._ssl_close()
        else:
            return TCPStream._close(self)

    def _flush(self):
        if self.ssl:
            return self._ssl_flush()
        else:
            return TCPStream._flush(self)

    def _has_data(self, timeout):
        if self.ssl:
            return self._ssl_has_data(timeout)
        else:
            return TCPStream._has_data(self, timeout)

    def _read(self, size):
        if self.ssl:
            return self._ssl_read(size)
        else:
            return TCPStream._read(self, size)

    def _readline(self):
        if self.ssl:
            return self._ssl_readline()
        else:
            return TCPStream._readline(self)

    def _starttls(self):
        if self.ssl:
            raise NotImplementedError("can't initiate SSL/TLS twice")
        else:
            self.ssl = self._ssl_starttls()
            return self.ssl

    def _write(self, data):
        if self.ssl:
            return self._ssl_write(data)
        else:
            return TCPStream._write(self, data)
