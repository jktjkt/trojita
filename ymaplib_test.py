# -*- coding: utf-8
"""Unit tests for ymaplib"""

import unittest
import ymaplib

__revision__ = '$Id$'

class IMAPParserParseLineTest(unittest.TestCase):
    """Test for ymaplib.IMAPParser._parse_line()"""
    mailbox_names = ('~/mail/fOo', 'trms', '"qwe"', '"' + u'ěščřž' + '"',
                     '"foo"', '"ab\\\\c"')

    def setUp(self):
        self.parser = ymaplib.IMAPParser()

    def test_rubbish(self):
        """Junk data at the beginning of the line"""
        self.assertRaises(ymaplib.ParseError, self.parser._parse_line, '')
        self.assertRaises(ymaplib.ParseError, self.parser._parse_line, '.')
        self.assertRaises(ymaplib.ParseError, self.parser._parse_line, '_ * ok')

    def test_continuation(self):
        """Test if getting a continuation request raises exception"""
        self.assertRaises(ymaplib.ParseError, self.parser._parse_line, '+ ')
        self.assertRaises(ymaplib.ParseError, self.parser._parse_line, '+ foo')

    def test_untagged_unknown(self):
        """Test correct parsing of untagged reply - unknown response"""
        self.assertRaises(ymaplib.UnknownResponseError, self.parser._parse_line,
                           '* foo bar')

    def test_untagged_missing_data(self):
        """Untagged reply without human readable text"""
        self.assertRaises(ymaplib.UnknownResponseError, self.parser._parse_line,
                           '* ok')

    def test_tagged_unknown_tag(self):
        """Invalid tagname"""
        self.assertRaises(ymaplib.ParseError, self.parser._parse_line,
                           self.parser._tag_prefix.swapcase() + '12')

    def test_untagged_generic(self):
        """Untagged (OK|NO|BYE|PREAUTH)"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        ok.data = 'fOo baR Baz'
        for kind in ('ok', 'no', 'bye', 'preauth'):
            ok.kind = kind.upper()
            response = self.parser._parse_line('* %s %s' % (kind, ok.data))
            self.assertEqual(ok, response)

    def test_mallformed_response_code(self):
        """Test if unterminated Response Code screams"""
        self.assertRaises(ymaplib.ParseError, self.parser._parse_line,
                          '* ok [foo trms')

    def test_tagged_unknown(self):
        """Should scream if sent a properly tagged but unrecognizable response"""
        self.assertRaises(ymaplib.UnknownResponseError, self.parser._parse_line,
                          self.parser._tag_prefix + '123 foo bar')

    def test_preauth_with_capability(self):
        """Test an PREAUTH response with some capabilities"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        ok.data = "IMAP server ready; logged in as someuser"
        ok.response_code = ('CAPABILITY', ('IMAP4REV1', 'SORT',
                                           'THREAD=REFERENCES', 'MULTIAPPEND',
                                           'UNSELECT', 'LITERAL+', 'IDLE',
                                           'CHILDREN', 'NAMESPACE',
                                           'LOGIN-REFERRALS'))
        ok.kind = 'PREAUTH'
        
        s = "* PREaUTH [CAPaBILiTY IMAP4rev1 SORT THREAD=REFERENCES " \
            "MULTIAPPEND UNSeLECT LiTERAL+ IDLE CHILDREN NAMESPACE " \
            "LOGIN-REFErRALS] IMAP server ready; logged in as someuser"
        response = self.parser._parse_line(s)
        self.assertEqual(ok, response)

    def test_preauth(self):
        """Test PREAUTH response without capability list"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        ok.data = "user fooBar ready"
        ok.kind = 'PREAUTH'
        s = '* PREAuTH ' + ok.data
        response = self.parser._parse_line(s)
        self.assertEqual(ok, response)

    def test_capability(self):
        """Test the CAPABILITY response"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        ok.kind = 'CAPABILITY'
        ok.data = ('IMAP4REV1', 'FOO', 'BAR', 'BAZ')
        s = '* CAPABILITY IMAP4rev1 foo bar baz'
        response = self.parser._parse_line(s)
        self.assertEqual(ok, response)

    def _helper_test_response_codes(self, ok, tag, kind, message):
        """Helper for test_untagged_responses and test_tagged_status"""
        pattern = '%s %s [%s] %s'
        pattern_param = '%s %s [%s %s] %s'
        for item in (item.lower() for item in self.parser._response_code_single):
            s = pattern % (tag, kind, item, message)
            response = self.parser._parse_line(s)
            ok.response_code = (item.upper(), None)
            self.assertEqual(ok, response)
        for item in (item.lower() for item in self.parser._response_code_number):
            s = pattern % (tag, kind, item, message)
            # The actual value is missing -> let's treat it as
            # an uknown Response code
            response = self.parser._parse_line(s)
            ok.response_code = (item.upper(), None)
            self.assertEqual(ok, response)
            # now check for various values
            for num in (0, 1, 6, 37, 99, 12345, '', 'a', 'bc', 'x y'):
                s = pattern_param % (tag, kind, item, num, message)
                response = self.parser._parse_line(s)
                ok.response_code = (item.upper(), num)
                self.assertEqual(ok, response)
        for item in (item.lower() for item in
                      self.parser._response_code_spaces):
            s = pattern % (tag, kind, item, message)
            # missing value, see above
            response = self.parser._parse_line(s)
            ok.response_code = (item.upper(), None)
            self.assertEqual(ok, response)
            for param1 in ('', '0', '6', '37', '12345', '', 'a', 'bc', 'x_y'):
                for param2 in ('', ' ', ' 0', ' 2', ' ahoj', ')'):
                    s = pattern_param % (tag, kind, item, param1 + param2,
                                         message)
                    response = self.parser._parse_line(s)
                    ok.response_code = (item.upper(),
                                   tuple((param1 + param2).upper().split(' '),))
                    self.assertEqual(ok, response)
        for item in (item.lower() for item in
                      self.parser._response_code_parenthesized):
            s = pattern % (tag, kind, item, message)
            # missing value
            response = self.parser._parse_line(s)
            ok.response_code = (item.upper(), None)
            self.assertEqual(ok, response)
            for param1 in ('0', '6', '37', '', 'a', 'x_y', '(', ')'):
                for param2 in ('', ' ', ' 2', ' ahoj', '(', ')', ' (', ') '):
                    s = pattern_param % (tag, kind, item,
                                         '(' + param1 + param2 + ')', message)
                    response = self.parser._parse_line(s)
                    ok.response_code = (item.upper(),
                                   tuple((param1 + param2).upper().split(' ')),)
                    if ok.response_code[1] == ('',):
                        ok.response_code = (ok.response_code[0], ())
                    self.assertEqual(ok, response)

    def test_untagged_status(self):
        """Test the untagged responses with optional Response Code"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        for stuff in ('blabla trMs', 'x', '', '1', '4589',
                      'Ave caesar imperator, morituri te salutant'):
            for kind in ('ok', 'no', 'bye', 'preauth'):
                ok.kind = kind.upper()
                ok.data = stuff
                ok.response_code = (None, None)
                s = '* %s %s' % (kind, stuff)
                response = self.parser._parse_line(s)
                self.assertEqual(ok, response)
                self._helper_test_response_codes(ok, '*', kind, stuff)

    def test_tagged_status(self):
        """Test tagged OK/NO/BAD responses"""
        ok = ymaplib.IMAPResponse()
        ok.tag =  self.parser._tag_prefix + '12345'
        for stuff in ('trms', 'BS', '', 'a', 'bezi liska k Taboru'):
            for kind in ('ok', 'no', 'bad'):
                ok.kind = kind.upper()
                ok.response_code = (None, None)
                ok.data = stuff
                response = self.parser._parse_line('%s %s %s' % (
                                                   ok.tag, kind.upper(), stuff))
                self.assertEqual(ok, response)
                self._helper_test_response_codes(ok, ok.tag, kind, stuff)

    def test_resp_capability(self):
        """Test the CAPABILITY untagged response"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        ok.kind = 'CAPABILITY'
        for capabilities in (('',), ('foo',), ('a', 'b')):
            s = '* capaBility ' + ' '.join(capabilities)
            ok.data = tuple([item.upper() for item in capabilities])
            response = self.parser._parse_line(s)
            self.assertEqual(ok, response)

    def test_resp_list_lsub(self):
        """Test the LIST and LSUB responses"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        for kind in ('list', 'lsub'):
            ok.kind = kind.upper()
            for flags in ((), ('\\MarkEd',), ('\\foo', '\\bar')):
                for separator in ('.', '/', '\\\\', 'w'):
                    for mailbox in self.mailbox_names:
                        my_mailbox = mailbox.encode('imap4-utf-7')
                        s = '* %s (%s) "%s" %s' % (kind, ' '.join(flags),
                                    separator, my_mailbox)
                        response = self.parser._parse_line(s)
                        if my_mailbox.startswith('"'):
                            my_mailbox = my_mailbox[1:-1].decode('string_escape')
                        else:
                            my_mailbox = my_mailbox.decode('string_escape')
                        ok.data = (tuple([item.upper() for item in flags]),
                                   separator.decode('string_escape'),
                                   my_mailbox.decode('imap4-utf-7'))
                        self.assertEqual(ok, response)

    def test_resp_list_lsub_invalid(self):
        """Test invalid data in LIST/LSUB responses"""
        for command in ('LIST', 'lsub'):
            for stuff in ('(trms)', 'x (foo)', 'foo', '"" (bar)', '(bar) xy',
                          '(x yz zz) zzz '):
                self.assertRaises(ymaplib.ParseError, self.parser._parse_line,
                                   '* %s %s' % (command, stuff))
        

    def test_resp_status(self):
        """Test untagged STATUS response"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        ok.kind = 'STATUS'
        args = ({'messages': 231, 'uidnExt': 44292, 'foobaR': 5},
                {'foo': 10}, {'foo': 0}, {'x': 2}
               )
        for mailbox in self.mailbox_names:
            for items in args:
                my_mailbox = mailbox.encode('imap4-utf-7')
                s_stack = []
                stack = {}
                for name in items:
                    s_stack.append('%s %s' % (name, items[name]))
                    stack[name.upper()] = items[name]
                s = '* staTus %s (%s)' % (my_mailbox, ' '.join(s_stack))
                response = self.parser._parse_line(s)
                if my_mailbox.startswith('"'):
                    my_mailbox = my_mailbox[1:-1].decode('string_escape')
                else:
                    my_mailbox = my_mailbox.decode('string_escape')
                ok.data = (my_mailbox.decode('imap4-utf-7'), stack)
                self.assertEqual(ok, response)

    def test_resp_status_invalid(self):
        """Invalid data in a STATUS response"""
        for stuff in ('bs', '()', '(a)', '(a 1)', 'x(1)', 'x(1 2)', 'yy (a a)',
                      'fn (a 2 b)', ''):
            self.assertRaises(ymaplib.ParseError, self.parser._parse_line,
                              '* status %s' % stuff)

    def test_resp_search_sort(self):
        """Test the SEARCH/SORT responses"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        for command in ('search', 'sort'):
            ok.kind = command.upper()
            for stuff in ((), ('0',), ('1',), ('1', '2'),
                          ('45', '9976', '99', '147', '251', '5')):
                s = '* %s %s' % (command, ' '.join(stuff))
                ok.data = tuple([int(item) for item in stuff])
                response = self.parser._parse_line(s)
                self.assertEqual(ok, response)

    def test_resp_search_invalid(self):
        """Garbage in SEARCH and SORT responses"""
        for command in ('search', 'sort'):
            for stuff in ('x', 'x y', 'x y zzz', '1 2 a', 'a b 5', 'a54'):
                self.assertRaises(ymaplib.ParseError, self.parser._parse_line,
                                  '* %s %s' % (command, stuff))

    def test_resp_flags(self):
        """Test the FLAGS response"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        ok.kind = 'FLAGS'
        for stuff in ((), ('',), ('foo', 'bar'), ('1',), ('\\bar',),
                      ('\\ahoj', '\\voe'), ('\\x', 'y'), ('1', 'trms', '2')):
            s = '* fLAgs (%s)' % (' '.join(stuff))
            if stuff == ('',):
                ok.data = ()
            else:
                ok.data = tuple([item.upper() for item in stuff])
            response = self.parser._parse_line(s)
            self.assertEqual(ok, response)

    def test_resp_flags_invalid(self):
        """Garbage in a FLAGS response"""
        for stuff in ('', '(\\ab', '\\ba)', '(a b ', 'xy z gg)', 'a ba c'):
            self.assertRaises(ymaplib.ParseError, self.parser._parse_line,
                              '* fLags %s' % stuff)

    def test_resp_exists_recent_expunge(self):
        """Test the EXISTS, RECENT and EXPUNGE responses"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        for command in ('exists', 'recent', 'expunge'):
            ok.kind = command.upper()
            for number in (0, 1, 6, 54, 789, 45612377):
                s = '* %d %s' % (number, command)
                ok.data = number
                response = self.parser._parse_line(s)
                self.assertEqual(ok, response)

    def test_resp_exists_recent_expunge_invalid(self):
        """Junk data in the EXISTS, RECENT and EXPUNGE responses"""
        for command in ('exists', 'recent', 'expunge'):
            for stuff in ('', 'ahoj', 'x y z', '1 2', '(1)'):
                self.assertRaises(ymaplib.UnknownResponseError,
                                  self.parser._parse_line,
                                  '* %s %s' % (stuff, command))
                self.assertRaises(ymaplib.UnknownResponseError,
                                  self.parser._parse_line,
                                  '* %s %s' % (command, stuff))
            for stuff in (0, 2, 5489):
                self.assertRaises(ymaplib.UnknownResponseError,
                                  self.parser._parse_line,
                                  '* %s %d' % (command, stuff))

    def test_resp_thread(self):
        """Test the THREAD response"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        ok.kind = 'THREAD'

        ok.data = []
        response = self.parser._parse_line('* thRead')
        self.assertEqual(ok, response)
        response = self.parser._parse_line('* thRead ')
        self.assertEqual(ok, response)
        response = self.parser._parse_line('* thRead ()')
        self.assertEqual(ok, response)

        response = self.parser._parse_line('* thRead (1 2 3)')
        i1 = ymaplib.IMAPThreadItem()
        i1.id = 1
        i2 = ymaplib.IMAPThreadItem()
        i2.id = 2
        i3 = ymaplib.IMAPThreadItem()
        i3.id = 3
        i1.children = [i2]
        i2.children = [i3]
        ok.data = [i1]
        self.assertEqual(ok, response)

        i1 = ymaplib.IMAPThreadItem()
        i1.id = 78
        i2 = ymaplib.IMAPThreadItem()
        i2.id = 23
        ok.data = [i1, i2]
        for s in ('(78)(23)', '(78) (23)'):
            response = self.parser._parse_line('* thRead (78) (23)')
            self.assertEqual(ok, response)

        response = self.parser._parse_line('* thRead (1) (2 3)')
        i1 = ymaplib.IMAPThreadItem()
        i1.id = 1
        i2 = ymaplib.IMAPThreadItem()
        i2.id = 2
        i3 = ymaplib.IMAPThreadItem()
        i3.id = 3
        i2.children = [i3]
        ok.data = [i1, i2]
        self.assertEqual(ok, response)

        response = self.parser._parse_line('* thRead (2)(3 6 (4 23)(44 7 96))')
        i2 = ymaplib.IMAPThreadItem()
        i2.id = 2
        i3 = ymaplib.IMAPThreadItem()
        i3.id = 3
        i6 = ymaplib.IMAPThreadItem()
        i6.id = 6
        i4 = ymaplib.IMAPThreadItem()
        i4.id = 4
        i23 = ymaplib.IMAPThreadItem()
        i23.id = 23
        i44 = ymaplib.IMAPThreadItem()
        i44.id = 44
        i7 = ymaplib.IMAPThreadItem()
        i7.id = 7
        i96 = ymaplib.IMAPThreadItem()
        i96.id = 96
        ok.data = [i2, i3]
        i3.children = [i6]
        i6.children = [i4, i44]
        i4.children = [i23]
        i44.children = [i7]
        i7.children = [i96]
        self.assertEqual(ok, response)

        response = self.parser._parse_line('* thRead ((3)(5))')
        iroot = ymaplib.IMAPThreadItem()
        i3 = ymaplib.IMAPThreadItem()
        i3.id = 3
        i5 = ymaplib.IMAPThreadItem()
        i5.id = 5
        iroot.children = [i3, i5]
        ok.data = [iroot]
        self.assertEqual(ok, response)

    def test_resp_thread_invalid(self):
        """Junk data in a THREAD response"""
        for stuff in ('2)', '(', ')', '(2', '5', '1 5 8', '(x) (y)', '(x)(4)'):
            try:
                print self.parser._parse_line('* thread %s' % stuff)
                print stuff
            except ymaplib.ParseError:
                pass
            self.assertRaises(ymaplib.ParseError, self.parser._parse_line,
                              '* thread %s' % stuff)

    # FIXME: FETCH

if __name__ == '__main__':
    unittest.main()
