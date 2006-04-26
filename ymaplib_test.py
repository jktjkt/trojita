"""Unit tests for ymaplib"""

import unittest
import ymaplib

__revision__ = '$Id$'

class IMAPParserParseLineTest(unittest.TestCase):
    """Test for ymaplib.IMAPParser._parse_line()"""

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

    def test_untagged_generic(self):
        """Untagged (OK|NO|BYE|PREAUTH)"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        ok.data = 'fOo baR Baz'
        for kind in ('ok', 'no', 'bye', 'preauth'):
            ok.kind = kind.upper()
            response = self.parser._parse_line('* %s fOo baR Baz' % kind)
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
        
        # FIXME: we might want to change the case of some letters, but I'm
        # not sure if that's the Right Thing(tm)
        # see the ymaplib.IMAPParser._parse_line for reasons...
        s = "* PREaUTH [CAPaBILiTY IMAP4rev1 SORT THREAD=REFERENCES " \
            "MULTIAPPEND UNSELECT LITERAL+ IDLE CHILDREN NAMESPACE " \
            "LOGIN-REFERRALS] IMAP server ready; logged in as someuser"
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
        ok.data = ('IMAP4rev1', 'foo', 'bar', 'baz')
        s = '* CAPABILITY IMAP4rev1 foo bar baz'
        response = self.parser._parse_line(s)
        self.assertEqual(ok, response)

    def test_untagged_responses(self):
        """Test the untagged responses with optional Response Code"""
        ok = ymaplib.IMAPResponse()
        ok.tag = None
        stuff = 'blabla trMs'
        for stuff in ('blabla trMs', 'x', '',
                      'Ave caesar imperator, morituri te salutant'):
            for kind in ('ok', 'no', 'bye', 'preauth'):
                ok.kind = kind.upper()
                ok.data = stuff
                ok.response_code = (None, None)
                s = '* %s %s' % (kind, stuff)
                response = self.parser._parse_line(s)
                self.assertEqual(ok, response)
                for item in ('alert', 'parse', 'read-only', 'read-write',
                             'trycreate'):
                    s = '* %s [%s] %s' % (kind, item, stuff)
                    response = self.parser._parse_line(s)
                    ok.response_code = (item.upper(), None)
                    self.assertEqual(ok, response)
                for item in ('uidnext', 'uidvalidity', 'unseen'):
                    s = '* %s [%s] %s' % (kind, item, stuff)
                    # The actual value is missing -> let's treat it as
                    # an uknown Response code
                    response = self.parser._parse_line(s)
                    ok.response_code = (item.upper(), None)
                    self.assertEqual(ok, response)
                    # now check for various values
                    for num in (0, 1, 6, 37, 99, 12345, '', 'a', 'bc', 'x y'):
                        s = '* %s [%s %s] %s' % (kind, item, num, stuff)
                        response = self.parser._parse_line(s)
                        ok.response_code = (item.upper(), num)
                        self.assertEqual(ok, response)
                # CAPABILITY is already tested in test_preauth_with_capability

    def test_tagged_ok_no_bad(self):
        """Test tagged OK/NO/BAD responses"""
        ok = ymaplib.IMAPResponse()
        ok.tag =  self.parser._tag_prefix + '12345'
        ok.data = 'trms'
        for kind in ('ok', 'no', 'bad'):
            ok.kind = kind.upper()
            response = self.parser._parse_line('%s %s %s' % (
                                                ok.tag, kind.upper(), ok.data))
            # FIXME: complete the code :)

if __name__ == '__main__':
    unittest.main()