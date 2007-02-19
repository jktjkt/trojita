# -*- coding: utf-8
"""IMAP4rev1 client"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__version__ = "0.1-svn"
__revision__ = "$Id$"

__all__ = ["gui", "streams", "ymaplib", "banner"]

def get_revisions():
	import sys, types
	import streams, ymaplib, gui, gui.controller
	import gui.generic, gui.curses_simple
	revisions = {}
	for _module in sys.modules.values():
		if (type(_module) == types.ModuleType
		  and _module.__name__ not in revisions
		  and (
		    _module.__name__.startswith("ymaplib.") or
		    _module.__name__.startswith("gui.") or
		    _module.__name__.startswith("streams.") or
		    _module.__name__.startswith("trojita.") or
		    _module.__name__ in ("streams", "gui", "ymaplib", "imap4utf7"))):
		    	if _module.__name__.startswith("trojita."):
				revisions[_module.__name__] = _module.__revision__
			else:
				revisions["trojita." + _module.__name__] = _module.__revision__
	return revisions

def banner():
	buf = ["trojita version %s:" % __version__,
		" trojita:\t\t%s" % __revision__]
	revisions = get_revisions().items()
	revisions.sort()
	for (name, rev) in revisions:
		buf.append(" %s:\t%s" % (name, rev))
	return "\n".join(buf)

if __name__ == "__main__":
	print banner()
