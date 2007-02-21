# -*- coding: utf-8
"""Parent for all GUIs"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'

import curses
from generic import Generic
import time

class Curses(Generic):
    def __init__(self, controller):
        GenericGUI.__init__(self, controller)
        self._stdscr = curses.initscr()
        curses.noecho()
        curses.cbreak()

        # create subwindows
        #self._mbox_list = self._stdscr.

    def __del__(self):
        # curses cleanup
        curses.nocbreak()
        self.stdscr.keypad(0)
        curses.echo()
        curses.endwin()
        #GenericGUI.__del__(self)

if __name__ == "__main__":
    g = CursesGUI(None)
    time.sleep(1)
    del w
