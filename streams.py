"""Support a wide range of different streams.

Stream is a special object that supports several file-object-like methods like
close(), flush(), read(), readline() and write(). As a bonus :), there is
a has_data() function that check if you can read from the stream without 
blocking, and a starttls() which (if supported) switches to the encrypted 
communication channel.
"""

import os
import select
import socket
from OpenSSL import SSL

class ProcessStream:
    """Streamable interface to local process.

Supports read(), readline(), write(), flush(), and has_data() methods. Doesn't
work on Win32 systems due to their lack of poll() functionality on pipes.
"""

    def __init__(self, command, timeout=-1):
        # disable buffering, otherwise readline() might read more than just
        # one line and following poll() would say that there's nothing to read
        (self._w, self._r) = os.popen2(command, bufsize=0)
        self._r_poll = select.poll()
        self._r_poll.register(self._r.fileno(), select.POLLIN | select.POLLHUP)
        self.timeout = int(timeout)
        self.flush = self._w.flush
        self.read = self._r.read
        self.readline = self._r.readline
        self.write = self._w.write
        self.okay = True

    def close(self):
        self._r.close()
        self,_w.close()

    def has_data(self, timeout=None):
        """Check if we can read from socket without blocking"""
        if timeout is None:
            timeout = self.timeout
        polled = self._r_poll.poll(timeout)
        if len(polled):
            result = polled[0][1]
            if result & select.POLLIN:
                if result & select.POLLHUP:
                    # closed connection, data still available
                    self.okay = False
                return True
            elif result & select.POLLHUP:
                # connection is closed
                time.sleep(max(timeout/1000.0, 0))
                self.okay = False
                return False
            else:
                return False
        else:
            return False

    def starttls(self):
        raise NotImplementedError('no TLS on pipes')

class TCPStream:
    """Streamed TCP/IP connection"""
    # FIXME: support everything from ProcessStream

    def __init__(self, host, port, timeout=-1):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.connect((host, port))
        self._file = self._sock.makefile('rb', bufsize=0)
        self._r_poll = select.poll()
        self._r_poll.register(self._sock.fileno(), select.POLLIN | select.POLLHUP)
        self.timeout = int(timeout)
        self._sock.settimeout(timeout)
        self.okay = True
        self.close = self._sock.close
        self.flush = self._file.flush
        self.read = self._file.read
        self.readline = self._file.readline
        self.write = self._file.write

    def starttls(self):
        raise NotImplementedError('starttls() on a pure TCPStream instance')

#        else:
#            s = self._buffer
#            self._buffer = ''
#            if len(s):
#                if size >= 2:
#                    return s + self._ssl_connection.recv(size - 1)
#                elif size == 1:
#                    return s
#                elif size == 0:
#                    return ''
#                else:
#                    return s + self._ssl_connection.recv(1048576)
#            else:
#                return self._ssl_connection.recv(size)
#######################
#       else:
#           buf = []
#           while 1:
#               #char = self._ssl_connection.recv(1)
#               char = self.read(1)
#               buf = char
#               if char == '\n':
#                   break
#           return ''.join(buf)

    def has_data(self, timeout=None):
        """Check if we can read from socket without blocking"""
        if timeout is None:
            timeout = self.timeout
        polled = self._r_poll.poll(timeout)
        if len(polled):
            result = polled[0][1]
            if result & select.POLLIN:
                if result & select.POLLHUP:
                    # closed connection, data still available
                    self.okay = False
                return True
            elif result & select.POLLHUP:
                # connection is closed
                time.sleep(max(timeout/1000.0, 0))
                self.okay = False
                return False
            else:
                return False
        else:
            return False
#        else:
#            if (self._ssl_connection.pending() > 0) or len(self._buffer):
#                return True
#            #old_timeout = self._sock.gettimeout()
#            #self._sock.settimeout(0)
#            res = True
#            try:
#                self._buffer = self._ssl_connection.recv(1)
#            except SSL.WantReadError:
#                res = False
#            #self._sock.settimeout(old_timeout)
#            return res


class GenericSSLStream(TCPStream):
    """Generic SSL Stream template"""
    def __init__(self, host, port, timeout=-1):
        TCPStream.__init__(self, host, port, timeout)
        self.ssl = False

    def close(self):
        if self.ssl:
            return self._ssl_close()
        else:
            return TCPStream.close(self)

    def flush(self):
        if self.ssl:
            return self._ssl_close()
        else:
            return TCPStream.close(self)

    def has_data(self, timeout=None):
        if self.ssl:
            return self._ssl_has_data(timeout)
        else:
            return TCPStream.has_data(self, timeout)

    def read(self, size=-1):
        if self.ssl:
            return self._ssl_read(size)
        else:
            return TCPStream.read(self, size)

    def readline(self):
        if self.ssl:
            return self._ssl_readline()
        else:
            return TCPStream.readline(self)

    def starttls(self):
        if self.ssl:
            raise NotImplementedError("can't initiate SSL/TLS twice")
        else:
            self.ssl = self._ssl_starttls()
            return self.ssl

    def write(self, data):
        if self.ssl:
            return self._ssl_write(data)
        else:
            return TCPStream.write(self, data)


class OpenSSLStream(GenericSSLStream):
    def _ssl_close(self):
        self._ssl_connection.shutdown()
        self._ssl_connection.sock_shutdown(socket.SHUT_RDWR)

    def _ssl_flush(self):
        # FIXME
        return self._sock.flush()

    def _ssl_read(self, size=-1):
        # FIXME: size handling (max & default)
        return self._ssl_connection.recv(size)

    def _ssl_readline(self):
       buf = []
       while 1:
           char = self._ssl_connection.recv(1)
           buf.append(char)
           if char == '\n':
               break
       return ''.join(buf)

    def _ssl_starttls(self):
        self._ssl_context = SSL.Context(SSL.TLSv1_METHOD)
        self._ssl_connection = SSL.Connection(self._ssl_context, self._sock)
        self._ssl_connection.set_connect_state()
        self.ssl = True
        self._buffer = ''

    def _ssl_write(self, data):
        return self._ssl_connection.sendall(data)