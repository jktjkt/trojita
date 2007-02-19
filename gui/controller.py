# -*- coding: utf-8
"""Controlling ymaplib - GUI interaction"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = "$Id$"
__all__ = ["Controller"]

class Controller:
	# requests from GUI: per-message
	def gui_message_show(self, context):
		"""User wants to see offset-th message in current context"""

	def gui_message_flag_add(self, context, flag):
		"""Message was marked by a flag"""

	def gui_message_flag_clear(self, context, flag):
		"""Message flag cleared"""

	def gui_message_copy(self, context, target):
		"""Copy a message to some target"""

	def gui_message_move(self, context, target):
		"""Move message away to something"""

	# FIXME: reply, forward, edit,...
