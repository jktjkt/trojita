# -*- coding: utf-8
"""A generic Stream temeplate"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = "$Id$"

from common import default_timeout

class Stream:
    """Stream object.

Stream is a special object that supports several file-object-like methods like
close(), flush(), read(), readline() and write(). As a bonus :), there is
a has_data() function that check if you can read from the stream without
blocking, and a starttls() which (if supported) switches to the encrypted
communication channel on the fly.

This class is just a template for other classes. They should override
_close(), _flush(), _has_data(), _read(), _readline(), _starttls() and _write()
with their own implementation. These names start with underscore to prevent the
need to redefine docstrings.
"""
    def __todo(self):
        """Default handler for methods that aren't implemented"""
        raise NotImplementedError("streams.Stream doesn't support this method")

    def close(self):
        return self._close()

    def flush(self):
        return self._flush()

    def has_data(self, timeout=default_timeout):
        """Check if we can read from socket without blocking

Timeout is an optional parameter specifying the maximum time to wait for the
result. If None, there's no timeout - the function will block until there is
something to read. If timeout is zero, the function will return immediately.
Positive floating point value is number of seconds to wait.
"""
        return self._has_data(timeout)

    def read(self, size=-1):
        return self._read(size)

    def readline(self):
        return self._readline()

    def starttls(self):
        """Setup a SSL/TLS session over existing connection"""
        return self._starttls()

    def write(self, data):
        return self._write(data)

    close.__doc__ = file.close.__doc__
    flush.__doc__ = file.flush.__doc__
    read.__doc__ = file.read.__doc__
    readline.__doc__ = file.readline.__doc__
    write.__doc__ = file.write.__doc__
    _close = __todo
    _flush = __todo
    _has_data = __todo
    _read = __todo
    _readline = __todo
    _starttls = __todo
    _write = __todo
