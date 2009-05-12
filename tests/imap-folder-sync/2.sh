#!/bin/bash

echo "Warning: this will nuke your Dovecot's INBOX!"
echo "Press Ctrl-C IMMEDIATELLY if you don't want to proceed."
echo
echo "Press Enter to continue AND LOOSE ALL DATA."
read

send_mail() {
    echo -e "From: jkt@flaska.net\r\n"\
"To: jkt@flaska.net\r\n"\
"Subject: $1\r\n"\
"Date: dneska\r\n\r\nblesmrt 333 666\r\n"| /usr/libexec/dovecot/deliver -e
}

delete_sequence() {
    (
    echo -e "1 SELECT INBOX\r\n2 STORE $1 +FLAGS (\\deleted)\r\n"\
"3 EXPUNGE\r\n4 LOGOUT\r\n"; sleep 1) | dovecot --exec-mail imap >/dev/null
    echo
}

send_mail X

echo "Mailbox contents: X"; read

delete_sequence "1"
send_mail Y
send_mail Z

echo "Mailbox contents: Y Z"; read

delete_sequence "1:*"

echo "Mailbox is empty now and that's all :)"; read
