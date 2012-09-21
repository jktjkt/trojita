TARGET = trojitaqnamwebviewplugin
CONFIG += qt plugin
QT += webkit declarative
TEMPLATE = lib
include(../../configh.pri)

DESTDIR = ../trojita-tp/net/flaska/QNAMWebView

# would be cool to have this one inherited from the main project...
isEmpty(OUTPUT_DIR) {
    OUTPUT_DIR = /opt/trojita-tp/bin/
    DESTDIR = net/flaska/QNAMWebView
}

QMLDIRFILE = $${_PRO_FILE_PWD_}/qmldir
copy2build.input = QMLDIRFILE
copy2build.output = ./$$DESTDIR/qmldir

!contains(TEMPLATE_PREFIX, vc):copy2build.variable_out = PRE_TARGETDEPS
copy2build.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
copy2build.name = COPY ${QMAKE_FILE_IN}
copy2build.CONFIG += no_link
QMAKE_EXTRA_COMPILERS += copy2build

SOURCES += qdeclarativewebview.cpp plugin.cpp
HEADERS += qdeclarativewebview_p.h plugin.h

qmldir.files += $$PWD/qmldir
qmldir.path += $$OUTPUT_DIR/$$DESTDIR
target.path += $$OUTPUT_DIR/$$DESTDIR

INSTALLS += target qmldir
