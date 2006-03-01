# -*- coding: utf-8
"""IMAP4rev1 client library written with aim to be as much RFC3501 compliant as possible and wise :)

References: IMAP4rev1 - RFC3501

Author: Jan Kundrát <jkt@flaska.net>
Inspired by the Python's imaplib library.
"""

__version__ = "0.1"
# $Id$

import re
if __debug__:
    import sys, time

__all__ = ["IMAPStream", "IMAPResponse", "StreamProcess", "StreamTCP"]

CRLF = "\r\n"

class StreamProcess:
    """Streamable interface to local process. Supports read(), readline(), write() and flush() methods."""

    def __init__(self, command):
        import os
        (self._w, self._r) = os.popen2(command)
        self.read = self._r.read
        self.readline = self._r.readline
        self.write = self._w.write
        self.flush = self._w.flush

class StreamTCP:
    """Streamed TCP/IP connection"""

    def __init__(self, host, port):
        import socket
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.connect((host, port))
        self._file = self._sock.makefile('rb')
        self.read = self._file.read
        self.readline = self._file.readline
        self.write = self._file.write
        self.flush = self._file.flush


class IMAPResponse:
    """Simple container to hold a response from IMAP server. Storage only, don't expect to get usable methods here :)"""
    def __init__(self):
        self.tagged = False               # was it a tagged response?
        self.kind = None                  # which "kind" of response is it? (PREAUTH, CAPABILITY, BYE, EXISTS,...)
        self.response_code = (None, None) # optional "response code" - first item is kind of message,
                                          # second either tuple of parsed items, string, number or None
        self.data = ()                    # string with human readable text or tuple with parsed items

    def __repr__(self):
        s = "<ymaplib.IMAP_response - "
        if self.tagged:
            s += "Tagged"
        else:
            s += "Untagged"
        return s + ", kind: " + self.kind.__repr__() + ', response_code: ' \
               + self.response_code.__repr__() + ", data: " + self.data.__repr__() + ">"


class IMAPStream:
    """Streamed connection to the IMAP4rev1 compliant IMAP server"""

    class NotImplementedError(Exception):
        """Something is not yet implemented"""

    class InvalidResponseError(Exception):
        """Invalid, unexpected, malformed or unparsable response. Possible reasons might be YMAPlib bug, IMAP server error or connection borkage."""
        pass

    class ParseError(InvalidResponseError):
        """Unable to parse server's response"""
        pass

    class InvalidResponseWrongTagError(InvalidResponseError):
        """Response contains unknown tag (comand pipelining is not implemented)"""
        pass

    class UnknownResponseError(InvalidResponseError):
        """Unknown response from server"""
        pass

    tag_prefix = "ym"
    tagged_response = re.compile(tag_prefix + r'\d+ ')

    # RFC3501, section 7.1 - Server Responses - Status Responses
    _resp_status = ('OK', 'NO', 'BAD', 'PREAUTH', 'BYE')
    _resp_status_tagged = ('OK', 'NO', 'BAD')
    # 7.2 - Server Responses - Server and Mailbox Status
    _resp_server_mailbox_status = ('CAPABILITY', 'LIST', 'LSUB', 'STATUS', 'SEARCH', 'FLAGS')
    # 7.3 - Server Responses - Mailbox Size
    _resp_mailbox_size = ('EXISTS', 'RECENT')
    # 7.4 - Server  Responses - Message Status
    _resp_message_status = ('EXPUNGE', 'FETCH')
    # 7.5 - Server Responses - Command Continuation Request
    # handled in _parse_response_line()

    _response_code_single = ('ALERT', 'PARSE', 'READ-ONLY', 'READ-WRITE', 'TRYCREATE')
    _response_code_number = ('UIDNEXT', 'UIDVALIDITY', 'UNSEEN')
    _response_code_spaces = ('CAPABILITY')
    _response_code_parenthesized = ('BADCHARSET', 'PERMANENTFLAGS')

    def _make_res(expr, iterable):
        """Make tuple of (name, re(expr)) tuples"""
        buf = []
        for item in iterable:
            buf.append((item, re.compile(expr % item, re.IGNORECASE)))
        return tuple(buf)

    # initialize data structures for server response parsing
    _re_resp_status_tagged = _make_res('^%s (.*)', _resp_status_tagged)
    _re_resp_status = _make_res('^%s (.*)', _resp_status)
    _re_resp_server_mailbox_status = _make_res('^%s ?(.*)',
                                      _resp_server_mailbox_status)
    _re_resp_mailbox_size = _make_res(r'^(\d+) %s', _resp_mailbox_size)
    _re_resp_message_status = _make_res(r'^(\d+) %s ?(.*)', _resp_message_status) # the ' ?(.*)' is here to allow matching of FETCH responses

    _re_response_code_single = _make_res('%s', _response_code_single)
    _re_response_code_number = _make_res('%s', _response_code_number)
    _re_response_code_spaces = _make_res('%s', _response_code_spaces)
    _re_response_code_parenthesized = _make_res('%s',
                                       _response_code_parenthesized)

    _re_literal = re.compile(r'{(\d+)}')

    def __init__(self, stream, debug = 0):
        self._stream = stream
        if __debug__:
            self.debug = debug
        else:
            self.debug = 0
        self.in_progress = 0
        self.tag_current = 0

    def _read(self, size):
        """Read size octets from server's output"""
        buf = self._stream.read(size)
        if __debug__:
            if self.debug >= 5:
                self._log('< %s' % buf)
        return buf

    def _write(self, data):
        """Write data to server"""
        if __debug__:
            if self.debug >= 5:
                self._log('> %s' % data)
        return self._stream.write(data)

    def _get_line(self):
        """Get one line of server's output. Based on the method of imaplib's IMAP4 class."""

        line = self._stream.readline()
        if not line:
            raise self.InvalidResponseError("socket error: EOF")

        # Protocol mandates all lines terminated by CRLF
        if not line.endswith(CRLF):
            raise self.InvalidResponseError("line doesn't end with CRLF")

        # Trim the trailing CRLF
        line = line[:-len(CRLF)]

        if __debug__:
            if self.debug >= 5:
                self._log('< %s' % line)
        return line

    def responses(self):
        """Parse the server's responses. Expects zero or more untagged replies
        and one tagged reply. Returns a list of IMAP_response objects."""
        responses = []

        while 1:
            line = self._get_line()
            response = self._parse_line(line, self._make_tag())
            responses.append(response)
            if response.tagged:
                # should be the last item, so stop iterating
                break
        self.in_progress -= 1
        return responses


    def _parse_line(self, line, current_tag):
        """Parse one line of the response to the IMAP_response object."""
        response = IMAPResponse()

        if line.startswith('* '):
            # Untagged response
            response.tagged = False
            line = line[2:]
        elif line.startswith('+ '):
            # Command Continuation Request
            # FIXME :)
            raise self.NotImplementedError(line)
        elif self.tagged_response.match(line):
            # Tagged response
            if not line.startswith(current_tag + " "):
                # wrong tag
                raise self.InvalidResponseWrongTagError(current_tag, line)
            response.tagged = True
            line = line[len(current_tag) + 1:]
        else:
            # Unparsable response
            raise self.ParseError(line)

        if response.tagged:
            test = self._re_resp_status_tagged
        else:
            test = self._re_resp_status
        response.kind = self._helper_foreach(line, test)[0]

        if response.kind is not None:
            # we should check for optional Response Code
            line = line[len(response.kind) + 1:]
            if line.startswith('['):
                # parse the Response Code
                try:
                    last = line.index(']')
                    try:
                        # do we have to deal with response code with arguments?
                        space = line[1:last].index(' ')
                        code = line[1:space+1]
                        arguments = self._parse_response_code(code, line[space+2:last])
                    except ValueError:
                        # just an "[atom]"
                        code = line[1:last]
                        arguments = None
                    response.response_code = (code, arguments)
                    line = line[last + 2:]
                except ValueError:
                    # line contains "[" but no "]"
                    raise self.ParseError(line)
            # the rest of the line should be only a string
            response.data = line
        elif not response.tagged:
            for test in (self._re_resp_server_mailbox_status, self._re_resp_mailbox_size, self._re_resp_message_status):
                (response.kind, r) = self._helper_foreach(line, test)
                if response.kind == 'FETCH':
                    # FETCH response will have two items as the result
                    try:
                        response.data = (r.groups()[0], r.groups()[1])
                    except IndexError:
                        raise self.ParseError(line)
                elif response.kind is not None:
                    # we've matched against some command
                    response.data = r.groups()[0]
                    break

        if response.kind is None:
            # response kind wasn't detected so far
            raise self.UnknownResponseError(line)

        # now it's time to convert textual data in response.data to something better
        return self._parse_response_data(response)


    def _helper_foreach(self, item, iterable):
        """Helper function - if line matches iterable[x][1], returns (iterable[x][0], r.match(item))"""
        for name, r in iterable:
            foo = r.match(item)
            if foo:
                return (name, foo)
        return (None, None)


    def _parse_response_code(self, code, line):
        """Parse optional (sect 7.1) response codes"""

        if self._helper_foreach(code, self._re_response_code_single)[0] is not None:
            # "[atom]"
            return None
        elif self._helper_foreach(code, self._re_response_code_number)[0] is not None:
            # "[atom number]"
            return int(line)
        elif self._helper_foreach(code, self._re_response_code_parenthesized)[0] is not None:
            # "[atom (foo bar)]"
            if not line.startswith('(') or not line.endswith(')'):
                raise self.ParseError(line)
            buf = line[1:-1].split(' ')
            if buf == ['']:
                return ()
            else:
                return tuple(line[1:-1].split(' '))
        elif self._helper_foreach(code, self._re_response_code_spaces)[0] is not None:
            # "[atom foo bar]"
            return tuple(line.split(' '))
        else:
            # unknown; RFC recommends ignoring
            if self.debug > 1:
                self._log("! unknown Response Code: '%s'" % code)
            return line

    def _parse_response_data(self, response):
        """Parse response.data string into proper form"""
        if response.tagged:
            if response.kind in self._resp_status_tagged:
                # RFC specifies the rest of the line to be "human readable text" so we don't have much to do here :)
                pass
            else:
                raise self.UnknownResponseError(response)
        else:
            if response.kind in self._resp_status:
                # RFC specifies the rest of the line to be "human readable text" so we don't have much to do here :)
                pass
            elif response.kind in self._resp_mailbox_size or response.kind == 'EXPUNGE':
                # "* number FOO"
                response.data = int(response.data)
            elif response.kind == 'CAPABILITY':
                response.data = tuple(response.data.split(' '))
            elif response.kind == 'LIST' or response.kind == 'LSUB':
                # [name_attributes, hierarchy_delimiter, name]
                try:
                    pos1 = response.data.index('(')
                    pos2 = response.data.index(')')
                    buf = [tuple(response.data[pos1 + 1:pos2].split(' '))]
                except ValueError:
                    raise self.ParseError(response)
                line = response.data[pos2 + 2:]
                (s, line) = self._extract_astring(line)
                buf.append(s)
                (s, line) = self._extract_string(line)
                buf.append(s)
                response.data = tuple(buf)
            elif response.kind == 'STATUS':
                (s, line) = self._extract_astring(response.data)
                response.data = [s]
                if not line.startswith('(') or not line.endswith(')'):
                    raise self.ParseError(line)
                items = line[1:-1].split(' ')
                buf = {}
                last = None
                for item in items:
                    if item == '':
                        break
                    if last is None:
                        buf[item] = None
                        last = item
                    else:
                        try:
                            buf[last] = int(item)
                        except ValueError:
                            raise self.ParseError(response)
                        last = None
                response.data.append(buf)
                response.data = tuple(response.data)
            elif response.kind == 'SEARCH':
                items = response.data.split(' ')
                if items == ['']:
                    response.data = ()
                else:
                    try:
                        #items = map(lambda x: int(x), items)
                        items = map(int, items)
                    except ValueError:
                        raise self.ParseError(response)
                    response.data = tuple(items)
            elif response.kind == 'FLAGS':
                if not response.data.startswith('(') or not response.data.endswith(')'):
                    raise self.ParseError(line)
                items = response.data[1:-1].split(' ')
                response.data = tuple(items)
            elif response.kind == 'FETCH':
                # "* number FETCH (data...)"
                try:
                    msgno = int(response.data[0])
                except ValueError:
                    raise self.ParseError(response)
                line = response.data[1]
                if not line.startswith('('):
                    # response isn't enclosed in parentheses
                    line = response.data[1] + ')'
                    if self.debug > 2:
                        self._log('adding parenthesis to %s' % response)
                else:
                    line = line[1:]
                response.data = (msgno, self._parse_parenthesized_line(line)[0])
                # FIXME: add handling of response fields
            else:
                raise self.NotImplementedError(response)
        return response

    def _parse_parenthesized_line(self, line):
        """Parse parenthesized line into Python data structure"""
        buf = []
        limit = 0
        while limit < 1024:
            # safety limit for 1024 items on the same level at most
            limit += 1
            (s, line) = self._extract_string(line)
            if s == '(':
                # nested block
                (item, line) = self._parse_parenthesized_line(line)
                #print 'nested returned %s' % item
                buf.append(item)
            elif s == ')':
                # end of nested block
                break
            elif line == '':
                # end of string
                break
            else:
                buf.append(s)
        return (tuple(buf), line)

    def _extract_string(self, string):
        """Extract string, including checks for literals"""
        r = self._re_literal.match(string)
        if r:
            string = ''
            size = int(r.groups()[0])
            if self.debug >= 6:
                self._log('got literal - %d octets' % size)
            buf = self._read(size)
            string = self._get_line()
            return (buf, string)
        else:
            return self._extract_astring(string)

    def _extract_astring(self, string):
        """Extract an astring from string. Astring can't be literal."""
        string = string.lstrip(' ')
        if string.startswith('"'):
            # quoted string
            try:
                pos = string.index('"', 1)
                buf = string[1:pos]
                string = string[pos + 1:]
            except ValueError:
                raise self.ParseError(string)
        elif string.startswith('(') or string.startswith(')'):
            # "(" or ")"
            buf = string[0]
            string = string[1:]
        else:
            # atom
            pos_par = string.find(')')
            pos_space = string.find(' ')
            if pos_par == -1 and pos_space == -1:
                # atom
                buf = string
                string = ''
            elif pos_par == -1:
                # no ")", but space found
                buf = string[:pos_space]
                string = string[pos_space + 1:]
            elif pos_space == -1:
                # no space, but ")" found
                buf = string[:pos_par]
                string = string[pos_par + 1:]
            else:
                # both space and ")"
                pos = min(pos_par, pos_space)
                buf = string[:pos]
                string = string[pos:]
        string = string.lstrip(' ')
        return (buf, string)


    def send_command(self, command):
        """Sends a raw command, wrapping it with apropriate tag"""
        self.in_progress += 1
        self.tag_current += 1
        self._write(self._make_tag() + ' ' + command + CRLF)
        self._stream.flush()


    def _make_tag(self):
        """Create a string tag"""
        return self.tag_prefix + str(self.tag_current)


    if __debug__:
        def _log(self, s):
            """Internal logging function, "inspired" by imaplib"""
            secs = time.time()
            tm = time.strftime('%M:%S', time.localtime(secs))
            sys.stderr.write('  %s.%02d %s\n' % (tm, (secs * 100) % 100, s))
            sys.stderr.flush()



if __name__ == "__main__":
    import sys
    print "ymaplib version " + __version__
    if len(sys.argv) != 2:
        print "Usage: %s path/to/imapd" % sys.argv[0]
        sys.exit()
    stream = ProcessStream([sys.argv[1]])
    c = IMAPStream(stream, 5)
    """debug = 10; import ymaplib; y=ymaplib.IMAPStream(ymaplib.StreamProcess('dovecot --exec-mail imap'), debug); y._parse_line(y._get_line(), None); y.send_command('capability'); y.responses(); y.send_command('select gentoo.gentoo-user-cs'); y.responses(); y.send_command('fetch 1 full'); y.responses(); y.send_command('status inbox ()'); y.responses()"""
