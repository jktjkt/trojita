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

sleep 0.1s
}

delete_sequence() {
    (
    echo -e "1 SELECT INBOX\r\n2 STORE $1 +FLAGS (\\deleted)\r\n"\
"3 EXPUNGE\r\n4 LOGOUT\r\n"; sleep 1) | /usr/libexec/dovecot/imap >/dev/null
    echo
}

send_mail A
send_mail B
send_mail C

echo "Mailbox contents: A B C"; read

send_mail D

echo "Mailbox contents: A B C D"; read

delete_sequence "2"

echo "Mailbox contents: A C D"; read

delete_sequence "3"

echo "Mailbox contents: A C"; read

delete_sequence "1"

echo "Mailbox contents: C"; read

delete_sequence "1:*"

echo "Mailbox is empty now"; read

send_mail A
send_mail B
send_mail C
send_mail D

echo "Mailbox contents: A B C D"; read

delete_sequence "2:3"

echo "Mailbox contents: A D"; read

send_mail E
send_mail F
send_mail G

echo "Mailbox contents: A D E F G"; read

delete_sequence "2,4"

echo "Mailbox contents: A E G"; read

delete_sequence "1,3"

echo "Mailbox contents: E"; read

delete_sequence "1:*"

send_mail A
send_mail B
send_mail C
send_mail D
send_mail E
send_mail F
send_mail G

delete_sequence "1:3,5:6"

echo "Mailbox contents: D G"; read

delete_sequence "1:*"

echo "Mailbox is empty now and that's all :)"; read


