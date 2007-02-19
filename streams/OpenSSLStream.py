# -*- coding: utf-8
"""OpenSSL-based stream"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

from OpenSSL import SSL
from GenericSSLStream import GenericSSLStream

__revision__ = "$Id$"

class OpenSSLStream(GenericSSLStream):
    """OpenSSL-based stream"""

    def _ssl_close(self):
        self._ssl_connection.shutdown()
        self._ssl_connection.sock_shutdown(socket.SHUT_RDWR)

    def _ssl_flush(self):
        # OpenSSL doesn't have anything like flush() so we just call
        # the underlying socket's flush()
        return self._file.flush()

    def _ssl_has_data(self, timeout):
        if (self._ssl_connection.pending() > 0) or len(self._buffer):
            return True
        old_timeout = self._sock.gettimeout()
        self._sock.settimeout(timeout)
        res = True
        try:
            self._buffer = self._ssl_connection.recv(1)
        except SSL.WantReadError:
            res = False
        self._sock.settimeout(old_timeout)
        return res

    def _ssl_read(self, size):
        if size == 0:
            return ''
        if len(self._buffer):
            buf = self._buffer
            self._buffer = ''
            if size > 1:
                return buf + self._ssl_connection.recv(size - 1)
            elif size == 1:
                return buf
            else:
                raise ValueError("No idea how to read %d bytes..." % size)
        else:
            return self._ssl_connection.recv(size)

    def _ssl_readline(self):
        # FIXME: optimization would be nice...
        buf = [self._buffer]
        self._buffer = ''
        while 1:
            char = self._ssl_connection.recv(1)
            buf.append(char)
            if char == '\n':
                break
        return ''.join(buf)

    def _ssl_starttls(self):
        # FIXME: add certificate checking etc etc
        self._ssl_context = SSL.Context(SSL.TLSv1_METHOD)
        self._ssl_connection = SSL.Connection(self._ssl_context, self._sock)
        self._ssl_connection.set_connect_state()
        self.ssl = True
        self._buffer = ''
        return True

    def _ssl_write(self, data):
        return self._ssl_connection.sendall(data)
