TEMPLATE = subdirs
SUBDIRS  = \
    test_LibMailboxSync \
    tests
CONFIG += ordered

# At first, we define the "check" target which simply propagates the "check" call below
check.target = check
check.CONFIG = recursive
QMAKE_EXTRA_TARGETS += check

# And then wrap it in a "test" command which calls make with nice arguments
unix:test.commands = TESTARGS=-silent $(MAKE) -s check
QMAKE_EXTRA_TARGETS += test
