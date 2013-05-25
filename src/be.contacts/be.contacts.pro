TARGET = be.contacts

INCLUDEPATH += ../
DEPENDPATH += ../

HEADERS = \
    be.contacts.h \

FORMS = \
    be.contacts.ui \
    onecontact.ui

SOURCES = \
    be.contacts.cpp \
    main.cpp

CONFIG += qt staticlib
QT += gui core
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}
DEFINES += VERSION=0.1
target.path += $$[QT_INSTALL_BINS]
INSTALLS += target

!mac {
unix {
    DATADIR = $$[QT_INSTALL_PREFIX]/share
    DEFINES += "DATADIR=$$DATADIR"
}
}

trojita_libs = AbookAddressbook
myprefix = ../
include(../linking.pri)
