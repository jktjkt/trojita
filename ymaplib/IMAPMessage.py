# -*- coding: utf-8
"""RFC822 message stored on an IMAP server"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'

import email.Message
from exceptions import AttrReadOnlyError

class IMAPMessage(email.Message.Message):
    """RFC822 message stored on an IMAP server"""
    # FIXME: add actual code, this is only a template

    def __init__(self, client, namespace, mailbox, uidvalidity, uid):
        """Create new instance tied to a specified message in given IMAPClient

        client -- IMAPClient instance
        namespace, mailbox -- malibox specification
        uidvalidity, uid -- message specification
        """

        email.Message.Message.__init__(self)
        self._client = client
        self._namespace = namespace
        self._mailbox = mailbox
        self._uidvalidity = uidvalidity
        self._uid = uid
        self._flags = None
        self._internaldate = None
        self._size = None
        self._envelope = None
        # override email.Message.Message stuff
        self._headers = None

    def _fetch_flags(self):
        raise FIXME

    def _fetch_internaldate(self):
        raise FIXME

    def _fetch_size(self):
        raise FIXME

    def _fetch_envelope(self):
        raise FIXME

    def _return_none(self):
        """Helper that returns None as IMAP has nothing like that"""
        return None

    def _read_only(self, unixfrom):
        """Messages are immutable"""
        raise AttrReadOnlyError("IMAP messages are immutable")

    # modifying methods from email.Message
    attach = set_unixfrom = set_charset = set_payload = _read_only
    __set_item__ = __del_item__ = _read_only
    add_header = replace_header = _read_only
    set_default_type = set_param = set_type = set_boundary = _read_only
    # inherited methods that just don't make sense in IMAP
    get_unixfrom = _return_none
