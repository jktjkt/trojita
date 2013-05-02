TARGET = be.contacts
HEADERS = be.contacts.h
FORMS = be.contacts.ui
SOURCES = be.contacts.cpp
CONFIG += qt
QT += gui core
DEFINES += VERSION=0.1
target.path += $$[QT_INSTALL_BINS]
INSTALLS += target

!mac {
unix {
    DATADIR = $$[QT_INSTALL_PREFIX]/share
    DEFINES += "DATADIR=$$DATADIR"
    INSTALLS += desktop icon postinstall

    desktop.path = $$DATADIR/applications
    desktop.files += piQtureGLide.desktop

    icon.path = $$DATADIR/icons/hicolor/128x128/apps
    icon.files += piQtureGLide.png

    postinstall.path =  $$[QT_INSTALL_BINS]
    postinstall.extra = update-desktop-database
}
}
