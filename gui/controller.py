# -*- coding: utf-8
"""Controlling ymaplib - GUI interaction"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

__revision__ = "$Id$"
__all__ = ["Controller"]

class Controller:
    # requests from GUI: per-message
    def gui_message_show(self, context):
        """User wants to see offset-th message in current context"""
        raise NotImplementedError

    def gui_message_flag_add(self, context, flag):
        """Message was marked by a flag"""
        raise NotImplementedError

    def gui_message_flag_clear(self, context, flag):
        """Message flag cleared"""
        raise NotImplementedError

    def gui_message_flags_set(self, context, flags):
        """Overwrite flags of message specified by context by flags"""
        raise NotImplementedError

    def gui_message_copy(self, context, target):
        """Copy a message to some target"""
        raise NotImplementedError

    def gui_message_move(self, context, target):
        """Move message away to something"""
        raise NotImplementedError

    def gui_message_reply(self, context):
        """User wants to reply to some mail"""
        # FIXME: kind of reply -- all, sender only,...
        # FIXME: active selection
        raise NotImplementedError

    # FIXME: forward, edit
    # FIXME: "modifying a message" (ie. creating new because RFC says message is immutable)

    def gui_mailbox_set_active(self, context):
        """Users wants to see a specified mailbox"""
        raise NotImplementedError

    def gui_mailbox_set_inactive(self, context):
        """User no longer has interest in that mailbox"""
        raise NotImplementedError

    def gui_mailbox_create(self, context, name, with_children):
        """Create new mailbox whose parent is specified in context

        If server doesn't support mailboxes with both mailboxes and data,
        with_children is used -- if it's true, mailbox will support only sub-mailboxes."""
        raise NotImplementedError

    def gui_mailbox_move(self, context, target):
        """Move mailbox specified by context to target"""
        # FIXME: is "target" a parent or what?
        raise NotImplementedError

    def gui_mailbox_delete(self, context):
        """Permanently delete specified mailbox"""
        raise NotImplementedError

    def gui_mailbox_expunge(self, context):
        """Expunge messages from specified mailbox"""
        raise NotImplementedError

    def gui_mailbox_check_for_new(self, context):
        """Force check for new messages in a specified mailbox"""
        raise NotImplementedError

    # FIXME: sort, thread, threading/flat view, searching,...
    # FIXME: list of mailboxes, subscriptions,...

    def __init__(self):
        # FIXME: create pool of parsers/connections,...
        pass
