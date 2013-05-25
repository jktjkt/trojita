TARGET = AbookAddressbook
TEMPLATE = lib

INCLUDEPATH += ../
DEPENDPATH += ../

HEADERS = \
    be.contacts.h \
    AbookAddressbook.h

SOURCES = \
    be.contacts.cpp \
    AbookAddressbook.cpp

FORMS = \
    be.contacts.ui \
    onecontact.ui

CONFIG += qt staticlib

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}
