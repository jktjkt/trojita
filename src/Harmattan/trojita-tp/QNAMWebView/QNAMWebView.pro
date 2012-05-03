TARGET = trojitaqnamwebviewplugin
CONFIG += qt plugin
QT += webkit declarative
TEMPLATE = lib

DESTDIR = net/flaska/trojita

#isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../..
isEmpty(OUTPUT_DIR): OUTPUT_DIR = .

QMLDIRFILE = $${_PRO_FILE_PWD_}/qmldir
copy2build.input = QMLDIRFILE
copy2build.output = $$OUTPUT_DIR/$$DESTDIR/qmldir

!contains(TEMPLATE_PREFIX, vc):copy2build.variable_out = PRE_TARGETDEPS
copy2build.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
copy2build.name = COPY ${QMAKE_FILE_IN}
copy2build.CONFIG += no_link
QMAKE_EXTRA_COMPILERS += copy2build

SOURCES += qdeclarativewebview.cpp plugin.cpp
HEADERS += qdeclarativewebview_p.h

qmldir.files += $$PWD/qmldir
qmldir.path +=  $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

#INSTALLS += target qmldir
INSTALLS += qmldir
