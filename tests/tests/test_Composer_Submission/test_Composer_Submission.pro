TARGET = test_Composer_Submission

trojita_libs = Composer Common MSA
lessThan(QT_MAJOR_VERSION, 5) {
    trojita_libs += mimetypes-qt4
}
myprefix = ../../../src/
include(../../../src/linking.pri)

include(../tests.pri)
