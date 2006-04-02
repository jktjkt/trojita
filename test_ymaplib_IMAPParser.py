"""Simple ymaplib tester"""
import ymaplib
import time

imap_stream = ymaplib.ProcessStream('dovecot --exec-mail imap')
parser = ymaplib.IMAPParser(imap_stream, 0)

parser._queue_cmd('capability')
parser._queue_cmd('select gentoo.gentoo-user-cs')
parser._queue_cmd('fetch 1 full')
parser._queue_cmd('status inbox ()')
time.sleep(0.1)

while not parser._outgoing.empty():
    print parser.get()

time.sleep(0.1)
parser._queue_cmd('thread references ascii from jakub')
parser._queue_cmd('logout')
time.sleep(0.5)
#parser._queue_cmd('foo')
#time.sleep(0.5)

while not parser._outgoing.empty():
    print parser.get()

while not parser.worker_exceptions.empty():
    print parser.worker_exceptions.get()
