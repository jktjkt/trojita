# -*- coding: utf-8
"""Streamed connection to the IMAP4rev1 compliant IMAP server"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

import sys
import re
import threading
import Queue
import time
import traceback
import imap4utf7
import email.Utils
import base64
from exceptions import *
from IMAPResponse import IMAPResponse
from IMAPNIL import IMAPNIL
from IMAPThreadItem import IMAPThreadItem
from IMAPEnvelope import IMAPEnvelope

__revision__ = '$Id$'

# FIXME: MULTIAPPEND, ID, UIDPLUS, NAMESPACE, QUOTA

CRLF = "\r\n"



class _WorkerThread(threading.Thread):
    """A wrapper running IMAPParser.loop() with some exception handling"""
    def __init__(self, parser):
        threading.Thread.__init__(self)
        self.setDaemon(True)
        self.parser = parser

    def run(self):
        """Periodically run the IMAPParser.loop(), check for exceptions"""
        try:
            while self.parser._worker_flag.isSet():
                self.parser._loop()
        except:
            self.parser.worker_exceptions.put(sys.exc_info())
            self.parser._worker_flag.clear()

class IMAPParser:
    """Streamed connection to the IMAP4rev1 compliant IMAP server"""


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
    _response_code_spaces = ('CAPABILITY',)
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
    _re_resp_message_status = _make_res(r'^(\d+) %s ?(.*)',
                                         _resp_message_status)
    # the ' ?(.*)' is here to allow matching of FETCH responses
    _re_resp_imapext_sort = _make_res('^%s ?(.*)', _resp_imapext_sort)

    _re_response_code_single = _make_res('%s', _response_code_single)
    _re_response_code_number = _make_res('%s', _response_code_number)
    _re_response_code_spaces = _make_res('%s', _response_code_spaces)
    _re_response_code_parenthesized = _make_res('%s',
                                       _response_code_parenthesized)

    _re_literal = re.compile(r'{(\d+)}')

    def __init__(self, stream=None, debug=0, capabilities_mask=()):
        self._stream = stream
        if __debug__:
            self.debug = debug
        else:
            self.debug = 0
        self.last_tag_num = 0

        self._incoming = Queue.Queue()
        self._outgoing = Queue.Queue()
        self.worker_exceptions = Queue.Queue()
        self._worker = None
        self._worker_flag = threading.Event()

        # does the server support LITERAL+ extension?
        self.literal_plus = False
        self.capabilities = ()
        self.capabilities_mask = capabilities_mask

        self.okay = None
        self._in_idle = False

    def start_worker(self, incoming=None):
        """Create and start a thread doing all the work"""
        if self._worker_flag.isSet():
            # worker is already running...
            return False

        if isinstance(incoming, tuple) or isinstance(incoming, list):
            for item in incoming:
                self._incoming.put(item)
        elif incoming is not None:
            raise TypeError('incoming is not list or tuple', incoming)

        self._worker = _WorkerThread(self)
        self._worker_flag.set()
        self._worker.start()
        return True

    def stop_worker(self):
        """Stop the worker thread, return pending items etc"""
        self._worker_flag.clear()
        self._worker.join()
        # we should be single-threaded now
        incoming = []
        while not self._incoming.empty():
            incoming.append(self._incoming.get())
        return tuple(incoming)

    def _check_worker_exceptions(self):
        """Check if there was an exception in the worker thread"""
        # FIXME: what action to make? Raise an exception or what?
        if not self.worker_exceptions.empty():
            self.okay = False
            exc = self.worker_exceptions.get()
            print 'Exception in %s:' % str(self._worker)
            traceback.print_exception(*exc)
            raise exc[1]

    def _queue_cmd(self, command):
        """Add a command to the queue"""
        if self.okay == False:
            raise DisconnectedError
        self._check_worker_exceptions()
        self.last_tag_num += 1
        tag_name = self._make_tag()
        self._incoming.put((tag_name, command))
        return tag_name

    def _loop_from_server(self):
        """Helper processing server responses, internal use only"""
        # some response to read
        response = self._parse_line(self._get_line())

        if response.kind == 'BYE' or not self._stream.okay:
            self.okay = False
        elif self.okay is None:
            self.okay = True

        if response.kind == 'CAPABILITY':
            source = response.data
            do_caps = True
        elif response.response_code[0] == 'CAPABILITY':
            source = response.response_code[1]
            do_caps = True
        else:
            do_caps = False
        if do_caps:
            self.capabilities = tuple([item for item in source if item not in self.capabilities_mask])
            self.literal_plus = 'LITERAL+' in self.capabilities

        # TODO: state changes (SELECT/EXAMINE ok/failed,...)
        self._outgoing.put(response)
        return response

    def _loop(self):
        """Main loop - parse responses from server, send commands,..."""
        if self._stream.has_data(0.050):
            self._loop_from_server()
        if not self._incoming.empty():
            # let's check if the connectin is still ok
            self._stream.has_data(0)
            if not self._stream.okay:
                self.okay = False
            if self.okay == False:
                raise DisconnectedError
            # there's a command in the queue, let's process it

            if self._in_idle:
                # we have to terminate the idling at first
                self._write('DONE')
                self._write(CRLF)
                self._stream.flush()
                self._in_idle = False

            (tag_name, command) = self._incoming.get()
            self._write(tag_name)
            send_CRLF = True

            if command[0].upper() == 'IDLE':
                self._write(' IDLE' + CRLF)
                self._stream.flush()
                try:
                    while 1:
                        # wait for the continuation request
                        # or a notification that it never arrives :)
                        response = self._loop_from_server()
                        if response.tag == tag_name:
                            # server doesn't want us to continue
                            break
                except CommandContinuationRequest:
                    # a little abuse of exceptions :)
                    self._in_idle = True
                # don't process this command anymore
                return
            elif command[0].upper() == 'STARTTLS':
                self._write(' STARTTLS' + CRLF)
                self._stream.flush()
                while 1:
                    # wait for confirmation
                    response = self._loop_from_server()
                    if response.tag == tag_name:
                        break
                if response.kind == 'OK':
                    return self._stream.starttls()
                else:
                    return False
            elif command[0].upper() == 'AUTHENTICATE':
                authenticator = command[1]
                if authenticator is None:
                    # fake authenticator
                    self._write(' NOOP' + CRLF)
                    self._stream.flush()
                    return

                self._write(' AUTHENTICATE ' + authenticator.mechanism + CRLF)
                self._stream.flush()
                while 1:
                    try:
                        response = self._loop_from_server()
                        if response.tag == tag_name:
                            # command finished
                            break
                    except CommandContinuationRequest, exc:
                        stuff = authenticator.chat(base64.b64decode(exc.args[0]))
                        if stuff is None:
                            self._write('*' + CRLF)
                            self._stream.flush()
                        else:
                            self._write(base64.b64encode(stuff) + CRLF)
                            self._stream.flush()
                return

            for item in command:
                if isinstance(item, str):
                    self._write(' ' + item)
                elif isinstance(item, tuple):
                    # guess the best way to encode it
                    if not len(item[0]):
                        # empty string
                        self._write(' ""')
                    elif item[0].isalnum():
                        # atomable
                        self._write(' ' + item[0])
                    elif item[0].find("\n") == -1 and item[0].find("\r") == -1:
                        # quotable
                        self._write(' "' +
                            item[0].replace('\\', '\\\\').replace('"', '\\"') +
                            '"')
                    else:
                        # literal
                        if self.literal_plus:
                            self._write((' {%d+}' % len(item[0])) + CRLF +
                                        item[0])
                        else:
                            self._write((' {%d}' % len(item[0])) + CRLF)
                            should_continue = True
                            try:
                                while 1:
                                    # wait for the continuation request
                                    # or a notification that it never arrives :)
                                    response = self._loop_from_server()
                                    if response.tag == tag_name:
                                        # server doesn't want us to continue
                                        should_continue = False
                                        send_CRLF = False
                                        break
                            except CommandContinuationRequest:
                                # a little abuse of exceptions :)
                                self._write(item[0])
                            if not should_continue:
                                # we don't have to send the rest of the command
                                break
            if send_CRLF:
                self._write(CRLF)
            self._stream.flush()

    def get(self, timeout=None):
        """Return a server reply"""
        self._check_worker_exceptions()
        if timeout == 0:
            # non-blocking invocation, might raise an exception
            return self._outgoing.get(False)
        elif timeout is None:
            # block as long as needed
            return self._outgoing.get(True)
        else:
            # block with timeout
            return self._outgoing.get(True, timeout)

    def has_responses(self):
        """Return True if we have parsed responses"""
        return not self._outgoing.empty()

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
                self._log("> '%s'" % data)
        return self._stream.write(data)

    def cmd_capability(self):
        """Send a CAPABILITY command"""
        return self._queue_cmd(('CAPABILITY',))

    def cmd_noop(self):
        """Send a NOOP command"""
        return self._queue_cmd(('NOOP',))

    def cmd_logout(self):
        """Send a LOGOUT command"""
        # we don't adjust self.okay here as the LOGOUT command
        # might actually fail
        return self._queue_cmd(('LOGOUT',))

    def cmd_starttls(self):
        """Perform a TLS negotiation"""
        return self._queue_cmd(('STARTTLS',))

    def cmd_authenticate(self, authenticator):
        """Authenticate to the server"""
        return self._queue_cmd(('AUTHENTICATE', authenticator))

    def cmd_login(self, username, password):
        """Login with supplied username and password"""
        return self._queue_cmd(('LOGIN', (username,), (password,)))

    def cmd_select(self, mailbox):
        """Select a mailbox"""
        return self._queue_cmd(('SELECT', (mailbox.encode('imap4-utf-7'),)))

    def cmd_examine(self, mailbox):
        """Examine a mailbox"""
        return self._queue_cmd(('EXAMINE', (mailbox.encode('imap4-utf-7'),)))

    def cmd_create(self, mailbox):
        """Create a mailbox"""
        return self._queue_cmd(('CREATE', (mailbox.encode('imap4-utf-7'),)))

    def cmd_delete(self, mailbox):
        """Delete a mailbox"""
        return self._queue_cmd(('DELETE', (mailbox.encode('imap4-utf-7'),)))

    def cmd_rename(self, old_name, new_name):
        """Rename a mailbox"""
        return self._queue_cmd(('RENAME', (old_name.encode('imap4-utf-7'),),
                         (new_name.encode('imap4-utf-7'),)))

    def cmd_subscribe(self, mailbox):
        """Subscribe a mailbox"""
        return self._queue_cmd(('SUBSCRIBE', (mailbox.encode('imap4-utf-7'),)))

    def cmd_unsubscribe(self, mailbox):
        """Unsubscribe a mailbox"""
        return self._queue_cmd(('UNSUBSCRIBE', (mailbox.encode('imap4-utf-7'),)))

    def cmd_list(self, reference, name):
        """Send a LIST command"""
        return self._queue_cmd(('LIST', (reference.encode('imap4-utf-7'),),
                         (name.encode('imap4-utf-7'),)))

    def cmd_lsub(self, reference, name):
        """Send a LSUB command"""
        return self._queue_cmd(('LSUB', (reference.encode('imap4-utf-7'),),
                         (name.encode('imap4-utf-7'),)))

    def cmd_status(self, mailbox, items):
        """Send a STATUS command"""
        return self._queue_cmd(('STATUS', (mailbox.encode('imap4-utf-7'),),
                         "(" + items + ")"))

    def cmd_append(self, mailbox, message, flags=None, timestamp=None):
        """Send an APPEND command"""
        command = ['APPEND', (mailbox.encode('imap4-utf-7'),)]
        if flags is not None:
            if not len(flags):
                flags_str = ''
            else:
                flags_str = ' '.join(flags)
            command.append('(%s)' % flags_str)
        if timestamp is not None:
            date_rfc2822 = email.Utils.formatdate(timestamp, True)
            command.append('"%s-%s-%s"' % (date_rfc2822[5:7],
                            date_rfc2822[8:11], date_rfc2822[12:]))
        command.append((message,))
        return self._queue_cmd(tuple(command))

    def cmd_check(self):
        """Send a CHECK command"""
        return self._queue_cmd(('CHECK',))

    def cmd_close(self):
        """Send a CLOSE command"""
        return self._queue_cmd(('CLOSE',))

    def cmd_expunge(self):
        """Send an EXPUNGE command"""
        return self._queue_cmd(('EXPUNGE',))

    def _cmd_search(self, cmdname, criteria, charset=None):
        """SEARCH or UID SEARCH"""
        buf = [cmdname]
        if charset is not None:
            buf.append('CHARSET ' + charset)
        for item in criteria:
            buf.append((item,))
        return self._queue_cmd(tuple(buf))

    def cmd_search(self, criteria, charset=None):
        """Perform a SEARCH for messages"""
        return self._cmd_search('SEARCH', criteria, charset)

    def _cmd_fetch(self, cmdname, sequence, items):
        """UID FETCH or FETCH"""
        if isinstance(items, basestring):
            items_str = items
        else:
            items_str = '(' + ' '.join(items) + ')'
        return self._queue_cmd((cmdname, self._sequence_to_str(sequence),
                                items_str))

    def cmd_fetch(self, sequence, items):
        """Perform a FETCH command"""
        return self._cmd_fetch('FETCH', sequence, items)

    def cmd_store(self, sequence, item, value):
        """STORE message flags"""
        if isinstance(value, basestring):
            value_str = value
        else:
            value_str = ' '.join(value)
        return self._queue_cmd(('STORE', self._sequence_to_str(sequence), item,
                                value_str))

    def cmd_copy(self, sequence, mailbox):
        """COPY command"""
        return self._queue_cmd(('COPY', self._sequence_to_str(sequence),
                                (mailbox.encode('imap4-utf-7'),)))

    def cmd_uid_fetch(self, sequence, items):
        """UID FETCH command"""
        return self._cmd_fetch('UID FETCH', sequence, items)

    def cmd_uid_search(self, criteria, charset=None):
        """UID SEARCH"""
        return self._cmd_search('UID SEARCH', criteria, charset)

    def cmd_xatom(self):
        """X<atom> command"""
        # We can't support X<atom> commands as we don't know anything
        # about parameters
        # You'll have to call _cmd_search() yourself
        raise NotImplementedError

    def cmd_unselect(self):
        """UNSELECT a mailbox"""
        return self._queue_cmd(('UNSELECT',))

    def cmd_sort(self, algo, charset, criteria):
        """SORT query"""
        # we'll abuse _cmd_search a bit here :)
        return self._cmd_search('SORT (%s) %s' % (' '.join(algo), charset),
                                criteria, None)

    def cmd_uid_sort(self, algo, charset, criteria):
        """UID SORT query"""
        # we'll abuse _cmd_search a bit here :)
        return self._cmd_search('UID SORT (%s) %s' % (' '.join(algo), charset),
                                criteria, None)

    def cmd_thread(self, algo, charset, criteria):
        """THREAD command"""
        return self._cmd_search('THREAD %s %s' % (algo, charset), criteria,
                                None)

    def cmd_idle(self):
        """Enter the RFC 2177 IDLE mode"""
        return self._queue_cmd(('IDLE',))

    @classmethod
    def _sequence_to_str(self, sequence):
        """Returns the string representation of a sequence"""
        if isinstance(sequence, basestring):
            return sequence
        elif isinstance(sequence, int):
            return str(sequence)
        else:
            # FIXME: support for better datatype
            raise NotImplementedError

    def _get_line(self):
        """Get one line of server's output.

        Based on the method of imaplib's IMAP4 class.
        """

        if not self._stream.has_data(None):
            raise TimeoutError
        line = self._stream.readline()
        if not line:
            raise InvalidResponseError("socket error: EOF")

        # Protocol mandates all lines terminated by CRLF
        if not line.endswith(CRLF):
            raise InvalidResponseError("line doesn't end with CRLF", line)

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
            # either we handle it later or IMAP server sucks
            # or we've fscked up something
            raise CommandContinuationRequest(line[2:])
        elif self._re_tagged_response.match(line):
            # Tagged response
            try:
                pos = line.index(' ')
                response.tag = line[:pos]
                line = line[pos + 1:]
            except ValueError:
                raise ParseError(line)
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
            # We do require the response code to be present immediately after
            # the tag/star. This won't catch the BADCHARSET Response Code
            # from uw-imapd (unknown version, unfortunately).
            line = line[len(response.kind) + 1:]
            if line.startswith('['):
                # parse the Response Code
                try:
                    last = line.index(']')
                    try:
                        # do we have to deal with response code with arguments?
                        space = line[1:last].index(' ')
                        code = line[1:space+1].upper()
                        arguments = self._parse_response_code(code,
                                     line[space+2:last])
                    except ValueError:
                        # just an "[atom]"
                        code = line[1:last].upper()
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
        """Helper function :)

        If line matches iterable[x][1], returns (iterable[x][0], r.match(item))
        """

        for name, r in iterable:
            foo = r.match(item)
            if foo:
                return (name, foo)
        return (None, None)

    def _parse_response_code(self, code, line):
        """Parse optional (sect 7.1) response codes"""

        if self._helper_foreach(code,
                        self._re_response_code_single)[0] is not None:
            # "[atom]"
            return None
        elif self._helper_foreach(code,
                          self._re_response_code_number)[0] is not None:
            # "[atom number]"
            try:
                return int(line)
            except ValueError:
                # not a number, let's return it as-is
                return line
        elif self._helper_foreach(code,
                          self._re_response_code_parenthesized)[0] is not None:
            # "[atom (foo bar)]"
            # we don't scream if we see garbage characters like ")"
            if not line.startswith('(') or not line.endswith(')'):
                raise ParseError(line)
            buf = line[1:-1].split(' ')
            if buf == ['']:
                return ()
            else:
                return tuple(line[1:-1].upper().split(' '))
        elif self._helper_foreach(code,
                          self._re_response_code_spaces)[0] is not None:
            # "[atom foo bar]"
            # Convert them to the uppercase form. Yup, even the "IMAP4rev1"...
            return tuple([x.upper() for x in line.split(' ')])
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
                response.data = response.data.decode('imap4-utf-7')
                pass
            elif response.kind in self._resp_mailbox_size or \
               response.kind == 'EXPUNGE':
                # "* number FOO"
                response.data = int(response.data)
            elif response.kind == 'CAPABILITY':
                response.data = tuple(
                            [item.upper() for item in response.data.split(' ')])
            elif response.kind == 'LIST' or response.kind == 'LSUB':
                # [name_attributes, hierarchy_delimiter, name]
                if not response.data.startswith('('):
                    # start of attributes is missplaced or missing
                    raise ParseError(response)
                try:
                    pos1 = response.data.index('(')
                    pos2 = response.data.index(')')
                    flags = [item.upper() for item in \
                              response.data[pos1 + 1:pos2].split(' ')]
                    if flags == ['']:
                        flags = ()
                    buf = [tuple(flags)]
                except ValueError:
                    raise ParseError(response)
                line = response.data[pos2 + 2:]
                (s, line) = self._extract_astring(line)
                # don't decode the separator
                buf.append(s)
                (s, line) = self._extract_string(line)
                buf.append(unicode(s.decode('imap4-utf-7')))
                response.data = tuple(buf)
                if not len(response.data[1]) or not len(response.data[2]):
                    # empty separator or mailbox name
                    raise ParseError(response.data)
            elif response.kind == 'STATUS':
                (s, line) = self._extract_astring(response.data)
                response.data = [s.decode('imap4-utf-7')]
                if not line.startswith('(') or not line.endswith(')'):
                    raise ParseError(line)
                items = line[1:-1].split(' ')
                buf = {}
                last = None
                for item in items:
                    if item == '':
                        break
                    if last is None:
                        key = item.upper()
                        buf[key] = None
                        last = key
                    else:
                        try:
                            buf[last] = int(item)
                        except ValueError:
                            raise ParseError(response)
                        last = None
                if last is not None:
                    # missing value for a key
                    raise ParseError(line)
                response.data.append(buf)
                response.data = tuple(response.data)
            elif response.kind == 'SEARCH' or response.kind == 'SORT':
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
                    raise ParseError(response.data)
                items = response.data[1:-1].split(' ')
                if items == ['']:
                    response.data = ()
                else:
                    response.data = tuple([item.upper() for item in items])
            elif response.kind == 'FETCH':
                # "* number FETCH (data...)"
                try:
                    msgno = int(response.data[0])
                except ValueError:
                    raise ParseError(response)
                response.data = (msgno,
                                 self._parse_fetch_response(response.data[1]))
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
        while len(line):
            limit += 1
            (s, line) = self._extract_string(line)
            if s == '(':
                # nested block
                (item, line) = self._parse_parenthesized_line(line)
                buf.append(item)
            elif s == ')':
                # end of nested block
                break
            else:
                buf.append(s)
        return (tuple(buf), line)

    def _parse_thread_response(self, line):
        """Parse THREAD respone into Python data structure

        See draft-ietf-imapext-sort-17
        """
        if line == '':
            return []
        if (line.count('(') != line.count(')')) or not line.count('('):
            raise ParseError(line)

        parent = IMAPThreadItem()
        parent.children = []
        parent.id = -1
        last = ' '
        stack = []
        root = parent

        for item in self._extract_thread_response(line):
            try:
                if item.isdigit():
                    record = IMAPThreadItem()
                    record.id = int(item)
                    if parent.children is None:
                        parent.children = [record]
                    else:
                        parent.children.append(record)
                    parent = record
                elif item == ' ':
                    continue
                elif item == '(':
                    # next item will be appended *but* we have to save current position
                    if last == '(':
                        temp = IMAPThreadItem()
                        if parent.children is None:
                            parent.children = [temp]
                        else:
                            parent.children.append(temp)
                        parent = temp
                    stack.append(parent)
                elif item == ')':
                    # time to restore the old parent
                    parent = stack.pop()
                else:
                    # FUBAR.
                    raise ParseError(line)
                last = item
            except IndexError:
                raise ParseError(line)
        return root.children

    def _parse_fetch_response(self, line):
        """Parse a string with FETCH response to a Python data structure"""
        if not line.startswith('('):
            # response isn't enclosed in parentheses
            line = line + ')'
            if self.debug > 2:
                self._log('adding parenthesis to %s' % line)
        else:
            line = line[1:]
        buf = {}
        last = None
        for token in self._parse_parenthesized_line(line)[0]:
            if isinstance(last, basestring):
                # current item is either data or continuation of identifier
                if last.count('[') != last.count(']') or \
                   last.count('<') != last.count('>'):
                    # incomplete identifier
                    if isinstance(token, basestring):
                        last += token.upper()
                    elif isinstance(token, tuple):
                        last += ' (' + ' '.join(token).upper() + ')'
                    else:
                        raise ParseError(line)
                else:
                    #buf[last] = token
                    if last == 'ENVELOPE':
                        buf[last] = IMAPEnvelope(*token)
                    elif last == 'RFC822.SIZE':
                        try:
                            buf[last] = int(token)
                        except ValueError:
                            raise ParseError(line)
                    elif last == 'FLAGS':
                        if not isinstance(token, tuple):
                            raise ParseError(line)
                        buf[last] = tuple([flag.upper() for flag in token])
                    elif last == 'INTERNALDATE':
                        buf[last] = email.Utils.mktime_tz(email.Utils.parsedate_tz(token))
                    else:
                        buf[last] = token
                    last = None
            else:
                if isinstance(token, basestring):
                    last = token.upper()
                else:
                    # last token was garbage
                    raise InvalidResponseError(line)

        if last is not None:
            # odd number of items
            raise InvalidResponseError(line)
        return buf

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
                        # RFC 3501 doesn't specify what to do here, but such
                        # data aren't formatted as specified by the ABNF syntax
                        # at the end of the RFC
                        buf += '\\' + string[pos]
                        escaping = False
                        # FIXME: need a mechanism to report non-fatal errors
                        #raise ParseError(string)
                        if self.debug >= 6:
                            self._log('escaping unknown character: %s' % string)
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
        """Tokenize the THREAD response into parentheses and spaces"""
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
