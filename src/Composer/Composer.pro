CONFIG += staticlib
QT += network
TEMPLATE = lib
TARGET = Composer
SOURCES += \
    SubjectMangling.cpp \
    PlainTextFormatter.cpp \
    SenderIdentitiesModel.cpp \
    ReplaceSignature.cpp \
    Recipients.cpp \
    MessageComposer.cpp \
    ComposerAttachments.cpp \
    Submission.cpp
HEADERS += \
    SubjectMangling.h \
    PlainTextFormatter.h \
    SenderIdentitiesModel.h \
    ReplaceSignature.h \
    Recipients.h \
    MessageComposer.h \
    ComposerAttachments.h \
    Submission.h

INCLUDEPATH += ../
DEPENDPATH += ../
