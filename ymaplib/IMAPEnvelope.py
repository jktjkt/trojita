# -*- coding: utf-8
"""Container for the RFC822 envelope"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

import time
import email.Utils
import exceptions

__revision__ = '$Id$'

class IMAPEnvelope:
    """Container for RFC822 envelope"""

    def __repr__(self):
        if self.date is None:
            date = 'None'
        else:
            date = time.strftime('%c %Z', time.localtime(self.date))
        return ('<trojita.ymaplib.IMAPEnvelope: Date: %s, Subj: "%s", From: %s, ' + \
               'Sender: %s, Reply-To: %s, To: %s, Cc: %s, Bcc: %s, ' + \
               'In-Reply-To: %s, Message-Id: %s>') % (
               date, self.subject, self.from_, self.sender, self.reply_to,
               self.to, self.cc, self.bcc, self.in_reply_to, self.message_id)

    def __init__(self, date=None, subject=None, from_=None, sender=None,
                 reply_to=None, to=None, cc=None, bcc=None, in_reply_to=None,
                 message_id=None):
        """Container for an Evelope as specified by RFC 3501

        For meaning of parameters, see the above mentioned RFC.
        """
        if isinstance(date, basestring):
            self.date = email.Utils.mktime_tz(email.Utils.parsedate_tz(date))
        elif date is None:
            self.date = None
        else:
            raise ParseError(date)
        self.subject = subject
        self.from_ = from_
        self.sender = sender
        self.reply_to = reply_to
        self.to = to
        self.cc = cc
        self.bcc = bcc
        self.in_reply_to = in_reply_to
        self.message_id = message_id

    def __eq__(self, other):
        return (self.date == other.date and self.subject == other.subject and
                self.from_ == other.from_ and self.sender == other.sender and
                self.reply_to == other.reply_to and self.to == other.to and
                self.cc == other.cc and self.bcc == other.bcc and
                self.in_reply_to == other.in_reply_to and
                self.message_id == other.message_id)

    def __ne__(self, other):
        return not self.__eq__(other)
