
unix {
    isEmpty(PREFIX) {
        PREFIX = /usr/local
    }
    BINDIR = $$PREFIX/bin

    INSTALLS += target
    target.path = $$BINDIR

    DATADIR = $$PREFIX/share
}

