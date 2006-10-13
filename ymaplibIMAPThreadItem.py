# -*- coding: utf-8
"""One message in the threaded mailbox"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006

__revision__ = '$Id$'

class IMAPThreadItem:
    """One message in the threaded mailbox"""
    def __init__(self):
        self.id = None
        self.children = None

    def __repr__(self):
        return "<ymaplib.IMAPThreadItem %s: %s>" % (self.id, self.children)
        #return self.__str__()

    #def __str__(self, depth=0):
    #    s = depth * ' ' + ("%s:" % self.id)
    #    depth += 1
    #    if self.children is None:
    #        return '\n' + s + depth * ' ' + 'None'
    #    else:
    #        for child in self.children:
    #            s += '\n' + depth * ' ' + child.__str__(depth)
    #        return s

    def __eq__(self, other):
        if not isinstance(other, IMAPThreadItem) or self.id != other.id:
            return False
        if self.children is None and other.children is None:
            return True
        if len(self.children) != len(other.children):
            return False
        for i in range(len(self.children)):
            if self.children[i] != other.children[i]:
                return False
        return True

    def __ne__(self, other):
        return not self.__eq__(other)
