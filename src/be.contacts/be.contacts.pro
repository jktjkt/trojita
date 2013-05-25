TARGET = be.contacts

INCLUDEPATH += ../
DEPENDPATH += ../

SOURCES = \
    main.cpp

CONFIG += qt staticlib
QT += gui core
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

trojita_libs = AbookAddressbook
myprefix = ../
include(../linking.pri)
include(../../install.pri)
