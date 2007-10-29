# -*- coding: utf-8
"""A template for streams that support the poll() syscall"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

import select
import time
from Stream import Stream

__revision__ = "$Id$"

class PollableStream(Stream):
    """_has_data() helper for those streams that can use poll() for _has_data()"""
    def _has_data(self, timeout):
        if timeout is None or timeout < -0.000001:
            poll_timeout = None
            # we won't wait if timeout is "wait forever", ie negative or None...
            sleep_timeout = 0
        else:
            poll_timeout = timeout * 1000
            sleep_timeout = timeout
        polled = self._r_poll.poll(poll_timeout)
        if len(polled):
            result = polled[0][1]
            if result & select.POLLIN:
                if result & select.POLLHUP:
                    # closed connection, data still available
                    self.okay = False
                return True
            elif result & select.POLLHUP:
                # connection is closed
                time.sleep(sleep_timeout)
                self.okay = False
                return False
            else:
                return False
        else:
            return False
