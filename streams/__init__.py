# -*- coding: utf-8
"""Supports a wide range of different streams.

For details about what a "stream" is please see the Stream class."""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = "$Id$"
__al__ = ["ProcessStream", "TCPStream", "OpenSSLStream"]


from Stream import Stream
from common import default_timeout
from PollableStream import PollableStream
from TCPStream import TCPStream
from GenericSSLStream import GenericSSLStream
from OpenSSLStream import OpenSSLStream
from ProcessStream import ProcessStream
