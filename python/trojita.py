#!/usr/bin/python
# -*- coding: utf-8
"""Trojita MUA"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = "$Id$"

from optparse import OptionParser

if __name__ == "__main__":
    opt = OptionParser()
    opt.add_option("--gui", metavar="GUI",
                   choices=("dumb", "curses", "qt", "kde"),
                   help="what gui to start (default: %default)",
                   default="dumb")
    (options, args) = opt.parse_args()
    # FIXME: GUI specification
    # FIXME: optional action
else:
    raise "You probably want to import something else, not this wrapper"
