# -*- coding: utf-8
"""Parent for all GUIs"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = '$Id$'

class GenericGUI:
	# global events
	def set_task_progress(self, task, progress, info)
		"""Task's progress and textual information are updated"""
		return self._set_task_progress(task, progress, info)
	
	def alert(self, text, context)
		"""Display the server's ALERT response (mandatory by RFC}
		
		text is the unprocessed text from server, context has 
		additional information that can hint wtf has happened"""
		return self._alert(text, context)

	# per account events
	def set_account_mailboxes(self, account, mailboxes):

	# per mailbox events
	def set_mailbox_structure(self, account, mailbox, tree):
		"""New layout of message nesting in mailbox"""
		return self._set_mbox_structure(account, mailbox, tree)

	# per message events
	def set_msg_envelope(self, account, mailbox, msg_offset, envelope):
		"""Update envelope information for msg_offset-th message in current view"""
		return self._set_msg_enveleope(account, mailbox, msg_offset, envelope)

	def set_msg_structure(self, account, mailbox, msg_offset, envelope, structure):
		"""Structure of current message"""
		return self._set_msg_structure(account, mailbox, msg_offset, envelope, structure)

	def set_msg_content(self, account, mailbox, msg_offset, part, data):
		"""Data for message

		Updates part of the message with relevant data (string).
		Content-type is already known from set_msg_structure event."""
		return self._set_msg_content(account, mailbox, msg_offset, part, data)

	# system stuff
	def __todo(self):
		"""A placeholder for method that isn't implemented yet"""
		raise NotImplementedError

	def __init__(self, controller):
		self._controller = controller

	_set_msg_envelope = __todo
	_set_mbox_structure = __todo
	_set_msg_structure = __todo
	_set_msg_content = __todo
