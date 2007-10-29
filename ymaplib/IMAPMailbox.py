# -*- coding: utf-8
"""Interface to an IMAP mailbox"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

import cPickle

__revision__ = '$Id$'

class IMAPMailbox:
    """Interface to an IMAP mailbox"""

    def store(self, message, flags=None, date=None):
        """Store a new message into the specified mailbox

        message -- message compatible with email.Message
        flags -- IMAP Flags
        date -- IMAP Internal Date
        """
        raise FIXME

    def message_copy(self, message, new_namespace, new_mailbox):
        """Copy message to another mailbox

        message -- which message to copy
        new_namespace, new_mailbox -- where to put it
        """
        raise FIXME


    def expunge(self):
        """Expunge all deleted messages"""
        raise FIXME

    def status(self):
        """Return some statistical data concerning this mailbox"""
        raise FIXME

    def append(self, message):
        """Save a message in this mailbox"""
        raise FIXME

    def message(self, message):
        """Returns an IMAPMessage for given message"""
        raise FIXME

    def copy(self, message, target_namespace, target_mailbox):
        """Copy selected message to specified mailbox"""
        raise FIXME

    def sort(self):
        """Returns a sorted list of messages"""
        raise FIXME

    def thread(self, algo):
        """Returns threaded view"""
        raise FIXME

    def search(self, criteria, charset=None):
        """Search mailbox using specified search terms

        criteria -- Unicode string to be passed to the server, see RFC 3501
                    page 49 for details

        By default, searches are done either in UTF-8 or in US-ASCII encoding.
        Other encodings will be tried it the reply from the server suggests that
        we're talking to a dumb machine that can't speak UTF-8.
        """
        raise FIXME

    @classmethod
    def _optimized_sequence(cls, data):
        """Return and IMAP string sequence matching given (sorted) range"""
        # FIXME: now we're really dumb and include more than we really need...
        return "%d:%d" % (data[0], data[len(data) - 1])

