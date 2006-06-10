# -*- coding: utf-8
"""Simple ymaplib tester"""
import ymaplib
import streams
import time

imap_stream = streams.ProcessStream('dovecot --exec-mail imap')
#imap_stream = streams.TCPStream('localhost', 143, timeout=1.5)
#imap_stream = streams.OpenSSLStream('localhost', 143, timeout=1.5)
parser = ymaplib.IMAPParser(imap_stream, 10)
parser.enable_literal_plus = False
parser.start_worker()

parser.cmd_capability()
#parser.cmd_starttls()
#parser.cmd_capability()
#parser.cmd_noop()
#auth = ymaplib.PLAINAuthenticator('user', 'password')
#parser.cmd_authenticate(auth)
#parser.cmd_noop()

# the following line will raise an exception of working with LITERAL+
#parser._queue_cmd(('FOO', 'CHARSET utf-8', ('text',), ('odkazy.\n',), ('to',), ('user-cs',)))

message = """Date: Mon, 7 Feb 1994 21:52:25 -0800 (PST)
From: Fred Foobar <foobar@Blurdybloop.COM>
Subject: afternoon meeting
To: mooch@owatagu.siam.edu
Message-Id: <B27397-0100000@Blurdybloop.COM>
MIME-Version: 1.0
Content-Type: TEXT/PLAIN; CHARSET=US-ASCII

Hello Joe, do you think we can meet at 3:30 tomorrow?"""
flags = ('\\seen', '\\deleted')
timestamp = time.mktime(time.localtime())
#parser.cmd_append('INBOX', message, flags, timestamp)

#parser.cmd_list('', '*')

# just a demo to show that we can resume :)
print parser.start_worker(parser.stop_worker())

parser.cmd_select('gentoo.gentoo-user-cs')
#parser.cmd_search(('text', 'odkazy.\n', 'to', 'user-cs'), 'utf-8')
#parser.cmd_uid_search(('text', 'odkazy.\n', 'to', 'user-cs'), 'utf-8')
#parser.cmd_sort(('subject',), 'utf-8', ('from', 'jkt'))
#parser.cmd_uid_sort(('subject',), 'utf-8', ('from', 'jkt'))
#parser.cmd_thread('references', 'utf-8', ('all',))
#parser.cmd_examine('gentoo.gentoo-user-cs')
#parser.cmd_close()
#parser.cmd_expunge()
#parser.cmd_status('gentoo.gentoo-user-cs', 'messages recent')
#parser.cmd_check()
#parser.cmd_unselect()
#parser.cmd_lsub('', '*')
#parser.cmd_subscribe('trms')
#parser.cmd_lsub('', '*')
#parser.cmd_unsubscribe('trms')
#parser.cmd_lsub('', '*')
#parser.cmd_create('foo\nbar')
#parser.cmd_select('foo\nbar')
#parser.cmd_rename('foo\nbar', 'bar-foo')
#parser.cmd_delete('bar-foo')
#parser.cmd_noop()
#parser.cmd_logout()
#parser.cmd_login('foo', 'bar')
#parser.cmd_capability()
#parser.cmd_list('', '*')
#parser.cmd_select('inbox')
#parser.cmd_fetch('*:*', 'ALL')
#parser.cmd_fetch('*:*', ('envelope', 'flags', 'internaldate', 'body.peek[]'))
#parser.cmd_fetch('*', ('FLAGS', 'BODY[HEADER.FIELDS (DATE FROM)]'))
#parser.cmd_store('*', '+FLAGS', '\\seen')
#parser.cmd_copy('*', u'ěšč')
#parser.cmd_idle()
#i=0
#while 1:
#    time.sleep(0.1)
#    while not parser._outgoing.empty():
#        print parser.get()
#    if i>100:
#        break
#    i += 1

#parser.cmd_noop()
#parser._queue_cmd(('capability',))
#parser._queue_cmd(('select gentoo.gentoo-user-cs',))
#parser._queue_cmd(('fetch 1 full',))
#parser._queue_cmd(('status inbox ()',))

time.sleep(2)

while not parser._outgoing.empty():
    print parser.get()

#time.sleep(0.1)
#parser._queue_cmd('thread references ascii from jakub')
#parser._queue_cmd('logout')
time.sleep(0.5)
#parser._queue_cmd('foo')
#time.sleep(0.5)

while not parser._outgoing.empty():
    print parser.get()

parser._check_worker_exceptions()
while not parser.worker_exceptions.empty():
    print 'hmmm'
    print parser.worker_exceptions.get()
