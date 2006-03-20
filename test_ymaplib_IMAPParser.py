"""Simple ymaplib tester"""
import ymaplib
import time
import threading

stream = ymaplib.ProcessStream('dovecot --exec-mail imap')
parser = ymaplib.IMAPParser(stream)

parser.send_command('capability')
parser.send_command('select gentoo.gentoo-user-cs')
parser.send_command('fetch 1 full')
parser.send_command('status inbox ()')
parser.send_command('thread references ascii from jakub')

time.sleep(2)
tagged = parser.tagged_responses
untagged = parser.get_untagged_responses()

for item in tagged.values():
    print item
for item in untagged:
    print item
#print parser.responses_stream_thread
time.sleep(2)