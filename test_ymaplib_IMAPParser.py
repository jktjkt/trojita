# -*- coding: utf-8
"""Simple ymaplib tester"""
import ymaplib
import time

imap_stream = ymaplib.ProcessStream('dovecot --exec-mail imap')
parser = ymaplib.IMAPParser(imap_stream, 0)
parser.enable_literal_plus = False
parser.start_worker()

parser.cmd_capability()
# the following line will raise an exception of working with LITERAL+
#parser._queue_cmd(('FOO', 'CHARSET utf-8', ('text',), ('odkazy.\n',), ('to',), ('user-cs',)))

parser.cmd_list('', '*')
parser.cmd_select('gentoo.gentoo-user-cs')
parser.cmd_search(('text', 'odkazy.\n', 'to', 'user-cs'), 'utf-8')
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
parser.cmd_logout()
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
