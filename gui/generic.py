# -*- coding: utf-8
"""Parent for all GUIs"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'


class Context:
	"""Specifying a context for various Generic methods"""

	def __init__(self, account=None, mailbox=None, msg_offset=None, part=None):
		self.account = account
		self.mailbox = mailbox
		self.msg_offset = msg_offset
		self.part = part

	def __repr__(self):
		return "<trojita.gui.generic.Context: account %s, mailbox %s, msg_offset %s, part %s>" \
			% (str(self.account), str(self.mailbox), str(self.msg_offset),
			   str(self.part))

	def check(self, *args):
		"""Check a "context" for required items"""
		for item in args:
			if item == "account":
				if self.account is None:
					raise ValueError("Context lacks account specification")
			elif item == "mailbox":
				if self.mailbox is None:
					raise ValueError("Context lacks mailbox specification")
			elif item == "msg_offset":
				if self.msg_offset is None:
					raise ValueError("Context lacks msg_offset specification")
			elif item == "part":
				if self.part is None:
					raise ValueError("Context lacks part specification")
			else:
				raise ValueError("Unknown item to check: '%s'" % str(item))


class Generic:
	"""Parent that all GUI implementations should inherit from and override all methods beginning with one underscore followed by a letter"""


	# global events
	def set_task_progress(self, task, progress, info):
		"""Task's progress and textual information are updated"""
		return self._set_task_progress(task, progress, info)
	
	def alert(self, context, text):
		"""Display the server's ALERT response (mandatory by RFC}
		
		text is the unprocessed text from server, context has 
		additional information that can hint wtf has happened"""
		context.check("account")
		return self._alert(context, text)

	def notice(self, context, data):
		"""Inform about miscellanous events, usefull for debugging"""
		return self._notice(context, data)

	# per account events
	def set_account_mailboxes(self, context, mailboxes):
		"""Show a view of mailboxes for account"""
		context.check("account")
		# FIXME: how to support an incomplete tree?
		return self._set_account_mailboxes(context, mailboxes)

	# per mailbox events
	def set_mailbox_structure(self, context, tree):
		"""New layout of message nesting in mailbox"""
		context.check("account", "mailbox")
		return self._set_mbox_structure(context, tree)

	# per message events
	def set_msg_envelope(self, context, envelope):
		"""Update envelope information for msg_offset-th message in current view"""
		context.check("account", "mailbox", "msg_offset")
		return self._set_msg_enveleope(context, envelope)

	def set_msg_structure(self, context, structure):
		"""Structure of current message"""
		context.check("account", "mailbox", "msg_offset", "envelope")
		return self._set_msg_structure(context, structure)

	def set_msg_content(self, context, data):
		"""Data for message

		Updates part of the message with relevant data (string).
		Content-type is already known from set_msg_structure event."""
		context.check("account", "mailbox", "msg_offset", "part")
		return self._set_msg_content(context, data)

	# system stuff
	def __todo(self):
		"""A placeholder for method that isn't implemented yet"""
		raise NotImplementedError

	def __init__(self, controller):
		self._controller = controller

	_set_task_progress = __todo
	_alert = __todo
	_notice = __todo
	_set_account_mailboxes = __todo
	_set_mailbox_structure = __todo
	_set_msg_envelope = __todo
	_set_msg_structure = __todo
	_set_msg_content = __todo
