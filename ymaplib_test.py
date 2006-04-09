"""Unit tests for ymaplib"""

import unittest
import ymaplib

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
            self.assertEqual((ok.tag, ok.kind, ok.data),
                             (response.tag, response.kind, response.data))

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
        ok.response_code = ('CAPABILITY', ('IMAP4rev1', 'SORT',
                                           'THREAD=REFERENCES', 'MULTIAPPEND',
                                           'UNSELECT', 'LITERAL+', 'IDLE',
                                           'CHILDREN', 'NAMESPACE',
                                           'LOGIN-REFERRALS'))
        ok.kind = 'PREAUTH'
        
        # FIXME: we might want to change the case of some letters, but I'm
        # not sure if that's the Right Thing(tm)
        # see the ymaplib.IMAPParser._parse_line for reasons...
        str = "* PREaUTH [CAPaBILiTY IMAP4rev1 SORT THREAD=REFERENCES " \
              "MULTIAPPEND UNSELECT LITERAL+ IDLE CHILDREN NAMESPACE " \
              "LOGIN-REFERRALS] IMAP server ready; logged in as someuser"
        response = self.parser._parse_line(str)
        self.assertEqual((ok.tag, ok.kind, ok.data, ok.response_code),
                         (response.tag, response.kind, response.data,
                           response.response_code))

if __name__ == '__main__':
    unittest.main()