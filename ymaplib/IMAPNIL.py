# -*- coding: utf-8
"""A simple class to hold the NIL token"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'

class IMAPNIL:
    """Simple class to hold the NIL token"""
    def __repr__(self):
        return '<trojita.ymaplib.IMAPNIL>'

    def __eq__(self, other):
        return isinstance(other, IMAPNIL)

    def __ne__(self, other):
        return not self.__eq__(other)
