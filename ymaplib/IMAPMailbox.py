# -*- coding: utf-8
"""Interface to an IMAP mailbox"""

# Copyright (c) Jan Kundrát <jkt@flaska.net>, 2006 - 2007

import cPickle

__revision__ = '$Id$'

class IMAPMailbox:
    """Interface to an IMAP mailbox"""

    def __init__(self, name, connection, want_rw=False, create=False,
                  timeout=None, uid_cache_file=None, data_cache_file=None):
        self.name = name
        self._conn = connection
        self.want_rw = want_rw
        self.create = create
        self.timeout = timeout
        self._clear_attrs()
        self._load_uid_cache(uid_cache_file)
        self._open()
        if self._conn.mailbox == self.name:
            self.data_cache = {}
            self._sync_uid_cache()
            self._load_data_cache(data_cache_file)

    def _load_uid_cache(self, file=None):
        """Load seq -> UID mapping from persistent storage"""
        if file is not None:
            (self.uid_cache, self.uidnext, self.uidvalidity, self.exists) = cPickle.load(file)
        else:
            self.uid_cache = {}
            self.old_uidnext = None
            self.old_uidvalidity = None
            self.old_exists = None

    def _save_uid_cache(self, file):
        """Save the UID cache to a file object"""
        cPickle.dump((self.uid_cache, self.uidnext, self.uidvalidity, self.exists), file)

    def _load_data_cache(self, file=None):
        """Load a data cache from persistent storage"""
        if file is not None:
            self.data_cache = cPickle.load(file)
        else:
            self.data_cache = {}

    def _clear_attrs(self):
        """Clear current attributes"""
        self.rw = None
        self.uidnext = None
        self.uidvalidity = None
        self.unseen = None
        self.exists = None
        self.recent = None
        self.permanentflags = ()
        self.flags = ()

    def _open(self):
        """Ask the IMAP server to open the mailbox, catch all the important stuff"""
        if self.name != self._conn.mailbox:
            if self.want_rw:
                tag = self._conn.cmd_select(self.name)
            else:
                tag = self._conn.cmd_examine(self.name)
            while 1:
                response = self._get_response()

                if response.response_code[0] == 'READ-ONLY':
                    self.rw = False
                elif response.response_code[0] == 'READ-WRITE':
                    self.rw = True
                elif response.response_code[0] == 'UIDVALIDITY':
                    self.uidvalidity = int(response.response_code[1])
                elif response.response_code[0] == 'UIDNEXT':
                    self.uidnext = int(response.response_code[1])
                elif response.response_code[0] == 'UNSEEN':
                    self.unseen = int(response.response_code[1])
                elif response.response_code[0] == 'PERMANENTFLAGS':
                    if not isinstance(response.response_code[1], tuple):
                        raise TypeError(
                            "PERMANENTFLAGS response code isn't a tuple")
                    self.permanentflags = response.response_code[1]

                if response.kind == 'FLAGS':
                    if not isinstance(response.data, tuple):
                        raise TypeError("FLAGS response isn't a tuple")
                    self.flags = response.data
                elif response.kind == 'EXISTS':
                    self.exists = int(response.data)
                elif response.kind == 'RECENT':
                    self.recent = int(response.data)

                if self.rw is None:
                    self.rw = True

                if response.tag == tag:
                    if response.kind != 'OK':
                        self._conn.mailbox = None
                        self._clear_attrs()
                    else:
                        self._conn.mailbox = self.name
                    break

    def _sync_uid_cache(self):
        """Be smart and figure out what we have to sync"""
        if self.uidvalidity is None or self.exists is None or self.uidnext is None:
            # no cached data
            self._wipe()
            return

        if self.uidvalidity != self.old_uidvalidity:
            # UIDVALIDITY change
            self._wipe()
            return

        if self.old_uidnext > self.uidnext:
            # FIXME: this can't happen... :)
            self._wipe()
            return
        elif self.old_uidnext == self.uidnext:
            # nothing new
            if self.exists_old == self.exists:
                # old state
                return
            elif self.exists_old < self.exists:
                # FIXME: this can't happen :)
                self._wipe()
            else:
                # something got deleted
                self._sync_uid_cache_force()
        else:
            # soemthing was added, but we don't know if it remained...
            if self.old_exists == self.exists:
                # we don't know anything
                self._sync_uid_cache_force()
            elif self.old_exists < self.exists:
                d_exists = self.exists - self.old_exists
                d_uidnext = self.uidnext - self.old_uidnext
                if d_exists == d_uidnext:
                    # only new messages
                    self._get_uids_by_seq(self.old_exists + 1, self.exists)
                elif d_exists > d_uidnext:
                    # FIXME: again, this can't happen
                    self._wipe()
                else:
                    # who knows...
                    self._sync_uid_cache_force()
            else:
                # we don't know anything
                self._sync_uid_cache_force()

    def _sync_uid_cache_force(self):
        """Smart synchronization of uid cache itself"""
        # FIXME: as a temporal workaround, we just fetch the complete mapping...
        self._get_uids_by_seq(1, self.exists)
        return
        # FIXME: this is probably broken...
        sorted_sequences = self.uid_cache.keys()
        sorted_sequences.sort()
        current_middle = sorted_sequences[len(sorted_sequences) / 2]
        self._sync_uid_cache_job(current_middle, sorted_sequences)

    def _sync_uid_cache_job(self, current_middle, sorted_sequences):
        """Smart synchronization of uid cache itself -- helper"""
        print "_sync_uid_cache_job: #%d of %d" % (current_middle, len(sorted_sequences))
        # choose some seq which is already cached and lies approximately in the middle
        if current_middle >= len(sorted_sequences):
            print "# we're already at the end"
            return
        if self._get_uid_by_seq(sorted_sequences[current_middle], True) == \
           self.uid_cache[sorted_sequences[current_middle]]:
            print "# this value was correct, now let's move towards the end"
            self._sync_uid_cache_job(
             current_middle + (len(sorted_sequences) - current_middle) / 2.0 + 2, 
             sorted_sequences)
        else:
            print '# throw the current part away'
            for seq in sorted_sequences[current_middle:]:
                del self.uid_cache[seq]

    def  _get_response(self):
        return self._conn.get(self.timeout)

    def _wipe(self):
        """Wipe all the caches"""
        self.uid_cache = {}
        self.data_cache = {}

    def _get_uid_by_seq(self, seq, forbid_cache=False):
        try:
            if forbid_cache:
                raise KeyError
            return self.uid_cache[seq]
        except KeyError:
            tag = self._conn.cmd_uid_search((str(seq),))
            uid = None
            while 1:
                response = self._get_response()
                if response.kind == 'SEARCH':
                    uid = response.data[0]
                if response.tag == tag:
                    break
            if uid is not None and not forbid_cache:
                # FIXME: cache size checking...
                self.uid_cache[seq] = uid
            return uid

    def _get_uids_by_seq(self, start, end):
        result = []
        todo = []
        seq = range(start, end + 1)
        for number in seq:
            try:
                result.append(self.uid_cache[number])
            except KeyError:
                todo.append(number)
        if len(todo):
            tag = self._conn.cmd_uid_search((self._optimized_sequence(todo),))
            while 1:
                response = self._get_response()
                if response.kind == 'SEARCH':
                    uids = list(response.data)
                if response.tag == tag:
                    break
            i = 0
            uids.sort()
            for uid in uids:
                self.uid_cache[seq[i]] = uid
                i += 1
            return uids
        else:
            return result

    @classmethod
    def _optimized_sequence(cls, data):
        """Return and IMAP string sequence matching given (sorted) range"""
        # FIXME: now we're really dumb and include more than we really need...
        return "%d:%d" % (data[0], data[len(data) - 1])

    def __repr__(self):
        s = '<ymaplib.IMAPMailbox: '
        if self.name == self._conn.mailbox:
            s += 'connected'
        else:
            s += 'not connected'
        s += ', "%s", ' % (self.name)
        if self.rw:
            s += 'RW, '
        else:
            s += 'RO, '
        return s + ('UIDNEXT %s, UIDVALIDITY %s, UNSEEN %s, EXISTS %s, ' +
             'RECENT %s, FLAGS (%s), PERMANENTFLAGS (%s)>') % (self.uidnext,
             self.uidvalidity, self.unseen, self.exists, self.recent,
             ' '.join(self.flags), ' '.join(self.permanentflags))
