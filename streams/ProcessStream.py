# -*- coding: utf-8
"""Streamable interface to local process."""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

import os
import select
from PollableStream import PollableStream
from common import default_timeout

__revision__ = "$Id$"

class ProcessStream(PollableStream):
    """Streamable interface to local process.

Doesn't work on Win32 systems due to their lack of poll() functionality on pipes.
"""

    def __init__(self, command, timeout=default_timeout):
        # disable buffering, otherwise readline() might read more than just
        # one line and following poll() would say that there's nothing to read
        (self._w, self._r) = os.popen2(command, bufsize=0)
        self._r_poll = select.poll()
        self._r_poll.register(self._r.fileno(), select.POLLIN | select.POLLHUP)
        self.timeout = int(timeout)
        self.okay = True

    def _close(self):
        self._r.close()
        self._w.close()

    def _flush(self):
        return self._w.flush()

    def _read(self, size):
        return self._r.read(size)

    def _readline(self):
        return self._r.readline()

    def _write(self, data):
        return self._w.write(data)
