# -*- coding: utf-8
"""RFC822 message stored on an IMAP server"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'

import email.Message
from exceptions import AttrReadOnlyError

class IMAPMessage(email.Message.Message):
    """RFC822 message stored on an IMAP server"""
    # FIXME: add actual code, this is only a template

    def __init__(self, client, namespace, mailbox, uidvalidity, uid,
                 msg_part=None):
        """Create new instance tied to a specified message in given IMAPClient

        client -- IMAPClient instance
        namespace, mailbox -- malibox specification
        uidvalidity, uid -- message specification
        msg_part -- indicates message nesting (None means "root")
        """

        email.Message.Message.__init__(self)
        # mailbox reference
        self._client = client
        self._namespace = namespace
        self._mailbox = mailbox
        self._uidvalidity = uidvalidity
        self._uid = uid
        self._msg_part = msg_part
        # IMAP message properties
        self._structure = None
        self._flags = None
        self._internaldate = None
        self._size = None
        self._envelope = None
        # override email.Message.Message stuff
        self._headers = None

    # FIXME: get_payload() needs structure and possibly data
    # FIXME: __len__(), __contains__(), keys(), values(), items(), get(),
    # get_all() need headers
    # FIXME: walk() needs something :)

    def is_multipart(self):
        if self._structure is None:
            self._fetch_structure()
        # FIXME: we can't use the payload attribute as email.Message does...
        raise FIXME

    def get_charset(self):
        if self._structure is None:
            self._fetch_structure()
        raise FIXME

    def get_payload(self):
        raise FIXME

    def __len__(self):
        raise FIXME

    def __contains__(self):
        raise FIXME

    def keys(self):
        raise FIXME

    def values(self):
        raise FIXME

    def items(self):
        raise FIXME

    def get(self):
        raise FIXME

    def get_all(self):
        raise FIXME

    def _fetch_flags(self):
        """Fetch FLAGS from IMAP server"""
        raise FIXME

    def _fetch_internaldate(self):
        """Fetch the INTERNALDATE attribute from server"""
        raise FIXME

    def _fetch_size(self):
        """Fetch RFC822.SIZE attribute from server"""
        raise FIXME

    def _fetch_envelope(self):
        """Fetch ENVELOPE from IMAP server"""
        raise FIXME

    def _fetch_structure(self):
        """Make a BODYSTRUCTURE query against IMAP server"""
        # as soon as we know about the intermediate listing, we should set
        # self._payload to a list of Nones or a None
        raise FIXME

    def _return_none(self):
        """Helper that returns None as IMAP has nothing like that"""
        return None

    def _read_only(self, unixfrom):
        """Messages are immutable"""
        raise AttrReadOnlyError("IMAP messages are immutable")

    # disable modifying methods from email.Message
    attach = set_unixfrom = set_charset = set_payload = _read_only
    __setitem__ = __delitem__ = _read_only
    add_header = replace_header = del_param = _read_only
    set_default_type = set_param = set_type = set_boundary = _read_only
    # inherited methods that just don't make sense in IMAP
    get_unixfrom = _return_none
    # merge docstrings
    is_multipart.__doc__ = email.Message.Message.is_multipart.__doc__
    get_charset.__doc__ = email.Message.Message.get_charset.__doc__
    get_payload.__doc__ = email.Message.Message.get_payload.__doc__
    __len__.__doc__ = email.Message.Message.__len__.__doc__
    __contains__.__doc__ = email.Message.Message.__contains__.__doc__
    keys.__doc__ = email.Message.Message.keys.__doc__
    values.__doc__ = email.Message.Message.values.__doc__
    items.__doc__ = email.Message.Message.items.__doc__
    get.__doc__ = email.Message.Message.get.__doc__
    get_all.__doc__ = email.Message.Message.get_all.__doc__
