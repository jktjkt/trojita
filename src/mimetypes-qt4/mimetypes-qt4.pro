# mimetypes-qt4 lib project file

# include qmake-extensions file
include( qmake-extensions.pri )

isEmpty( build_mode ):BUILD_MODE = release
else:BUILD_MODE = $${build_mode}

isEmpty( build_type ):BUILD_TYPE = shared
else:BUILD_TYPE = $${build_type}

BUILD_PATH = build
BUILD_DESTDIR = build

TEMPLATE = lib
CONFIG -= release debug debug_and_release warn_on warn_off ppc x86 x86_64
CONFIG *= warn_on thread x11 windows qt $${BUILD_TYPE} $${BUILD_MODE}
QT -= gui

# Mac universal build from 10.3 & up
macx {
    CONFIG -= lib_bundle
    LIBS *= "-framework Carbon"
    
    universal {
        SDK_PATH = $$(MAC_SDKS_PATH)
        isEmpty( SDK_PATH ):SDK_PATH = /Developer/SDKs
        QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.3
        QMAKE_MAC_SDK = $${SDK_PATH}/MacOSX10.6.sdk
        #CONFIG *= x86_64
        CONFIG *= x86
        CONFIG *= ppc
        # this link is required for building the ppc port to avoid the undefined __Unwind_Resume symbol
        CONFIG( ppc ):LIBS *= -lgcc_eh
    }
}

setTarget( mimetypes-qt4 )

unix {
    UNIX_RAM_DISK = /media/ramdisk
    exists( $${UNIX_RAM_DISK} ) {
        BUILD_PATH = $${UNIX_RAM_DISK}/$${TARGET}
        BUILD_DESTDIR = $${BUILD_PATH}
    }
}

setTemporaryDirectories( $${BUILD_PATH} )
isEqual( BUILD_MODE, debug ):CONFIG *= console

DESTDIR = $${BUILD_DESTDIR}

isEqual( BUILD_TYPE, shared ) {
    win32:DLLDESTDIR = $${BUILD_DESTDIR}
}

SOURCES_PATHS = $$getFolders( io mimetypes ) $${UI_DIR} $${MOC_DIR} $${RCC_DIR}
DEPENDPATH *= $${SOURCES_PATHS}
INCLUDEPATH *= $${SOURCES_PATHS}

include( mimetypes/mimetypes.pri )

HEADERS *= io/qstandardpaths.h
SOURCES *= io/qstandardpaths.cpp

macx {
    SOURCES *= io/*_mac.c*
} else:unix {
    SOURCES *= io/*_unix.c*
} else:win32 {
    SOURCES *= io/*_win.c*
}
