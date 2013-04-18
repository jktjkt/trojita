QT -= gui
TEMPLATE = lib
CONFIG += staticlib
TARGET = mimetypes-qt4

INCLUDEPATH += io

include( mimetypes/mimetypes.pri )

HEADERS *= io/qstandardpaths.h
SOURCES *= io/qstandardpaths.cpp

macx {
    SOURCES += io/qstandardpaths_mac.cpp
} else:unix {
    SOURCES += io/qstandardpaths_unix.cpp
} else:win32 {
    SOURCES += io/qstandardpaths_win.cpp
} else:os2 {
    SOURCES += io/qstandardpaths_os2.cpp
} else {
    error("Support for QStandardPaths emulation is not available on your platform. Please file a bug.")
}
