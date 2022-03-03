
if(NOT RAGEL_EXECUTABLE)
    message(STATUS "Looking for ragel-c (v7.0.0.12+)")
    find_program(RAGEL_EXECUTABLE ragel-c)

    if(NOT RAGEL_EXECUTABLE)
        message(STATUS "Looking for ragel")
        find_program(RAGEL_EXECUTABLE ragel)
    endif()

    if(RAGEL_EXECUTABLE)
        execute_process(COMMAND "${RAGEL_EXECUTABLE}" -v OUTPUT_VARIABLE _version)
        string(REGEX MATCH "[0-9.]+" RAGEL_VERSION ${_version})
        set(RagelForTrojita_FOUND TRUE)
    endif()
else()
    execute_process(COMMAND "${RAGEL_EXECUTABLE}" -v OUTPUT_VARIABLE _version)
    string(REGEX MATCH "[0-9.]+" RAGEL_VERSION ${_version})
    set(RagelForTrojita_FOUND TRUE)
endif()

if(RagelForTrojita_FOUND)
    if(NOT RagelForTrojita_FIND_QUIETLY)
        message(STATUS "Found ragel: ${RAGEL_EXECUTABLE} (${RAGEL_VERSION})")
    endif()

    if(NOT RAGEL_FLAGS)
        set(RAGEL_FLAGS "-T1")
    endif()

    macro(RAGEL_PARSER SRCFILE)
        get_filename_component(SRCPATH "${SRCFILE}" PATH)
        get_filename_COMPONENT(SRCBASE "${SRCFILE}" NAME_WE)
        set(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}/${SRCBASE}.generated.cpp")
        set(INFILE "${SRCFILE}")
        set(_flags ${ARGV1})
        if(NOT _flags)
            set(_flags ${RAGEL_FLAGS})
        endif()
        if(RAGEL_VERSION VERSION_LESS "7.0.0.12")
            set(_language_param "-C")
        endif()
        add_custom_command(OUTPUT ${OUTFILE}
            COMMAND "${RAGEL_EXECUTABLE}"
            ARGS ${_language_param} ${_flags} -o "${OUTFILE}" "${INFILE}"
            DEPENDS "${INFILE}"
            COMMENT "Generating ${SRCBASE}.generated.cpp from ${SRCFILE}"
        )
    endmacro()

else()

    if(Ragel_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find ragel")
    endif()
endif()
