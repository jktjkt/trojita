# -*- coding: utf-8
"""IMAP4rev1 client library

Written with aim to be as much RFC3501 compliant as possible and wise :)

References: IMAP4rev1 - RFC3501

Author: Jan Kundrát <jkt@flaska.net>
Inspired by the Python's imaplib library.
"""

from __future__ import generators
import re
import threading
import Queue
if __debug__:
    import sys, time

__version__ = "0.1"
__revision__ = '$Id$'
__all__ = ["IMAPParser", "IMAPResponse", "IMAPNIL", "IMAPThreadItem",
           "ProcessStream", "TCPStream"]

CRLF = "\r\n"

class ProcessStream:
    """Streamable interface to local process.

Supports read(), readline(), write(), flush(), and has_data() methods. Doesn't
work on Win32 systems due to their lack of poll() functionality on pipes.
"""

    def __init__(self, command, timeout=-1):
        import os, select
        # disable buffering, otherwise readline() might read more than just
        # one line and following poll() would say that there's nothing to read
        (self._w, self._r) = os.popen2(command, bufsize=0)
        self.read = self._r.read
        self.readline = self._r.readline
        self.write = self._w.write
        self.flush = self._w.flush
        self._r_poll = select.poll()
        self._r_poll.register(self._r.fileno(), select.POLLIN)
        self.timeout = int(timeout)

    def has_data(self, timeout=None):
        """Check if we can read from socket without blocking"""
        if timeout is None:
            timeout = self.timeout
        return bool(len(self._r_poll.poll(timeout)))


class TCPStream:
    """Streamed TCP/IP connection"""

    def __init__(self, host, port, timeout=-1):
        import socket, select
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.connect((host, port))
        self._file = self._sock.makefile('rb', bufsize=0)
        self._r_poll = select.poll()
        self._r_poll.register(self._sock.fileno(), select.POLLIN)
        self.read = self._file.read
        self.readline = self._file.readline
        self.write = self._file.write
        self.flush = self._file.flush
        self.timeout = int(timeout)

    def has_data(self, timeout=None):
        """Check if we can read from the socket without blocking.

        Needs further testing.
        """
        if timeout is None:
            timeout = self.timeout
        return bool(len(self._r_poll.poll(timeout)))


class IMAPResponse:
    """Simple container to hold a response from IMAP server.

Storage only, don't expect to get usable methods here :)
"""
    def __init__(self):
        self.tag = False
        # response tag or None if untagged
        self.kind = None
        # which "kind" of response is it? (PREAUTH, CAPABILITY, BYE, EXISTS,...)
        self.response_code = (None, None)
        # optional "response code" - first item is kind of message,
        # second either tuple of parsed items, string, number or None
        self.data = ()
        # string with human readable text or tuple with parsed items

    def __repr__(self):
        s = "<ymaplib.IMAPResponse - "
        if self.tag is None:
            s += "untagged"
        else:
            s += "tag %s" % self.tag
        return s + ", kind: " + str(self.kind) + ', response_code: ' + \
               str(self.response_code) + ", data: " + str(self.data) + ">"

class IMAPNIL:
    """Simple class to hold the NIL token"""
    def __repr__(self):
        return '<ymaplib.IMAPNIL>'

class IMAPThreadItem:
    """One message in the threaded mailbox"""
    def __init__(self):
        self.id = None
        self.children = None

    def __repr__(self):
        return "<ymaplib.IMAPThreadItem %s: %s>" % (self.id, self.children)

class NotImplementedError(Exception):
    """Something is not yet implemented"""
    pass

class InvalidResponseError(Exception):
    """Invalid, unexpected, malformed or unparsable response.

    Possible reasons might be YMAPlib bug, IMAP server error or connection 
    borkage.
    """
    pass

class ParseError(InvalidResponseError):
    """Unable to parse server's response"""
    pass

class UnknownResponseError(InvalidResponseError):
    """Unknown response from server"""
    pass

class TimeoutError:
    """Socket timed out"""
    pass

class IMAPParser:
    """Streamed connection to the IMAP4rev1 compliant IMAP server"""
   
    class ResponsesStreamThread(threading.Thread):
        """Thread for handling responses from server"""
        
        def __init__(self, parser):
            threading.Thread.__init__(self)
            self.setDaemon(True)
            self.parser = parser
            
        def run(self):
            safety_limit = 1024
            # FIXME: hard-coded limit
            # self.parser.stream_lock will be unlocked at least every $ iterations
            while self.parser._stream:
                counter = 0
                self.parser.stream_lock.acquire()
                while self.parser._stream.has_data(0):
                    # we don't want to wait too long
                    response = self.parser._parse_line(self.parser._get_line())
                
                    if response.tag is not None:
                        self.parser.tagged_responses_lock.acquire()
                        self.parser.tagged_responses[response.tag] = response
                        self.parser.tagged_responses_lock.release()
                    else:
                        self.parser.untagged_responses.put(response)
                        #self.parser.untagged_responses_lock.acquire()
                        #self.parser.untagged_responses.append(response)
                        #self.parser.untagged_responses_lock.release()
                    # FIXME: do something with the response - enable some event?
                    counter += 1
                    if counter >= safety_limit:
                        # we've processed a lot of responses, so let's give
                        # other threads a chance to work
                        break
                self.parser.stream_lock.release()
                time.sleep(1)
                
    _tag_prefix = "ym"
    _re_tagged_response = re.compile(_tag_prefix + r'\d+ ')
    _re_nil = re.compile("^NIL", re.IGNORECASE)

    # RFC3501, section 7.1 - Server Responses - Status Responses
    _resp_status = ('OK', 'NO', 'BAD', 'PREAUTH', 'BYE')
    _resp_status_tagged = ('OK', 'NO', 'BAD')
    # 7.2 - Server Responses - Server and Mailbox Status
    _resp_server_mailbox_status = ('CAPABILITY', 'LIST', 'LSUB', 'STATUS', 
                                   'SEARCH', 'FLAGS')
    # 7.3 - Server Responses - Mailbox Size
    _resp_mailbox_size = ('EXISTS', 'RECENT')
    # 7.4 - Server  Responses - Message Status
    _resp_message_status = ('EXPUNGE', 'FETCH')
    # 7.5 - Server Responses - Command Continuation Request
    # handled in _parse_response_line()
    # draft-ietf-imapext-sort-17:
    _resp_imapext_sort = ('SORT', 'THREAD')

    _response_code_single = ('ALERT', 'PARSE', 'READ-ONLY', 'READ-WRITE', 
                             'TRYCREATE')
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
    _re_resp_message_status = _make_res(r'^(\d+) %s ?(.*)', _resp_message_status)
    # the ' ?(.*)' is here to allow matching of FETCH responses
    _re_resp_imapext_sort = _make_res('^%s ?(.*)', _resp_imapext_sort)

    _re_response_code_single = _make_res('%s', _response_code_single)
    _re_response_code_number = _make_res('%s', _response_code_number)
    _re_response_code_spaces = _make_res('%s', _response_code_spaces)
    _re_response_code_parenthesized = _make_res('%s',
                                       _response_code_parenthesized)

    _re_literal = re.compile(r'{(\d+)}')

    def __init__(self, stream, debug=0):
        self._stream = stream
        if __debug__:
            self.debug = debug
        else:
            self.debug = 0
        self.last_tag_num = 0
        self.untagged_responses = Queue.Queue()
        self.tagged_responses = {}
        
        self.stream_lock = threading.Lock()
        self.tagged_responses_lock = threading.Lock()
        
        self.responses_stream_thread = self.ResponsesStreamThread(self)
        self.responses_stream_thread.start()

    def _read(self, size):
        """Read size octets from server's output"""

        if not self._stream.has_data():
            raise TimeoutError
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
        """Get one line of server's output.

Based on the method of imaplib's IMAP4 class.
"""

        if not self._stream.has_data():
            raise TimeoutError
        line = self._stream.readline()
        if not line:
            raise InvalidResponseError("socket error: EOF")

        # Protocol mandates all lines terminated by CRLF
        if not line.endswith(CRLF):
            raise InvalidResponseError("line doesn't end with CRLF")

        # Trim the trailing CRLF
        line = line[:-len(CRLF)]

        if __debug__:
            if self.debug >= 5:
                self._log('< %s' % line)
        return line

    def _parse_line(self, line):
        """Parse one line of the response to the IMAP_response object."""
        response = IMAPResponse()

        if line.startswith('* '):
            # Untagged response
            response.tag = None
            line = line[2:]
        elif line.startswith('+ '):
            # Command Continuation Request
            # FIXME: :)
            raise NotImplementedError(line)
        elif self._re_tagged_response.match(line):
            # Tagged response
            try:
                pos = line.index(' ')
                response.tag = line[:pos]
                line = line[pos + 1:]
            except ValueError:
                raise ParseError(line)
            if not response.tag in self.tagged_responses:
                raise InvalidResponseError(line)
        else:
            # Unparsable response
            raise ParseError(line)

        if response.tag is not None:
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
                        arguments = self._parse_response_code(code,
                                     line[space+2:last])
                    except ValueError:
                        # just an "[atom]"
                        code = line[1:last]
                        arguments = None
                    response.response_code = (code, arguments)
                    line = line[last + 2:]
                except ValueError:
                    # line contains "[" but no "]"
                    raise ParseError(line)
            # the rest of the line should be only a string
            response.data = line
        elif response.tag is None:
            for test in (self._re_resp_server_mailbox_status,
              self._re_resp_mailbox_size, self._re_resp_message_status,
              self._re_resp_imapext_sort):
                (response.kind, r) = self._helper_foreach(line, test)
                if response.kind == 'FETCH':
                    # FETCH response will have two items as the result
                    try:
                        response.data = (r.groups()[0], r.groups()[1])
                    except IndexError:
                        raise ParseError(line)
                    break
                elif response.kind is not None:
                    # we've matched against some command
                    response.data = r.groups()[0]
                    break

        if response.kind is None:
            # response kind wasn't detected so far
            raise UnknownResponseError(line)
        return self._parse_response_data(response)

    @classmethod
    def _helper_foreach(cls, item, iterable):
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
                raise ParseError(line)
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
        # this one *can't* be classmethod as we might need to read a literal
        if response.tag is not None:
            if response.kind not in self._resp_status_tagged:
                raise UnknownResponseError(response)
            # RFC specifies the rest of the line to be "human readable text"
            # so we don't have much to do here :)
        else:
            if response.kind in self._resp_status:
                # human-readable text follows
                pass
            elif response.kind in self._resp_mailbox_size or \
               response.kind == 'EXPUNGE':
                # "* number FOO"
                response.data = int(response.data)
            elif response.kind == 'CAPABILITY' or response.kind == 'SORT':
                response.data = tuple(response.data.split(' '))
            elif response.kind == 'LIST' or response.kind == 'LSUB':
                # [name_attributes, hierarchy_delimiter, name]
                try:
                    pos1 = response.data.index('(')
                    pos2 = response.data.index(')')
                    buf = [tuple(response.data[pos1 + 1:pos2].split(' '))]
                except ValueError:
                    raise ParseError(response)
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
                    raise ParseError(line)
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
                            raise ParseError(response)
                        last = None
                response.data.append(buf)
                response.data = tuple(response.data)
            elif response.kind == 'SEARCH':
                items = response.data.split(' ')
                if items == ['']:
                    response.data = ()
                else:
                    try:
                        items = [int(item) for item in items]
                    except ValueError:
                        raise ParseError(response)
                    response.data = tuple(items)
            elif response.kind == 'FLAGS':
                if not response.data.startswith('(') \
                  or not response.data.endswith(')'):
                    raise ParseError(line)
                items = response.data[1:-1].split(' ')
                response.data = tuple(items)
            elif response.kind == 'FETCH':
                # "* number FETCH (data...)"
                try:
                    msgno = int(response.data[0])
                except ValueError:
                    raise ParseError(response)
                line = response.data[1]
                if not line.startswith('('):
                    # response isn't enclosed in parentheses
                    line = response.data[1] + ')'
                    if self.debug > 2:
                        self._log('adding parenthesis to %s' % response)
                else:
                    line = line[1:]
                response.data = (msgno, self._parse_parenthesized_line(line)[0])
            elif response.kind == 'THREAD':
                # "* THREAD data"
                response.data = self._parse_thread_response(response.data)
            else:
                raise NotImplementedError(response)
        return response

    def _parse_parenthesized_line(self, line):
        """Parse parenthesized line into Python data structure"""
        buf = []
        limit = 0
        while limit < 1024:
            # safety limit for 1024 items on the same level at most
            # FIXME: hard-coded constant (should be raised, btw, imho)
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

    def _parse_thread_response(self, line):
        """Parse draft-ietf-imapext-sort-17 THREAD respone into Python data structure"""
        parent = IMAPThreadItem()
        parent.children = []
        last_token = ' '
        stack = []
        item = IMAPThreadItem()
        root = parent
        for s in self._extract_thread_response(line):
            try:
                if (last_token == ' ' or last_token == '(' or last_token == ')') \
                   and s == ' ':
                    # ignore more spaces...
                    continue
                if s.isdigit():
                    # one level deeper
                    item = IMAPThreadItem()
                    item.id = s
                    item.children = []
                    parent.children.append(item)
                    parent = item
                elif s == ' ':
                    # ignore separator
                    pass
                elif s == '(':
                    # last item have multiple children,
                    # we will have to save a position
                    stack.append(parent)
                elif s == ')':
                    # end of subthread
                    if parent.children == []:
                        parent.children = None
                    else:
                        parent.children = parent.children
                    parent = stack.pop()
                else:
                    raise ParseError(line)
            except IndexError:
                # wrong combination of parentheses
                raise ParseError(line)
            last_token = s
        return root

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

    @classmethod
    def _extract_astring(cls, string):
        """Extract an astring from string. Astring can't be literal."""
        string = string.lstrip(' ')
        if string.startswith('"'):
            # quoted string, we must handle escaping
            # FIXME: we should use something more efficient
            escaping = False
            pos = 1 # first character is '"' so we can safely skip it
            go_on = True
            buf = ''
            size = len(string)
            while go_on and pos < size:
                if escaping:
                    if string[pos] == '\\' or string[pos] == '"':
                        buf += string[pos]
                    else:
                        # escaping an unknown character
                        # RFC 3501 doesn't specify what to do here, but such data
                        # aren't formatted as specified by the ABNF syntax at the
                        # end of the RFC 
                        buf += '\\' + string[pos]
                        escaping = False
                        # FIXME: need a mechanism to report non-fatal errors
                        #raise ParseError(string)
                    escaping = False
                elif string[pos] == '"':
                    go_on = False
                elif string[pos] == '\\':
                    escaping = True
                else:
                    buf += string[pos]
                pos += 1
            if go_on:
                # unterminated quoted string
                raise ParseError(string)
            else:
                string = string[pos:]
        elif string.startswith('(') or string.startswith(')'):
            # "(" or ")"
            buf = string[0]
            string = string[1:]
        elif cls._re_nil.match(string):
            # the 'NIL' token
            buf = IMAPNIL()
            string = string[len('NIL'):]
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

    @classmethod
    def _extract_thread_response(cls, s):
        """Tokenize the THREAD response into parentheses and spaces (helper function)"""
        while s != '':
            if s.startswith(' ') or s.startswith('(') or s.startswith(')'):
                yield s[0]
                s = s[1:]
            else:
                buf = ''
                while s != '' and not \
                  (s.startswith(' ') or s.startswith('(') or s.startswith(')')):
                    buf += s[0]
                    s = s[1:]
                yield buf

    def send_command(self, command):
        """Sends a raw command, wrapping it with apropriate tag"""
        self.tagged_responses_lock.acquire()
        self.last_tag_num += 1
        tag_name = self._make_tag()
        self.tagged_responses[tag_name] = None
        self.tagged_responses_lock.release()
        self.stream_lock.acquire()
        self._write(tag_name + ' ' + command + CRLF)
        self._stream.flush()
        self.stream_lock.release()
        return tag_name

    def get_tagged(self, tag):
        """Returns tagged response associated with given tag *and* extracts it from buffer"""
        response = None
        self.tagged_responses_lock.acquire()
        try:
            try:
                response = self.tagged_responses[tag]
                del self.tagged_responses[tag]
            except KeyError:
                pass
        finally:
            self.tagged_responses_lock.release()
            return response

    def get_untagged_responses(self):
        """[DEPRECATED???] Returns all buffered untagged responses"""
        buf = []
        while not self.untagged_responses.empty():
            buf.append(self.untagged_responses.get())
        #self.untagged_responses_lock.acquire()
        #buf = self.untagged_responses
        #self.untagged_responses = []
        #self.untagged_responses_lock.release()
        return buf

    def get_available_tags(self):
        """Return a list of tags which are either completed or in progress"""
        self.tagged_responses_lock.acquire()
        buf = self.tagged_responses.keys()
        self.tagged_responses_lock.release()
        return buf

    def _make_tag(self):
        """Create a string tag"""
        return self._tag_prefix + str(self.last_tag_num)

    if __debug__:
        def _log(self, s):
            """Internal logging function, "inspired" by imaplib"""
            secs = time.time()
            tm = time.strftime('%M:%S', time.localtime(secs))
            sys.stderr.write('  %s.%02d %s\n' % (tm, (secs * 100) % 100, s))
            sys.stderr.flush()


class IMAPEnvelope:
    """Container for RFC822 envelope"""
    pass


class IMAPMessage:
    """RFC822 message stored on an IMAP server"""
    pass

class IMAPMailbox:
    """Interface to an IMAP mailbox"""
    pass


if __name__ == "__main__":
    print "ymaplib version %s (SVN %s)" % (__version__, __revision__)
