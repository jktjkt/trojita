
unix {
    isEmpty(PREFIX) {
        PREFIX = /usr/local
    }
    BINDIR = $$PREFIX/bin

    INSTALLS += target
    target.path = $$BINDIR

    DATADIR = $$PREFIX/share
    PKGDATADIR = $$DATADIR/trojita

    # Share the information about the setup that qmake knows with the application
    DEFINES += DATADIR=\\\"$$DATADIR\\\" PKGDATADIR=\\\"$$PKGDATADIR\\\"
}

