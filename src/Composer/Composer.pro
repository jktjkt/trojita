CONFIG += staticlib
TEMPLATE = lib
TARGET = Composer
SOURCES += \
    SubjectMangling.cpp \
    PlainTextFormatter.cpp \
    SenderIdentitiesModel.cpp \
    ReplaceSignature.cpp
HEADERS += \
    SubjectMangling.h \
    PlainTextFormatter.h \
    SenderIdentitiesModel.h \
    ReplaceSignature.h \
    Recipients.h

INCLUDEPATH += ../
DEPENDPATH += ../
