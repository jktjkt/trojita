file(READ ${SOURCE_DIR}/src/trojita-version TROJITA_VERSION)
string(REPLACE "\n" "" TROJITA_VERSION ${TROJITA_VERSION})

if(GIT_EXECUTABLE AND EXISTS ${SOURCE_DIR}/.git)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --dirty --long --always
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE TROJITA_GIT_VERSION)
    string(REPLACE "\n" "" TROJITA_GIT_VERSION ${TROJITA_GIT_VERSION})
endif()

# Windows resources using four 16bit integers as version number, so prepare these numbers in VERNUMX variables from VERSION
macro(set_win_rc_vernum VERSION VERNUM1 VERNUM2 VERNUM3 VERNUM4)
    string(REGEX REPLACE "[^0-9-]" "_" VERNUM ${VERSION})
    if(VERNUM MATCHES "^_*([0-9]+)_+([0-9]+)_+([0-9]+)_+([0-9]+)")
        set(${VERNUM1} ${CMAKE_MATCH_1})
        set(${VERNUM2} ${CMAKE_MATCH_2})
        set(${VERNUM3} ${CMAKE_MATCH_3})
        set(${VERNUM4} ${CMAKE_MATCH_4})
    elseif(VERNUM MATCHES "^_*([0-9]+)_+([0-9]+)_+([0-9]+)")
        set(${VERNUM1} ${CMAKE_MATCH_1})
        set(${VERNUM2} ${CMAKE_MATCH_2})
        set(${VERNUM3} ${CMAKE_MATCH_3})
        set(${VERNUM4} "0")
    elseif(VERNUM MATCHES "^_*([0-9]+)_+([0-9]+)")
        set(${VERNUM1} ${CMAKE_MATCH_1})
        set(${VERNUM2} ${CMAKE_MATCH_2})
        set(${VERNUM3} "0")
        set(${VERNUM4} "0")
    elseif(VERNUM MATCHES "^_*([0-9]+)")
        set(${VERNUM1} ${CMAKE_MATCH_1})
        set(${VERNUM2} "0")
        set(${VERNUM3} "0")
        set(${VERNUM4} "0")
    else()
        set(${VERNUM1} "0")
        set(${VERNUM2} "0")
        set(${VERNUM3} "0")
        set(${VERNUM4} "0")
    endif()
endmacro()

set_win_rc_vernum(${TROJITA_VERSION} TROJITA_VERNUM1 TROJITA_VERNUM2 TROJITA_VERNUM3 TROJITA_VERNUM4)
set(TROJITA_VERSION_H
    "#define TROJITA_VERSION \"${TROJITA_VERSION}\"\n"
    "#define TROJITA_VERNUM1 ${TROJITA_VERNUM1}\n"
    "#define TROJITA_VERNUM2 ${TROJITA_VERNUM2}\n"
    "#define TROJITA_VERNUM3 ${TROJITA_VERNUM3}\n"
    "#define TROJITA_VERNUM4 ${TROJITA_VERNUM4}\n"
)

if(TROJITA_GIT_VERSION)
    set_win_rc_vernum(${TROJITA_GIT_VERSION} TROJITA_GIT_VERNUM1 TROJITA_GIT_VERNUM2 TROJITA_GIT_VERNUM3 TROJITA_GIT_VERNUM4)
    set(TROJITA_GIT_VERSION_H
        "#define TROJITA_GIT_VERSION \"${TROJITA_GIT_VERSION}\"\n"
        "#define TROJITA_GIT_VERNUM1 ${TROJITA_GIT_VERNUM1}\n"
        "#define TROJITA_GIT_VERNUM2 ${TROJITA_GIT_VERNUM2}\n"
        "#define TROJITA_GIT_VERNUM3 ${TROJITA_GIT_VERNUM3}\n"
        "#define TROJITA_GIT_VERNUM4 ${TROJITA_GIT_VERNUM4}\n"
    )
else()
    set(TROJITA_GIT_VERSION_H "")
endif()

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/trojita-version.h.in ${TROJITA_VERSION_H})
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/trojita-version.h.in ${CMAKE_CURRENT_BINARY_DIR}/trojita-version.h)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/trojita-git-version.h.in ${TROJITA_GIT_VERSION_H})
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/trojita-git-version.h.in ${CMAKE_CURRENT_BINARY_DIR}/trojita-git-version.h)

if(NSIS)
    if(TROJITA_GIT_VERSION)
        string(REPLACE "GIT_" "" TROJITA_VERSION_NSI ${TROJITA_GIT_VERSION_H})
    else()
        set(TROJITA_VERSION_NSI ${TROJITA_VERSION_H})
    endif()
    string(REPLACE "#" "!" TROJITA_VERSION_NSI ${TROJITA_VERSION_NSI})
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/trojita-version.nsi.in ${TROJITA_VERSION_NSI})
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/trojita-version.nsi.in ${CMAKE_CURRENT_BINARY_DIR}/trojita-version.nsi)
endif()
