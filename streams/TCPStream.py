# -*- coding: utf-8
"""Streamed TCP/IP connection"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

import socket
import select
from PollableStream import PollableStream
from common import default_timeout

__revision__ = "$Id$"

class TCPStream(PollableStream):
    """Streamed TCP/IP connection"""

    def __init__(self, host, port, timeout=default_timeout):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.connect((host, port))
        self._file = self._sock.makefile('rb', bufsize=0)
        self._r_poll = select.poll()
        self._r_poll.register(self._sock.fileno(), select.POLLIN | select.POLLHUP)
        self.timeout = int(timeout)
        self._sock.settimeout(timeout)
        self.okay = True

    def _close(self):
        return self._sock.close()

    def _flush(self):
        return self._file.flush()

    def _read(self, size):
        return self._file.read(size)

    def _readline(self):
        return self._file.readline()

    def _write(self, data):
        return self._file.write(data)
