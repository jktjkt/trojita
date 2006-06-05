"""Supports a wide range of different streams."""

import os
import select
import socket
from OpenSSL import SSL

# default timeout in seconds
default_timeout = 30

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

    def has_data(self, timeout=0):
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

class ProcessStream(Stream):
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
        self,_w.close()

    def _flush(self):
        return self._w.flush()

    def _has_data(self, timeout):
        if timeout is None:
            timeout = self.timeout
        polled = self._r_poll.poll(timeout * 1000)
        if len(polled):
            result = polled[0][1]
            if result & select.POLLIN:
                if result & select.POLLHUP:
                    # closed connection, data still available
                    self.okay = False
                return True
            elif result & select.POLLHUP:
                # connection is closed
                # FIXME: what to do if timeout is "wait forever", ie negative or None?
                time.sleep(max(timeout, 0))
                self.okay = False
                return False
            else:
                return False
        else:
            return False

    def _read(self, size):
        return self._r.read(size)

    def _readline(self):
        return self._r.readline()

    def _write(self, data):
        return self._w.write(data)


class TCPStream(Stream):
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

    def _has_data(self, timeout):
        if timeout is None:
            timeout = self.timeout
        polled = self._r_poll.poll(timeout * 1000)
        if len(polled):
            result = polled[0][1]
            if result & select.POLLIN:
                if result & select.POLLHUP:
                    # closed connection, data still available
                    self.okay = False
                return True
            elif result & select.POLLHUP:
                # connection is closed
                # FIXME: what to do if timeout is "wait forever", ie negative or None?
                time.sleep(max(timeout, 0))
                self.okay = False
                return False
            else:
                return False
        else:
            return False


class GenericSSLStream(TCPStream):
    """Generic SSL Stream template"""

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


class OpenSSLStream(GenericSSLStream):
    """OpenSSL-based stream"""

    def _ssl_close(self):
        self._ssl_connection.shutdown()
        self._ssl_connection.sock_shutdown(socket.SHUT_RDWR)

    def _ssl_flush(self):
        # FIXME: flush :)
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
