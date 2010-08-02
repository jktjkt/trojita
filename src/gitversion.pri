unix:exists($$join(GITVERSION_PREFIX,,,/.git/HEAD)) {
    DEFINES += HAS_GITVERSION
    GITVERSION_FILES = main.cpp
    gitversion.name = Creating GIT version
    gitversion.input = GITVERSION_FILES
    gitversion.depends = FORCE
    gitversion.commands = echo \"const char* gitVersion=\\\"\"\"`cd $$IN_PWD;git describe --dirty --long || git describe --long`\"\"\\\";\" > ${QMAKE_FILE_BASE}.version.cpp;
    gitversion.output = ${QMAKE_FILE_BASE}.version.cpp
    gitversion.variable_out = SOURCES
    gitversion.CONFIG += no_link
    QMAKE_EXTRA_COMPILERS += gitversion
}
