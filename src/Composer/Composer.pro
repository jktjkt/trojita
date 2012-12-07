CONFIG += staticlib
TEMPLATE = lib
TARGET = Composer
SOURCES += \
    SubjectMangling.cpp \
    PlainTextFormatter.cpp \
    SenderIdentitiesModel.cpp
HEADERS += \
    SubjectMangling.h \
    PlainTextFormatter.h \
    SenderIdentitiesModel.h

INCLUDEPATH += ../
DEPENDPATH += ../
