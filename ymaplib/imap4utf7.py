"""IMAP4 Modified UTF-7 implementation

Based on Twisted Mail stuff, Copyright (c) 2001-2004 Twisted Matrix Laboratories.
"""

import codecs

__revision__ = "$Id$"

def modified_base64(s):
    s_utf7 = s.encode('utf-7')
    return s_utf7[1:-1].replace('/', ',')

def modified_unbase64(s):
    s_utf7 = '+' + s.replace(',', '/') + '-'
    return s_utf7.decode('utf-7')

def encoder(s):
    r = []
    _in = []
    for c in s:
        if ord(c) in (range(0x20, 0x26) + range(0x27, 0x7f)):
            if _in:
                r.extend(['&', modified_base64(''.join(_in)), '-'])
                del _in[:]
            r.append(str(c))
        elif c == '&':
            if _in:
                r.extend(['&', modified_base64(''.join(_in)), '-'])
                del _in[:]
            r.append('&-')
        else:
            _in.append(c)
    if _in:
        r.extend(['&', modified_base64(''.join(_in)), '-'])
    return (''.join(r), len(s))

def decoder(s):
    r = []
    decode = []
    for c in s:
        if c == '&' and not decode:
            decode.append('&')
        elif c == '-' and decode:
            if len(decode) == 1:
                r.append('&')
            else:
                r.append(modified_unbase64(''.join(decode[1:])))
            decode = []
        elif decode:
            decode.append(c)
        else:
            r.append(c)
    if decode:
        r.append(modified_unbase64(''.join(decode[1:])))
    return (''.join(r), len(s))

class StreamReader(codecs.StreamReader):
    def decode(self, s, errors='strict'):
        return decoder(s)

class StreamWriter(codecs.StreamWriter):
    def decode(self, s, errors='strict'):
        return encoder(s)

def imap4_utf_7(name):
    if name == 'imap4-utf-7':
        return (encoder, decoder, StreamReader, StreamWriter)
codecs.register(imap4_utf_7)
