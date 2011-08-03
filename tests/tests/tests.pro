TEMPLATE = subdirs
SUBDIRS  = \
    test_RingBuffer \
    test_Imap_LowLevelParser test_Imap_Message test_Imap_Parser_parse \
    test_Imap_Responses test_rfccodecs test_Imap_Model \
    test_Imap_Tasks_OpenConnection \
    test_Imap_Tasks_ListChildMailboxes \
    test_Imap_Tasks_CreateMailbox \
    test_Imap_Tasks_DeleteMailbox \
    test_Imap_Tasks_ObtainSynchronizedMailbox \
    test_Imap_Idle \
    test_Imap_SelectedMailboxUpdates \
    test_Imap_DisappearingMailboxes \
    test_Imap_Threading

# At first, we define the "check" target which simply propagates the "check" call below
check.target = check
check.CONFIG = recursive
QMAKE_EXTRA_TARGETS += check

# And then wrap it in a "test" command which calls make with nice arguments
unix:test.commands = TESTARGS=-silent $(MAKE) -s check
QMAKE_EXTRA_TARGETS += test
