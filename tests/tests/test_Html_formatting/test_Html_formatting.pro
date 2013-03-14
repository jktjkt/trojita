greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets webkitwidgets
} else {
    QT += webkit
}
TARGET = test_Html_formatting
include(../tests.pri)

trojita_libs = Composer Common
myprefix = ../../../src/
include(../../../src/linking.pri)
