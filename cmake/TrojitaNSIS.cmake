# Create NSIS Installer, check for all dependent DLL libraries and include them into Installer

### Generate list of files for inclusion into installer ###

# Include trojita executable
get_target_property(TROJITA_EXE_PATH trojita LOCATION)
get_filename_component(TROJITA_EXE ${TROJITA_EXE_PATH} NAME)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "!define TROJITA_EXE_PATH \"${TROJITA_EXE_PATH}\"\n!define TROJITA_EXE \"${TROJITA_EXE}\"\n")

# Include trojita icon
get_filename_component(TROJITA_ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src/icons/trojita.ico REALPATH)
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "!define TROJITA_ICON_PATH \"${TROJITA_ICON_PATH}\"\n")

set(TROJITA_INSTALL_FILES "")

set(TROJITA_LIBRARIES "")
set(TROJITA_LIBRARIES_PATHS "")

if(WITH_QT5)
    # TODO: List all required Qt5 libraries
    message(SEND_ERROR "Windows NSIS installer is not supported for Qt5 version yet")
else()
    # All Qt4 libraries which are required for Trojita
    set(TROJITA_QT_LIBRARIES QT_QTNETWORK_LIBRARY QT_QTSQL_LIBRARY QT_QTGUI_LIBRARY QT_QTCORE_LIBRARY QT_QTWEBKIT_LIBRARY)
    if(WITH_DBUS)
        list(APPEND TROJITA_QT_LIBRARIES QT_QTDBUS_LIBRARY QT_QTXML_LIBRARY)
        list(APPEND TROJITA_LIBRARIES libdbus-1-3.dll)
    endif()
    foreach(LIBRARY ${TROJITA_QT_LIBRARIES})
        if(${LIBRARY}_RELEASE AND ${LIBRARY}_DEBUG AND (CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE))
            if(${LIBRARY}_DEBUG AND CMAKE_BUILD_TYPE STREQUAL "Debug")
                list(APPEND TROJITA_LIBRARIES ${${LIBRARY}_DEBUG})
            else()
                list(APPEND TROJITA_LIBRARIES ${${LIBRARY}_RELEASE})
            endif()
        else()
            list(APPEND TROJITA_LIBRARIES ${${LIBRARY}})
        endif()
    endforeach()
endif()

# Check if QtWebKit depends on external system sqlite3 library
# NOTE: Only pkgconfig file in Libs.private section contains this information
#       Output of Libs.private is in list of static libraries needed to link
include(FindPkgConfig)
pkg_check_modules(QTWEBKIT QUIET QtWebKit)
if(QTWEBKIT_STATIC_LIBRARIES AND QTWEBKIT_STATIC_LIBRARIES MATCHES "sqlite3")
    list(APPEND TROJITA_LIBRARIES libsqlite3-0.dll)
endif()

# Check if Qt depends on external system zlib library
if(WITH_ZLIB AND NOT ZLIB_LIBRARIES STREQUAL QT_QTCORE_LIBRARY)
    list(APPEND TROJITA_LIBRARIES "${ZLIB_LIBRARIES}")
endif()

if(EXISTS "${QT_MKSPECS_DIR}/qconfig.pri")
    file(READ ${QT_MKSPECS_DIR}/qconfig.pri QT_CONFIG)
    string(REGEX MATCH "QT_CONFIG[^\n]+" QT_CONFIG ${QT_CONFIG})
    # Check if Qt depends on external system libpng library
    if(QT_CONFIG MATCHES " system-png ")
        find_package(PNG)
        if(PNG_VERSION_STRING)
            # QtGui depends on libpng, name pattern is: "libpng<maj><min>-<maj><min>.dll"
            string(REGEX REPLACE "^([0-9])\\.([0-9])\\..*\$" "libpng\\1\\2-\\1\\2.dll" LIBRARY "${PNG_VERSION_STRING}")
            list(APPEND TROJITA_LIBRARIES "${LIBRARY}")
        else()
            message(SEND_ERROR "External PNG library is required but version was not detected")
        endif()
    endif()
    # Check if Qt was compiled with openssl support
    if((QT_CONFIG MATCHES " openssl " OR QT_CONFIG MATCHES " openssl-linked ") AND NOT QT_CONFIG MATCHES " no-openssl ")
        # By default use MSVC name scheme of openssl libraries because other name schemes are not supported by standard Qt
        if(NOT OPENSSL_DLL_LIBRARIES)
            set(OPENSSL_DLL_LIBRARIES ssleay32.dll libeay32.dll CACHE STRING "" FORCE)
        endif()
        # Append windows openssl libraries
        list(APPEND TROJITA_LIBRARIES ${OPENSSL_DLL_LIBRARIES})
    endif()
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -dumpversion OUTPUT_VARIABLE GNU_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -dumpmachine OUTPUT_VARIABLE GNU_TARGET OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -v ERROR_VARIABLE GNU_OUTPUT ERROR_STRIP_TRAILING_WHITESPACE)
    # Detect exceptions handling and threading model: http://stackoverflow.com/a/16103497
    if(GNU_OUTPUT MATCHES "--enable-[a-z]+-exceptions")
        string(REGEX REPLACE ".*--enable-([a-z]+)-exceptions.*" "\\1" GNU_EXCEPTIONS "${GNU_OUTPUT}")
        message(STATUS "Detected ${GNU_EXCEPTIONS} exceptions handling")
    elseif(CMAKE_SIZEOF_VOID_P STREQUAL 8 AND NOT GNU_VERSION VERSION_LESS "4.8")
        set(GNU_EXCEPTIONS seh)
    else()
        set(GNU_EXCEPTIONS sjlj)
    endif()
    if(GNU_OUTPUT MATCHES "--enable-threads=posix" OR GNU_OUTPUT MATCHES "Thread model: posix")
        # Posix threading model needs libwinpthread-1.dll library
        list(APPEND TROJITA_LIBRARIES libwinpthread-1.dll)
    endif()
    # All not static linked mingw/gcc binaries depends on libstdc++ and GNU runtime library, name pattern is: libgcc_s_<exceptions>-1.dll
    list(APPEND TROJITA_LIBRARIES libgcc_s_${GNU_EXCEPTIONS}-1.dll libstdc++-6.dll)
    # Detect gcc install path
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-search-dirs OUTPUT_VARIABLE GNU_INSTALL OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(GNU_INSTALL MATCHES "install: ")
        string(REGEX REPLACE ".*install: ([^\n]+)\n.*" "\\1" GNU_INSTALL "${GNU_INSTALL}")
        list(APPEND TROJITA_LIBRARIES_PATHS ${GNU_INSTALL})
    else()
        # Add additional gcc directories where runtime libraries can be stored if install path is not detected
        foreach(PREFIX ${CMAKE_SYSTEM_PREFIX_PATH})
            list(APPEND TROJITA_LIBRARIES_PATHS ${PREFIX}/lib/gcc/${GNU_TARGET}/${GNU_VERSION})
        endforeach()
    endif()
else()
    # TODO: List all MSVC and CLang required runtime libraries
    message(SEND_ERROR "Windows NSIS installer is not supported without GCC compiler yet")
endif()

# Include all DLL libraries
foreach(LIBRARY ${TROJITA_LIBRARIES})
    # By default mingw/gcc application are linked with dll.a archives and not dll libraries. In dll.a archive is stored real dll name, but from cmake we cannot read it. Solution is to convert dll.a basename to dll and find new dll file in system.
    # TODO: Add support for MSVC lib naming scheme
    if(LIBRARY MATCHES "lib.*\\.dll\\.a\$")
        if(IS_ABSOLUTE "${LIBRARY}")
            get_filename_component(LIBRARY ${LIBRARY} NAME)
        endif()
        # Convert .dll.a filename to .dll filename
        string(REGEX REPLACE "^lib(.*\\.dll)\\.a\$" "\\1" LIBRARY "${LIBRARY}")
    endif()
    if(NOT IS_ABSOLUTE "${LIBRARY}" AND LIBRARY MATCHES "^.*\\.dll\$")
        if("${LIBRARY}" STREQUAL "z.dll")
            # Correct name of libz dll library is zlib1.dll
            set(LIBRARY "zlib1.dll")
        endif()
        find_library(LIBRARY_PATH "${LIBRARY}" PATHS ${TROJITA_LIBRARIES_PATHS} CMAKE_FIND_ROOT_PATH_BOTH)
        if(LIBRARY_PATH AND NOT LIBRARY_PATH MATCHES "^lib.*\\.dll\\.a\$")
            set(LIBRARY "${LIBRARY_PATH}")
        endif()
        unset(LIBRARY_PATH CACHE)
    endif()
    if(LIBRARY MATCHES ".*\\.dll\$")
        if(IS_ABSOLUTE "${LIBRARY}" AND EXISTS "${LIBRARY}")
            message(STATUS "Including library ${LIBRARY} into NSIS installer")
            list(APPEND TROJITA_INSTALL_FILES "${LIBRARY}")
        else()
            message(SEND_ERROR "Full path for library ${LIBRARY} was not detected")
        endif()
    endif()
endforeach()

# Include be.contacts executable
get_target_property(FILE_PATH be.contacts LOCATION)
list(APPEND TROJITA_INSTALL_FILES "${FILE_PATH}")

# Include common library for plugins
if(WITH_SHARED_PLUGINS)
    get_target_property(FILE_PATH Plugins LOCATION)
    list(APPEND TROJITA_INSTALL_FILES "${FILE_PATH}")
endif()

# Include all shared plugins
get_property(SHARED_PLUGINS GLOBAL PROPERTY TROJITA_SHARED_PLUGINS)
foreach(PLUGIN ${SHARED_PLUGINS})
    get_target_property(FILE_PATH ${PLUGIN} LOCATION)
    list(APPEND TROJITA_INSTALL_FILES "${FILE_PATH}")
endforeach()

# Include additional files
list(APPEND TROJITA_INSTALL_FILES "${CMAKE_CURRENT_SOURCE_DIR}/README")
list(APPEND TROJITA_INSTALL_FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# Generate list of install files
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "!macro TROJITA_INSTALL_FILES\n")
if(LinguistForTrojita_FOUND OR Qt5LinguistForTrojita_FOUND)
    if(num_languages)
        file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "SetOutPath \"\$INSTDIR\\locale\"\n")
        file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "File /r /x *.ts /x x_test locale\\\n")
        file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "SetOutPath \"\$INSTDIR\"\n")
    endif()
endif()
if(QT_QSQLITE_LIBRARY AND QT_QSQLITE_LIBRARY MATCHES ".*\\.dll")
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "SetOutPath \"\$INSTDIR\\sqldrivers\"\n")
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "File \"${QT_QSQLITE_LIBRARY}\"\n")
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "SetOutPath \"\$INSTDIR\"\n")
endif()
foreach(FILE_PATH ${TROJITA_INSTALL_FILES})
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "File \"${FILE_PATH}\"\n")
endforeach()
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "!macroend\n")

# Generate list of uninstall files
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "!macro TROJITA_UNINSTALL_FILES\n")
if(LinguistForTrojita_FOUND OR Qt5LinguistForTrojita_FOUND)
    if(num_languages)
        file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "RMDir /r \"\$INSTDIR\\locale\"\n")
    endif()
endif()
if(QT_QSQLITE_LIBRARY AND QT_QSQLITE_LIBRARY MATCHES ".*\\.dll")
    get_filename_component(FILE_NAME ${QT_QSQLITE_LIBRARY} NAME)
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "Delete \"\$INSTDIR\\sqldrivers\\${FILE_NAME}\"\n")
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "RMDir \"$INSTDIR\\sqldrivers\"\n")
endif()
foreach(FILE_PATH ${TROJITA_INSTALL_FILES})
    get_filename_component(FILE_NAME ${FILE_PATH} NAME)
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "Delete \"\$INSTDIR\\${FILE_NAME}\"\n")
endforeach()
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "!macroend\n")

execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi)

### Generate installer ###

if(CMAKE_SIZEOF_VOID_P STREQUAL 4)
    set(MAKENSIS_OUTPUT Trojita-Setup.exe)
else()
    set(MAKENSIS_FLAGS -DX86_64 ${MAKENSIS_FLAGS})
    set(MAKENSIS_OUTPUT Trojita-Setup-x86_64.exe)
endif()

if(NOT CMAKE_VERBOSE_MAKEFILE)
    set(MAKENSIS_FLAGS -V2 -DQUIET ${MAKENSIS_FLAGS})
endif()

set(MAKENSIS_FLAGS ${MAKENSIS_FLAGS} -NOCD)

add_custom_command(OUTPUT ${MAKENSIS_OUTPUT}
    COMMAND ${MAKENSIS}
    ARGS ${MAKENSIS_FLAGS} ${CMAKE_CURRENT_SOURCE_DIR}/packaging/trojita.nsi
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/packaging/trojita.nsi ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi ${TROJITA_EXE_PATH} ${TROJITA_ICON_PATH} ${TROJITA_INSTALL_FILES} version)
add_custom_target(installer ALL DEPENDS ${MAKENSIS_OUTPUT})
