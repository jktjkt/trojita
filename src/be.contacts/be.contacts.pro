TARGET = be.contacts
HEADERS = be.contacts.h
FORMS = \
    be.contacts.ui \
    onecontact.ui
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
}
}
