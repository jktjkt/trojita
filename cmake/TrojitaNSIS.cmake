# Create NSIS Installer, check for all dependent DLL libraries and include them into Installer

### Generate list of files for inclusion into installer ###

if(POLICY CMP0026)
    # reading the LOCATION property
    cmake_policy(SET CMP0026 OLD)
endif()

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

# Prepare a list of all Qt plugins which are needed.
# We need a hardcoded list of plugin types because I'm too lazy to find out how to work with associative arrays/hashmaps in cmake.
set(TROJITA_QT_PLUGIN_TYPES bearer generic iconengines imageformats sqldrivers platforms)

foreach(plugintype ${TROJITA_QT_PLUGIN_TYPES})
    set(TROJITA_QT_${plugintype}_PLUGINS "")
endforeach()

foreach(plugin ${Qt5Gui_PLUGINS} ${Qt5Network_PLUGINS} Qt5::QSQLiteDriverPlugin Qt5::QSvgIconPlugin)
  get_target_property(_loc ${plugin} LOCATION)

  set(plugin_recognized FALSE)

  foreach(plugintype ${TROJITA_QT_PLUGIN_TYPES})
      if(${_loc} MATCHES "/plugins/${plugintype}/.*\\.dll$")
          list(APPEND TROJITA_QT_${plugintype}_PLUGINS ${_loc})
          set(plugin_recognized TRUE)
      endif()
  endforeach()

  if(NOT ${plugin_recognized})
      message(FATAL_ERROR "Unrecognized Qt plugin -- don't know where to put it: ${_loc}")
  endif()

endforeach()

# Qt doesn't really link with OpenSSL, but loads it during runtime, which means that our objdump won't find the DLL names.
# Of course, the library names are provided through the .dll.a convention, and I'm too lazy to write code which converts
# them to just DLLs, so we support either passing them explicitly, or we require CMake 4.4 and its OpenSSL imported targets.
# The reason for this is that we only have cmake-3.3 in the CI so far.
if(NOT (EXISTS "${OpenSSL_Crypto_LOC}" AND EXISTS "${OpenSSL_SSL_LOC}"))
    find_package(OpenSSL REQUIRED)
    cmake_minimum_required(VERSION 3.4.0)
    get_target_property(OpenSSL_SSL_LOC OpenSSL::SSL LOCATION)
    get_target_property(OpenSSL_Crypto_LOC OpenSSL::Crypto LOCATION)
endif()

# We don't have trojita.exe at CMake time yet, so we have to take a look at all the stuff which we know is going to be needed.
# I could probably write some CMake code to handle this, but sorry, that's ETOOCOMPLEX for me. Instead, I chose to write this
# ugly beast and to swear each time in future when the list of libraries which Trojita links against gets extended, and someone
# forgets to update *this* hardcoded list. Patches which improve this are very welcome.

get_target_property(QtCore_LOC Qt5::Core LOCATION)
get_target_property(QtGui_LOC Qt5::Gui LOCATION)
get_target_property(QtNetwork_LOC Qt5::Network LOCATION)
get_target_property(QtSql_LOC Qt5::Sql LOCATION)
get_target_property(QtWebKitWidgets_LOC Qt5::WebKitWidgets LOCATION)
get_target_property(QtWidgets_LOC Qt5::Widgets LOCATION)
set(required_dll_names ${QtCore_LOC} ${QtGui_LOC} ${QtNetwork_LOC} ${QtSql_LOC} ${QtWebKitWidgets_LOC} ${QtWidgets_LOC} ${OpenSSL_SSL_LOC} ${OpenSSL_Crypto_LOC})
if(WITH_DBUS)
    get_target_property(QtDBus_LOC Qt5::DBus LOCATION)
    list(APPEND required_dll_names ${QtDBus_LOC})
endif()

find_package(PythonInterp REQUIRED interpreter)

message(STATUS "Determining the list of required DLL files...")
set(all_plugin_dlls "")
foreach(plugintype ${TROJITA_QT_PLUGIN_TYPES})
    list(APPEND all_plugin_dlls ${TROJITA_QT_${plugintype}_PLUGINS})
endforeach()
execute_process(COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/packaging/mingw-bundledlls
    ${required_dll_names} ${all_plugin_dlls}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT_VARIABLE library_dependencies_STR)
# convert from one-file-per-line into a cmake list
string(REPLACE "\n" ";" library_dependencies "${library_dependencies_STR}")

# because mingw-bundledlls filters out the name of the libraries which were passed on the command line
list(APPEND library_dependencies ${required_dll_names})

# ...and here we go!
foreach(dll_lib ${library_dependencies})
    message(STATUS "Including DLL: ${dll_lib}")
endforeach()
set(TROJITA_INSTALL_FILES "${TROJITA_INSTALL_FILES};${library_dependencies}")


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
foreach(FILE_PATH ${TROJITA_INSTALL_FILES})
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "File \"${FILE_PATH}\"\n")
endforeach()
if(Qt5LinguistForTrojita_FOUND)
    if(num_languages)
        file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "File /r /x *.ts /x x_test locale\n")
    endif()
endif()
foreach(plugintype ${TROJITA_QT_PLUGIN_TYPES})
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "SetOutPath \"\$INSTDIR\\${plugintype}\"\n")
    foreach(plugin ${TROJITA_QT_${plugintype}_PLUGINS})
        file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "File \"${plugin}\"\n")
        message(STATUS "Including ${plugintype} plugin: ${plugin}")
    endforeach()
endforeach()

file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "!macroend\n")

# Generate list of uninstall files
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "!macro TROJITA_UNINSTALL_FILES\n")
if(Qt5LinguistForTrojita_FOUND)
    if(num_languages)
        file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "RMDir /r \"\$INSTDIR\\locale\"\n")
    endif()
endif()

foreach(plugintype ${TROJITA_QT_PLUGIN_TYPES})
    foreach(plugin ${TROJITA_QT_${plugintype}_PLUGINS})
        get_filename_component(plugin_basename ${plugin} NAME)
        file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "Delete \"\$INSTDIR\\${plugintype}\\${plugin_basename}\"\n")
    endforeach()
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/trojita-files.nsi.in "RMDir \"\$INSTDIR\\${plugintype}\"\n")
endforeach()

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
