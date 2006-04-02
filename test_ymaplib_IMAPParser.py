"""Simple ymaplib tester"""
import ymaplib
import time
import threading

stream = ymaplib.ProcessStream('dovecot --exec-mail imap')
parser = ymaplib.IMAPParser(stream, 0)

parser.cmd('capability')
parser.cmd('select gentoo.gentoo-user-cs')
parser.cmd('fetch 1 full')
parser.cmd('status inbox ()')
time.sleep(0.1)

while not parser._outgoing.empty():
    print parser.get()

time.sleep(0.1)
parser.cmd('thread references ascii from jakub')
parser.cmd('logout')
parser.cmd('foo')
parser._parse_line('* 60 FETCH (FLAGS (\Seen) INTERNALDATE "21-Mar-2006 17:19:30 +0100" RFC822.SIZE 2995 ENVELOPE ("Tue, 21 Mar 2006 17:19:14 +0100" "Role of infra" (("=?UTF-8?B?SmFuIEt1bmRyw6F0?=" NIL "jkt" "gentoo.org")) (("=?UTF-8?B?SmFuIEt1bmRyw6F0?=" NIL "jkt" "gentoo.org")) (("=?UTF-8?B?SmFuIEt1bmRyw6F0?=" NIL "jkt" "gentoo.org")) ((NIL NIL "seemant" "gentoo.org")(NIL NIL "g2boojum" "gentoo.org")) NIL NIL NIL "<44202782.5030004@gentoo.org>"))')

time.sleep(1)

while not parser._outgoing.empty():
    print parser.get()
