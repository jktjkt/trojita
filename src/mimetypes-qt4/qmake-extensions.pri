###########################################################################################
##      Created using Monkey Studio IDE v1.9.0.1 (1.9.0.1)
##
##  Author    : Filipe Azevedo aka Nox P@sNox <pasnox@gmail.com>
##  Project   : qmake-extensions.pri
##  FileName  : qmake-extensions.pri
##  Date      : 2012-07-28T13:13:40
##  License   : LGPL3
##  Comment   : Creating using Monkey Studio RAD
##  Home Page : https://github.com/pasnox/qmake-extensions
##
##  This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
##  WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
##
###########################################################################################

Q_HOST_OS = $${QMAKE_HOST.os}
Q_LOWER_HOST_OS = $$lower( $${Q_HOST_OS} )

win32 {
    !isEqual(  Q_LOWER_HOST_OS, "windows" ) {
        # we are cross building for windows
        CONFIG *= cb_win32
    }
}

macx {
    !isEqual(  Q_LOWER_HOST_OS, "darwin" ) {
        # we are cross building for mac os x
        CONFIG *= cb_mac
    }
}

# the build target os
Q_TARGET = $${Q_HOST_OS}
# the build target arch
Q_ARCH = $${QT_ARCH}

cb_win32 {
  Q_TARGET = Windows
}

cb_mac {
  Q_TARGET = Darwin
}

Q_TARGET_ARCH = "$${Q_TARGET}-$${Q_ARCH}"

# Some useful variables that can't be easily added in variable values directly
Q_NULL =
Q_BACK_SLASH = "\\"
Q_SLASH = "/"
Q_QUOTE = "\""
Q_DOLLAR = "\\$"
Q_OPENING_BRACE = "\\{"
Q_CLOSING_BRACE = "\\}"

# include functions
isEmpty( translations_pass ) {
    include( $$PWD/functions.pri )
} else {
    include( $$PWD/fake-functions.pri )
}
