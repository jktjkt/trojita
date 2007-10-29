# -*- coding: utf-8
"""Data structures for NAMESPACE"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'

class IMAPNamespace:
    def __init__(self, personal=None, others=None, shared=None):
        """Namespace (RFC 2342) definition

        Each argument can be None (equal to empty list), list or tuple"""

        # check parameters
        for item in (personal, others, shared):
            if item is None:
                # if we could modify "item" here, we wouldn't have to check for
                # None later...
                continue
            okay = False
            if isinstance(item, tuple) or isinstance(item, list):
                okay = True
                for namespace in item:
                    if not isinstance(namespace, IMAPNamespaceItem):
                        okay = False
                        break
            if not okay:
                raise ValueError("Parameters should be None, tuple or list")

        if personal is None:
            self.personal = ()
        else:
            self.personal = personal
        if others is None:
            self.others = ()
        else:
            self.others = others
        if shared is None:
            self.shared = ()
        else:
            self.shared = shared

    def __repr__(self):
        return ("<trojita.ymaplib.IMAPNamespace: personal = %s, others = %s, " +
                "shared = %s>") % (self.personal, self.others, self.shared)

    def __eq__(self, other):
        return isinstance(other, type(self)) and (self.personal ==
            other.personal) and (self.others == other.others) and (self.shared
            == other.shared)

    def __ne__(self, other):
        return not self.__eq__(other)


class IMAPNamespaceItem:
    def __init__(self, prefix, delimiter):
        """Namespace description for each namespace type

        See RFC 2342 for details, this is just a relativelly dumb wrapper.

        prefix -- where does it start
        delimiter -- what separates nested levels"""

        self.prefix = prefix
        self.delimiter = delimiter

    def __repr__(self):
        return ("<trojita.ymaplib.IMAPNamespaceItem: prefix '%s', " +
                "delimiter '%s'>") % (self.prefix, self.delimiter)

    def __eq__(self, other):
        return (isinstance(other, type(self) and (self.prefix == other.prefix)
            and (self.delimiter == other.delimiter)))

    def __ne__(self, other):
        return not self.__eq__(other)
