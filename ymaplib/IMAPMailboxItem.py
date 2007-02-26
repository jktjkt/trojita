# -*- coding: utf-8
"""Data structures for listing mailboxes"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'

class IMAPMailboxItem:
    """Represents one mailbox in a mailbox hiererarchy"""

    # mailbox attributes from RFC 3501
    LIST_NOINFERIORS="\\Noinferiors"
    LIST_NOSELECT="\\Noselect"
    LIST_MARKED="\\Marked"
    LIST_UNMARKED="\\Unmarked"

    def __init__(self, name, attributes, delimiter, children):
        self.name = name
        self.attributes = attributes
        self.delimiter = delimiter
        self.children = children
