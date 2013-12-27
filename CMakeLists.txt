if(WITH_QT5)
    # The Qt5's qt5_use_modules is only available on 2.8.9 or later -- sweet, isn't it?
    cmake_minimum_required(VERSION 2.8.9)
else()
    cmake_minimum_required(VERSION 2.8.7)
endif()
project(trojita)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# POSITION_INDEPENDENT_CODE is only available on cmake 2.8.9 or later
# Add needed flags for supported compilers which simulate POSITION_INDEPENDENT_CODE property
if(CMAKE_VERSION VERSION_LESS "2.8.9")
    if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    else()
        message(FATAL_ERROR "Needs GNU or Clang C++ compiler or CMake 2.8.9 (or later)")
    endif()
else()
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

include(FindCXXFeatures)
if(NOT CXXFeatures_auto_FOUND)
    message(SEND_ERROR "Your compiler doesn't support C++11's auto")
endif()
if(NOT CXXFeatures_static_assert_FOUND)
    message(SEND_ERROR "Your compiler doesn't support C++11's static_assert")
endif()
if(NOT CXXFeatures_alignof_FOUND)
    if(NOT CMAKE_COMPILER_IS_GNUCXX)
        message(SEND_ERROR "Your compiler doesn't support C++11's alignof and it also isn't gcc. Either would work.")
    endif()
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX11_COMPILER_FLAGS}")

include(TrojitaOption)

trojita_option(WITH_DESKTOP "Build desktop version" ON)
trojita_option(WITH_HARMATTAN "Build MeeGo Harmattan version" OFF "NOT WITH_DESKTOP")
trojita_option(WITH_QT5 "Build with Qt5 library" OFF)
trojita_option(WITH_RAGEL "Build with Ragel library" AUTO)
trojita_option(WITH_ZLIB "Build with zlib library" AUTO)
trojita_option(WITH_SHARED_PLUGINS "Enable shared dynamic plugins" ON)
trojita_option(WITH_TESTS "Build tests" ON "NOT WITH_HARMATTAN")
trojita_option(DEV_FATAL_WARNINGS "Turn build warnings into errors (developers only)" OFF)

if(WIN32)
    trojita_option(WITH_NSIS "Build Windows NSIS installer" AUTO)
endif()

if(WITH_QT5)
    message(STATUS "Building the Qt5 version")
    find_package(Qt5Gui REQUIRED)
    find_package(Qt5Network REQUIRED)
    find_package(Qt5Sql REQUIRED)
    find_package(Qt5WebKitWidgets REQUIRED)
    find_package(Qt5Widgets REQUIRED)
    find_package(Qt5LinguistTools)
    if(WITH_TESTS)
        find_package(Qt5Test REQUIRED)
    endif()
    if(Qt5LinguistTools_FOUND)
        find_package(Qt5LinguistForTrojita)
    endif()
else()
    message(STATUS "Building the Qt4 version")
    set(QT_USE_QTNETWORK 1)
    set(QT_USE_QTSQL 1)
    set(QT_USE_QTWEBKIT 1)
    if(WITH_TESTS)
        set(QT_USE_QTTEST 1)
    endif()
    if(WITH_HARMATTAN)
        set(QT_USE_QTDECLARATIVE 1)
    endif()
    trojita_find_package(Qt4 4.6 "http://qt-project.org" "Qt4" "Needed for building" REQUIRED)
    include(${QT_USE_FILE})
    trojita_check_qt4_module(QTCORE REQUIRED)
    trojita_check_qt4_module(QTGUI REQUIRED)
    trojita_check_qt4_module(QTNETWORK REQUIRED)
    trojita_check_qt4_module(QTSQL REQUIRED)
    trojita_check_qt4_module(QTWEBKIT REQUIRED)
    trojita_check_qt4_module(QTTEST WITH_TESTS)
    trojita_check_qt4_module(QTDECLARATIVE WITH_HARMATTAN)
    trojita_find_package(LinguistForTrojita "" "" "" "")
endif()
trojita_find_package(Git "" "" "" "")

if(WIN32)
    trojita_find_package(MakeNSIS "" "http://nsis.sourceforge.net" "Nullsoft Scriptable Install System" "Needed for building Windows installer" WITH_NSIS)
endif()

# Add support for Mingw RC compiler
if(WIN32)
    enable_language(RC)
    include(CMakeDetermineRCCompiler)

    if(MINGW)
        set(CMAKE_RC_COMPILER_INIT windres)
        set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")
    endif()
endif()

if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR "lib${LIB_SUFFIX}")
endif()
mark_as_advanced(CMAKE_INSTALL_LIBDIR)

if(NOT CMAKE_INSTALL_PLUGIN_DIR)
    set(CMAKE_INSTALL_PLUGIN_DIR "${CMAKE_INSTALL_LIBDIR}/trojita")
endif()
mark_as_advanced(CMAKE_INSTALL_PLUGIN_DIR)

if(NOT PLUGIN_DIR)
    set(PLUGIN_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_PLUGIN_DIR}")
endif()
mark_as_advanced(PLUGIN_DIR)

if(NOT WIN32 AND NOT MSVC)
    # Check if we are NOT on Windows and don't use MSVC since -g is not supported by VC
    # and Wall makes the build very noisy.
    # At first, enable optimizations. In my testing, these do *not* make debugging any harder than no optimization and
    # they (of course) optimize away stuff like QByteArray::operator[] etc.
    # We put the user's CXXFLAGS *after* that so that they take priority.
    set(CMAKE_CXX_FLAGS "-O2 -g ${CMAKE_CXX_FLAGS}")
    # Now enable build warnings; these are useful tools (and Trojita should be warning-free anyway). We do not set
    # -Werror for sanity reasons, though (one cannot know what warnings a future compiler might bring along).
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wsign-compare")
endif(NOT WIN32 AND NOT MSVC)
if(DEV_FATAL_WARNINGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
# The following is required so that the moc_*.cpp and ui_*.h are found
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_definitions(-DQT_STRICT_ITERATORS)

# Make sure that plugins not export all symbols, only that which are explicitly marked
include(GenerateExportHeader)
add_compiler_export_flags()

if(NOT WITH_QT5 AND CMAKE_VERSION VERSION_LESS 2.8.10)
    set(SUPPORTS_TARGET_INCLUDES 0)
    include_directories(
        ${QT_MKSPECS_DIR}/default
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui/
        ${CMAKE_CURRENT_SOURCE_DIR}/src/mimetypes-qt4/io/
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Harmattan/qmlapplicationviewer
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_LibMailboxSync)
else()
    set(SUPPORTS_TARGET_INCLUDES 1)
endif()

set(CMAKE_AUTOMOC True)

trojita_find_package(RagelForTrojita "" "" "" "" WITH_RAGEL)

if(WIN32) # Check if we are on Windows
    # On win32, qt can statically link to zlib and export their symbols.
    # We check if we can find the string " zlib " in the QT_CONFIG list in
    # ${QT_MKSPECS_DIR}/qconfig.pri and include on success ${QT_QTCORE_LIBRARY}
    # and ${QTDIR}/src/3rdparty/zlib as lib and include folder
    if(QT_QTCORE_FOUND)
        message(STATUS "We are on Windows with Qt. Checking if zlib is built into Qt")

        if(EXISTS "${QT_MKSPECS_DIR}/qconfig.pri")
            file(READ ${QT_MKSPECS_DIR}/qconfig.pri _qconfig_FILE_contents_ZLIB_CHECK)
            string(REGEX MATCH "QT_CONFIG[^\n]+" QT_CONFIG_ZLIB_CHECK ${_qconfig_FILE_contents_ZLIB_CHECK})

            set(ENV_QTDIR $ENV{QTDIR})
            if(NOT DEFINED ENV_QTDIR)
                message(STATUS "QTDIR not specified in environment, will not use zlib")
            elseif(QT_CONFIG_ZLIB_CHECK)
                if(QT_CONFIG_ZLIB_CHECK MATCHES " zlib ")
                    message(STATUS "Found zlib in QT_QCONFIG. zlib seems to be built into Qt")
                    set(ZLIB_LIBRARY_RELEASE ${QT_QTCORE_LIBRARY_RELEASE})
                    set(ZLIB_LIBRARY_DEBUG ${QT_QTCORE_LIBRARY_DEBUG})
                    set(ZLIB_LIBRARY ${QT_QTCORE_LIBRARY})

                    string(REGEX REPLACE "\\\\" "/" QTDIR ${ENV_QTDIR})
                    set(ZLIB_INCLUDE_DIR "${QTDIR}/src/3rdparty/zlib")

                    set(ZLIB_FOUND TRUE)
                    set(ZLIB_LIBRARIES ${ZLIB_LIBRARY} )
                    mark_as_advanced(ZLIB_LIBRARY ZLIB_INCLUDE_DIR ZLIB_FOUND)
                else(QT_CONFIG_ZLIB_CHECK MATCHES " zlib ")
                    message(STATUS "Could not determine if Qt was built with zlib support.")
                endif(QT_CONFIG_ZLIB_CHECK MATCHES " zlib ")

            endif()
        endif(EXISTS "${QT_MKSPECS_DIR}/qconfig.pri")
    endif(QT_QTCORE_FOUND)
endif()

if(NOT ZLIB_FOUND)
    trojita_find_package(ZLIB "" "" "" "" WITH_ZLIB)
endif()

if(WITH_ZLIB)
    set(TROJITA_HAVE_ZLIB True)
    message(STATUS "Support for COMPRESS=DEFLATE enabled")
else()
    set(TROJITA_HAVE_ZLIB False)
    message(STATUS "Disabling COMPRESS=DEFLATE, zlib is not available")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/configure.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/configure.cmake.h)

feature_summary(FATAL_ON_MISSING_REQUIRED_PACKAGES DESCRIPTION "\n" WHAT ALL)

set(path_Common ${CMAKE_CURRENT_SOURCE_DIR}/src/Common)
set(libCommon_SOURCES
    ${path_Common}/Application.cpp
    ${path_Common}/ConnectionId.cpp
    ${path_Common}/DeleteAfter.cpp
    ${path_Common}/FileLogger.cpp
    ${path_Common}/MetaTypes.cpp
    ${path_Common}/Paths.cpp
    ${path_Common}/SettingsNames.cpp
)

set(path_Plugins ${CMAKE_CURRENT_SOURCE_DIR}/src/Plugins)
set(libPlugins_SOURCES
    ${path_Plugins}/AddressbookPlugin.cpp
    ${path_Plugins}/PasswordPlugin.cpp
    ${path_Plugins}/PluginJob.cpp
    ${path_Plugins}/PluginManager.cpp
)

set(path_Composer ${CMAKE_CURRENT_SOURCE_DIR}/src/Composer)
set(libComposer_SOURCES
    ${path_Composer}/ComposerAttachments.cpp
    ${path_Composer}/MessageComposer.cpp
    ${path_Composer}/PlainTextFormatter.cpp
    ${path_Composer}/Recipients.cpp
    ${path_Composer}/ReplaceSignature.cpp
    ${path_Composer}/SenderIdentitiesModel.cpp
    ${path_Composer}/SubjectMangling.cpp
    ${path_Composer}/Submission.cpp
)

set(path_MSA ${CMAKE_CURRENT_SOURCE_DIR}/src/MSA)
set(libMSA_SOURCES
    ${path_MSA}/AbstractMSA.cpp
    ${path_MSA}/FakeMSA.cpp
    ${path_MSA}/ImapSubmit.cpp
    ${path_MSA}/SMTP.cpp
    ${path_MSA}/Sendmail.cpp
)

set(path_Streams ${CMAKE_CURRENT_SOURCE_DIR}/src/Streams)
set(libStreams_SOURCES
    ${path_Streams}/DeletionWatcher.cpp
    ${path_Streams}/FakeSocket.cpp
    ${path_Streams}/IODeviceSocket.cpp
    ${path_Streams}/Socket.cpp
    ${path_Streams}/SocketFactory.cpp
)

if(WITH_ZLIB)
    set(libStreams_SOURCES ${libStreams_SOURCES}
        ${path_Streams}/3rdparty/rfc1951.cpp)
    include_directories(${ZLIB_INCLUDE_DIR})
endif()

if(NOT WITH_QT5)
    set(path_mimetypesqt4 ${CMAKE_CURRENT_SOURCE_DIR}/src/mimetypes-qt4)
    set(libMimetypesQt4_SOURCES
        ${path_mimetypesqt4}/io/qstandardpaths.cpp
        ${path_mimetypesqt4}/mimetypes/qmimedatabase.cpp
        ${path_mimetypesqt4}/mimetypes/qmimeglobpattern.cpp
        ${path_mimetypesqt4}/mimetypes/qmimemagicrule.cpp
        ${path_mimetypesqt4}/mimetypes/qmimemagicrulematcher.cpp
        ${path_mimetypesqt4}/mimetypes/qmimetype.cpp
        ${path_mimetypesqt4}/mimetypes/qmimetypeparser.cpp
        ${path_mimetypesqt4}/mimetypes/qmimeprovider.cpp
    )
    if(WIN32)
        set(libMimetypesQt4_SOURCES ${libMimetypesQt4_SOURCES}
            ${path_mimetypesqt4}/io/qstandardpaths_win.cpp)
    elseif(APPLE)
        set(libMimetypesQt4_SOURCES ${libMimetypesQt4_SOURCES}
            ${path_mimetypesqt4}/io/qstandardpaths_mac.cpp)
    elseif (OS2)
        set(libMimetypesQt4_SOURCES ${libMimetypesQt4_SOURCES}
            ${path_mimetypesqt4}/io/qstandardpaths_os2.cpp)
    elseif (UNIX)
        set(libMimetypesQt4_SOURCES ${libMimetypesQt4_SOURCES}
            ${path_mimetypesqt4}/io/qstandardpaths_unix.cpp)
    else()
        message(FATAL_ERROR "Unsupported platform -- mimetypes-Qt4 support only Unix, MacOSX, Windows and OS/2")
    endif()
endif()

set(path_DesktopGui ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui)
set(libDesktopGui_SOURCES
    ${path_DesktopGui}/AttachmentView.cpp
    ${path_DesktopGui}/AutoCompletion.cpp
    ${path_DesktopGui}/CompleteMessageWidget.cpp
    ${path_DesktopGui}/ComposeWidget.cpp
    ${path_DesktopGui}/ComposerAttachmentsList.cpp
    ${path_DesktopGui}/ComposerTextEdit.cpp
    ${path_DesktopGui}/EmbeddedWebView.cpp
    ${path_DesktopGui}/EnvelopeView.cpp
    ${path_DesktopGui}/ExternalElementsWidget.cpp
    ${path_DesktopGui}/FindBar.cpp
    ${path_DesktopGui}/FlowLayout.cpp
    ${path_DesktopGui}/FromAddressProxyModel.cpp
    ${path_DesktopGui}/LineEdit.cpp
    ${path_DesktopGui}/LoadablePartWidget.cpp
    ${path_DesktopGui}/MailBoxTreeView.cpp
    ${path_DesktopGui}/MessageListWidget.cpp
    ${path_DesktopGui}/MessageSourceWidget.cpp
    ${path_DesktopGui}/MessageView.cpp
    ${path_DesktopGui}/MsgListView.cpp
    ${path_DesktopGui}/OnePanelAtTimeWidget.cpp
    ${path_DesktopGui}/OverlayWidget.cpp
    ${path_DesktopGui}/PartWidget.cpp
    ${path_DesktopGui}/PartWidgetFactory.cpp
    ${path_DesktopGui}/PasswordDialog.cpp
    ${path_DesktopGui}/ProgressPopUp.cpp
    ${path_DesktopGui}/ProtocolLoggerWidget.cpp
    ${path_DesktopGui}/ReplaceCharValidator.cpp
    ${path_DesktopGui}/SettingsDialog.cpp
    ${path_DesktopGui}/SimplePartWidget.cpp
    ${path_DesktopGui}/Spinner.cpp
    ${path_DesktopGui}/TagListWidget.cpp
    ${path_DesktopGui}/TagWidget.cpp
    ${path_DesktopGui}/TaskProgressIndicator.cpp
    ${path_DesktopGui}/UserAgentWebPage.cpp
    ${path_DesktopGui}/Util.cpp
    ${path_DesktopGui}/Window.cpp
    ${path_DesktopGui}/ShortcutHandler/ShortcutConfigDialog.cpp
    ${path_DesktopGui}/ShortcutHandler/ShortcutConfigWidget.cpp
    ${path_DesktopGui}/ShortcutHandler/ShortcutHandler.cpp
)
set(libDesktopGui_UI
    ${path_DesktopGui}/AboutDialog.ui
    ${path_DesktopGui}/ComposeWidget.ui
    ${path_DesktopGui}/CreateMailboxDialog.ui
    ${path_DesktopGui}/EditIdentity.ui
    ${path_DesktopGui}/PasswordDialog.ui
    ${path_DesktopGui}/ProgressPopUp.ui
    ${path_DesktopGui}/SettingsCachePage.ui
    ${path_DesktopGui}/SettingsGeneralPage.ui
    ${path_DesktopGui}/SettingsImapPage.ui
    ${path_DesktopGui}/SettingsOutgoingPage.ui
    ${path_DesktopGui}/ShortcutHandler/ShortcutConfigWidget.ui
)
set(libDesktopGui_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/icons.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/license.qrc
)

set(libqwwsmtpclient_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/qwwsmtpclient/qwwsmtpclient.cpp)

set(libAppVersion_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/AppVersion/SetCoreApplication.cpp)

set(path_Imap ${CMAKE_CURRENT_SOURCE_DIR}/src/Imap)
set(libImap_SOURCES
    ${path_Imap}/ConnectionState.cpp
    ${path_Imap}/Encoders.cpp
    ${path_Imap}/Exceptions.cpp
    ${path_Imap}/Parser/3rdparty/kcodecs.cpp
    ${path_Imap}/Parser/3rdparty/rfccodecs.cpp

    ${path_Imap}/Parser/Command.cpp
    ${path_Imap}/Parser/Data.cpp
    ${path_Imap}/Parser/LowLevelParser.cpp
    ${path_Imap}/Parser/MailAddress.cpp
    ${path_Imap}/Parser/Message.cpp
    ${path_Imap}/Parser/Parser.cpp
    ${path_Imap}/Parser/Response.cpp
    ${path_Imap}/Parser/Sequence.cpp
    ${path_Imap}/Parser/ThreadingNode.cpp

    ${path_Imap}/Network/FileDownloadManager.cpp
    ${path_Imap}/Network/ForbiddenReply.cpp
    ${path_Imap}/Network/MsgPartNetAccessManager.cpp
    ${path_Imap}/Network/MsgPartNetworkReply.cpp

    ${path_Imap}/Model/Cache.cpp
    ${path_Imap}/Model/CombinedCache.cpp
    ${path_Imap}/Model/DragAndDrop.cpp
    ${path_Imap}/Model/DiskPartCache.cpp
    ${path_Imap}/Model/FindInterestingPart.cpp
    ${path_Imap}/Model/FlagsOperation.cpp
    ${path_Imap}/Model/FullMessageCombiner.cpp
    ${path_Imap}/Model/MailboxMetadata.cpp
    ${path_Imap}/Model/MailboxModel.cpp
    ${path_Imap}/Model/MailboxTree.cpp
    ${path_Imap}/Model/MemoryCache.cpp
    ${path_Imap}/Model/Model.cpp
    ${path_Imap}/Model/MsgListModel.cpp
    ${path_Imap}/Model/NetworkWatcher.cpp
    ${path_Imap}/Model/OneMessageModel.cpp
    ${path_Imap}/Model/ParserState.cpp
    ${path_Imap}/Model/PrettyMailboxModel.cpp
    ${path_Imap}/Model/PrettyMsgListModel.cpp
    ${path_Imap}/Model/SpecialFlagNames.cpp
    ${path_Imap}/Model/SQLCache.cpp
    ${path_Imap}/Model/SubtreeModel.cpp
    ${path_Imap}/Model/TaskFactory.cpp
    ${path_Imap}/Model/TaskPresentationModel.cpp
    ${path_Imap}/Model/ThreadingMsgListModel.cpp
    ${path_Imap}/Model/Utils.cpp
    ${path_Imap}/Model/VisibleTasksModel.cpp

    # The ModelTest is only needed when debugging manually
    #${path_Imap}/Model/ModelTest/modeltest.cpp
    # The ModelWatcher is another debugging aid
    ${path_Imap}/Model/ModelWatcher.cpp

    ${path_Imap}/Model/kdeui-itemviews/kdescendantsproxymodel.cpp

    ${path_Imap}/Tasks/AppendTask.cpp
    ${path_Imap}/Tasks/CopyMoveMessagesTask.cpp
    ${path_Imap}/Tasks/CreateMailboxTask.cpp
    ${path_Imap}/Tasks/DeleteMailboxTask.cpp
    ${path_Imap}/Tasks/EnableTask.cpp
    ${path_Imap}/Tasks/ExpungeMailboxTask.cpp
    ${path_Imap}/Tasks/ExpungeMessagesTask.cpp
    ${path_Imap}/Tasks/Fake_ListChildMailboxesTask.cpp
    ${path_Imap}/Tasks/Fake_OpenConnectionTask.cpp
    ${path_Imap}/Tasks/FetchMsgMetadataTask.cpp
    ${path_Imap}/Tasks/FetchMsgPartTask.cpp
    ${path_Imap}/Tasks/GenUrlAuthTask.cpp
    ${path_Imap}/Tasks/GetAnyConnectionTask.cpp
    ${path_Imap}/Tasks/IdTask.cpp
    ${path_Imap}/Tasks/IdleLauncher.cpp
    ${path_Imap}/Tasks/ImapTask.cpp
    ${path_Imap}/Tasks/KeepMailboxOpenTask.cpp
    ${path_Imap}/Tasks/ListChildMailboxesTask.cpp
    ${path_Imap}/Tasks/NoopTask.cpp
    ${path_Imap}/Tasks/NumberOfMessagesTask.cpp
    ${path_Imap}/Tasks/ObtainSynchronizedMailboxTask.cpp
    ${path_Imap}/Tasks/OfflineConnectionTask.cpp
    ${path_Imap}/Tasks/OpenConnectionTask.cpp
    ${path_Imap}/Tasks/SortTask.cpp
    ${path_Imap}/Tasks/SubscribeUnsubscribeTask.cpp
    ${path_Imap}/Tasks/ThreadTask.cpp
    ${path_Imap}/Tasks/UidSubmitTask.cpp
    ${path_Imap}/Tasks/UnSelectTask.cpp
    ${path_Imap}/Tasks/UpdateFlagsTask.cpp
    ${path_Imap}/Tasks/UpdateFlagsOfAllMessagesTask.cpp
)

if(WITH_RAGEL)
    message(STATUS "Using Ragel for the RFC 5322 parser")
    ragel_parser(${path_Imap}/Parser/Rfc5322HeaderParser.cpp)
    set(libImap_SOURCES ${libImap_SOURCES}
        ${CMAKE_CURRENT_BINARY_DIR}/Rfc5322HeaderParser.generated.cpp)
else()
    message(STATUS "Using pregenerated RFC 5322 parser")
    set(libImap_SOURCES ${libImap_SOURCES}
        ${path_Imap}/Parser/Rfc5322HeaderParser.generated.cpp)
endif()

set(path_AbookAddressbook ${CMAKE_CURRENT_SOURCE_DIR}/src/AbookAddressbook)
set(libAbookAddressbook_SOURCES
    ${path_AbookAddressbook}/AbookAddressbook.cpp
    ${path_AbookAddressbook}/be-contacts.cpp
)
set(libAbookAddressbook_UI
    ${path_AbookAddressbook}/be-contacts.ui
    ${path_AbookAddressbook}/onecontact.ui
)

set(trojita_desktop_SOURCES
    ${path_DesktopGui}/main.cpp
)

if(WIN32)
    list(APPEND trojita_desktop_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/trojita_win32.rc)
endif()

set(be_contacts_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/be.contacts/main.cpp
)

set(libQmlModelGlue_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/QmlSupport/ModelGlue/ImapAccess.cpp
)

set(libQNAMWebView_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/QmlSupport/QNAMWebView/plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/QmlSupport/QNAMWebView/qdeclarativewebview.cpp
)

set(trojitaHarmattan_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Harmattan/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Harmattan/qmlapplicationviewer/qmlapplicationviewer.cpp
)

if(LinguistForTrojita_FOUND OR Qt5LinguistForTrojita_FOUND)
    file(GLOB_RECURSE lang_PO "${CMAKE_CURRENT_SOURCE_DIR}/po/trojita_common_*.po")
    if(WITH_QT5)
        qt5_wrap_po(trojita_QM ${lang_PO})
    else()
        qt4_wrap_po(trojita_QM ${lang_PO})
    endif()
    set(language_summary "")
    foreach(po ${lang_PO})
        string(REGEX REPLACE "^(.*)/trojita_common_(.*).po" "\\2" lang ${po})
        list(APPEND language_summary ${lang})
    endforeach()
    list(SORT language_summary)
    list(LENGTH language_summary num_languages)
    if(num_languages)
        message(STATUS "Available languages: ${language_summary}")
        if(WITH_DESKTOP)
           install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/locale/ DESTINATION share/trojita/locale FILES_MATCHING PATTERN *.qm)
        endif()
    else()
        message(STATUS "No .po files found, will not install any languages")
    endif()
else()
    message(STATUS "Qt Linguist (lupdate/lrelease/lconvert) not found, disabling localization support")
endif()

set(version_files ${CMAKE_CURRENT_BINARY_DIR}/trojita-version.h ${CMAKE_CURRENT_BINARY_DIR}/trojita-git-version.h)
if(WITH_NSIS)
    set(version_files ${version_files} ${CMAKE_CURRENT_BINARY_DIR}/trojita-version.nsi)
    set(NSIS TRUE)
endif()

add_custom_target(version DEPENDS version_fake_file)
add_custom_command(OUTPUT version_fake_file ${version_files}
    COMMAND ${CMAKE_COMMAND} -DGIT_EXECUTABLE=${GIT_EXECUTABLE} -DNSIS=${NSIS} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/TrojitaVersion.cmake)
set_source_files_properties(${version_files}
    PROPERTIES GENERATED TRUE
    HEADER_FILE_ONLY TRUE)

add_library(Common ${libCommon_SOURCES})
add_dependencies(Common version)
target_link_libraries(Common ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})

add_library(AppVersion ${libAppVersion_SOURCES})
add_dependencies(AppVersion version)
target_link_libraries(AppVersion Common ${QT_QTCORE_LIBRARY})

if(WITH_SHARED_PLUGINS)
    add_library(Plugins SHARED ${libPlugins_SOURCES})
else()
    add_library(Plugins STATIC ${libPlugins_SOURCES})
    set_property(TARGET Plugins APPEND PROPERTY COMPILE_DEFINITIONS QT_STATICPLUGIN)
endif()
set_target_properties(Plugins PROPERTIES OUTPUT_NAME trojita_plugins)
if(WITH_QT5)
    qt5_use_modules(Plugins Core)
else()
    target_link_libraries(Plugins ${QT_QTCORE_LIBRARY})
endif()

add_library(Streams ${libStreams_SOURCES})
target_link_libraries(Streams ${QT_QTNETWORK_LIBRARY} ${QT_QTCORE_LIBRARY})
if(WITH_ZLIB)
    target_link_libraries(Streams ${ZLIB_LIBRARIES})
endif()

add_library(qwwsmtpclient ${libqwwsmtpclient_SOURCES})
target_link_libraries(qwwsmtpclient ${QT_QTNETWORK_LIBRARY} ${QT_QTCORE_LIBRARY})

add_library(MSA ${libMSA_SOURCES})
target_link_libraries(MSA Imap Streams qwwsmtpclient ${QT_QTCORE_LIBRARY})

add_library(Composer ${libComposer_SOURCES})
target_link_libraries(Composer Common MSA Streams qwwsmtpclient ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})
if(NOT WITH_QT5)
    target_link_libraries(Composer MimetypesQt4)
endif()

add_library(Imap ${libImap_SOURCES})
target_link_libraries(Imap Common Streams ${QT_QTNETWORK_LIBRARY} ${QT_QTSQL_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})
if(WITH_ZLIB)
    target_link_libraries(Imap ${ZLIB_LIBRARIES})
endif()

if(NOT WITH_QT5)
    add_library(MimetypesQt4 ${libMimetypesQt4_SOURCES})
    if(SUPPORTS_TARGET_INCLUDES)
        set_property(TARGET MimetypesQt4 APPEND PROPERTY INCLUDE_DIRECTORIES
            ${QT_MKSPECS_DIR}/default ${path_mimetypesqt4}/io/)
    endif()
    target_link_libraries(MimetypesQt4 ${QT_QTCORE_LIBRARY})
endif()

# Generate file static_plugins.h.in
get_property(STATIC_PLUGINS GLOBAL PROPERTY TROJITA_STATIC_PLUGINS)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/static_plugins.h.in "#include <QtPlugin>\n")
foreach(PLUGIN ${STATIC_PLUGINS})
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/static_plugins.h.in "Q_IMPORT_PLUGIN(${PLUGIN})\n")
endforeach()
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/static_plugins.h.in ${CMAKE_CURRENT_BINARY_DIR}/static_plugins.h)

if(WITH_DESKTOP)
    if(WITH_QT5)
        qt5_wrap_ui(libAbookAddressbook_UI_OUT ${libAbookAddressbook_UI})
        qt5_wrap_ui(libDesktopGui_UI_OUT ${libDesktopGui_UI})
        qt5_add_resources(libDesktopGui_RESOURCES_OUT ${libDesktopGui_RESOURCES})
    else()
        qt4_wrap_ui(libAbookAddressbook_UI_OUT ${libAbookAddressbook_UI})
        qt4_wrap_ui(libDesktopGui_UI_OUT ${libDesktopGui_UI})
        qt4_add_resources(libDesktopGui_RESOURCES_OUT ${libDesktopGui_RESOURCES})
    endif()

    add_library(AbookAddressbook ${libAbookAddressbook_SOURCES} ${libAbookAddressbook_UI_OUT})
    target_link_libraries(AbookAddressbook ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})

    add_library(DesktopGui ${libDesktopGui_SOURCES} ${libDesktopGui_UI_OUT} ${libDesktopGui_RESOURCES_OUT})
    # The following is needed for the LineEdit widget within the .ui files.
    # The ${path_DesktopGui} is needed so that the generated ui_*.h file can find the headers of the custom widgets
    if(SUPPORTS_TARGET_INCLUDES)
        set_property(TARGET DesktopGui APPEND PROPERTY INCLUDE_DIRECTORIES ${path_DesktopGui})
    endif()
    target_link_libraries(DesktopGui Common Composer Imap MSA Plugins Streams qwwsmtpclient AbookAddressbook ${QT_QTWEBKIT_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})

    # On Windows build a real Win32 GUI application without console window
    # On other platforms WIN32 flag is ignored
    add_executable(trojita WIN32 ${trojita_desktop_SOURCES} ${trojita_QM})
    target_link_libraries(trojita AppVersion Common DesktopGui ${STATIC_PLUGINS} ${QT_QTMAIN_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})
    if(NOT WITH_QT5)
        target_link_libraries(trojita MimetypesQt4)
    endif()
    if(WITH_ZLIB)
        target_link_libraries(trojita ${ZLIB_LIBRARIES})
    endif()

    add_executable(be.contacts ${be_contacts_SOURCES})
    target_link_libraries(be.contacts AbookAddressbook ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})
elseif(WITH_HARMATTAN)
    add_library(QmlModelGlue ${libQmlModelGlue_SOURCES})
    add_library(trojitaqnamwebviewplugin SHARED ${libQNAMWebView_SOURCES})
    add_executable(trojita-tp ${trojitaHarmattan_SOURCES})
    if(SUPPORTS_TARGET_INCLUDES)
        set_property(TARGET trojita-tp APPEND PROPERTY INCLUDE_DIRECTORIES
            ${QT_MKSPECS_DIR}/default ${CMAKE_CURRENT_SOURCE_DIR}/src/Harmattan/qmlapplicationviewer)
    endif()
    target_link_libraries(trojita-tp QmlModelGlue AppVersion Imap MSA Streams qwwsmtpclient Common Composer MimetypesQt4
        ${QT_QTSQL_LIBRARY} ${QT_QTNETWORK_LIBRARY} ${QT_QTWEBKIT_LIBRARY} ${QT_QTDECLARATIVE_LIBRARY})
endif()

if(WITH_QT5)
    qt5_use_modules(AppVersion Core)
    qt5_use_modules(Common Core Network)
    qt5_use_modules(Streams Network)
    qt5_use_modules(qwwsmtpclient Network)
    qt5_use_modules(MSA Network)
    qt5_use_modules(Composer Gui Network)
    qt5_use_modules(Imap Gui Network Sql)
    qt5_use_modules(DesktopGui Network WebKitWidgets)
    qt5_use_modules(AbookAddressbook Widgets)
    qt5_use_modules(be.contacts Widgets)
    qt5_use_modules(trojita Widgets Network)
endif()

if(WITH_SHARED_PLUGINS)
    install(TARGETS Plugins DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

if(WITH_DESKTOP)
    install(TARGETS trojita be.contacts RUNTIME DESTINATION bin)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui/trojita.desktop DESTINATION share/applications/)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/icons/trojita.png DESTINATION share/icons/hicolor/32x32/apps/)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/icons/trojita.svg DESTINATION share/icons/hicolor/scalable/apps/)
endif()

if(WITH_HARMATTAN)
    set(trojita_harmattan_path ${CMAKE_CURRENT_SOURCE_DIR}/src/Harmattan)
    install(TARGETS trojita-tp RUNTIME DESTINATION bin)
    install(TARGETS trojitaqnamwebviewplugin LIBRARY DESTINATION bin/net/flaska/QNAMWebView)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/QmlSupport/QNAMWebView/qmldir DESTINATION bin/net/flaska/QNAMWebView)
    install(DIRECTORY ${trojita_harmattan_path}/qml DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(FILES ${trojita_harmattan_path}/trojita-tp.desktop DESTINATION share/applications)
    install(FILES ${trojita_harmattan_path}/trojita-tp80.png DESTINATION share/icons/hicolor/80x80/apps)
endif()

if(WITH_NSIS)
    if(CMAKE_SIZEOF_VOID_P STREQUAL 4)
        set(MAKENSIS_OUTPUT Trojita-installer.exe)
    else()
        set(MAKENSIS_FLAGS -DX86_64 ${MAKENSIS_FLAGS})
        set(MAKENSIS_OUTPUT Trojita-installer-x86_64.exe)
    endif()

    if(NOT CMAKE_VERBOSE_MAKEFILE)
        set(MAKENSIS_FLAGS -V2 -DQUIET ${MAKENSIS_FLAGS})
    endif()

    set(MAKENSIS_FLAGS ${MAKENSIS_FLAGS} -NOCD)

    add_custom_command(OUTPUT ${MAKENSIS_OUTPUT}
        COMMAND ${MAKENSIS}
        ARGS ${MAKENSIS_FLAGS} ${CMAKE_CURRENT_SOURCE_DIR}/packaging/trojita.nsi
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/packaging/trojita.nsi ${CMAKE_CURRENT_BINARY_DIR}/trojita.exe version)
    add_custom_target(installer ALL DEPENDS ${MAKENSIS_OUTPUT})
endif()


if(WITH_TESTS)
    set(test_LibMailboxSync_SOURCES
        tests/Utils/ModelEvents.cpp
        tests/Utils/LibMailboxSync.cpp
    )
    add_library(test_LibMailboxSync ${test_LibMailboxSync_SOURCES})
    if(WITH_QT5)
        qt5_use_modules(test_LibMailboxSync Test Network)
    endif()
    if(SUPPORTS_TARGET_INCLUDES)
        set_property(TARGET test_LibMailboxSync APPEND PROPERTY INCLUDE_DIRECTORIES
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/Utils)
    endif()
    target_link_libraries(test_LibMailboxSync Imap MSA Streams Common Composer ${QT_QTTEST_LIBRARY} ${QT_QTCORE_LIBRARY})

    macro(trojita_test dir fname)
        set(test_${fname}_SOURCES tests/${dir}/test_${fname}.cpp)
        add_executable(test_${fname} ${test_${fname}_SOURCES})
        target_link_libraries(test_${fname} Imap MSA Streams Common Composer test_LibMailboxSync)
        if(WITH_QT5)
            qt5_use_modules(test_${fname} Network Sql Test Widgets)
        else()
            target_link_libraries(test_${fname} ${QT_QTSQL_LIBRARY} ${QT_QTTEST_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTNETWORK_LIBRARY})
        endif()
        if(WITH_ZLIB)
            target_link_libraries(test_${fname} ${ZLIB_LIBRARIES})
        endif()
        if(SUPPORTS_TARGET_INCLUDES)
            set_property(TARGET test_${fname} APPEND PROPERTY INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        endif()
        if(NOT CMAKE_CROSSCOMPILING)
            add_test(test_${fname} test_${fname})
        endif()
    endmacro()

    enable_testing()
    trojita_test(Composer Composer_Submission)
    trojita_test(Composer Composer_responses)
    trojita_test(Composer Html_formatting)
    if(WITH_QT5)
        qt5_use_modules(test_Composer_responses WebKitWidgets)
        qt5_use_modules(test_Html_formatting WebKitWidgets)
    else()
        target_link_libraries(test_Html_formatting ${QT_QTWEBKIT_LIBRARY})
    endif()
    trojita_test(Imap Imap_DisappearingMailboxes)
    trojita_test(Imap Imap_Idle)
    trojita_test(Imap Imap_LowLevelParser)
    trojita_test(Imap Imap_Message)
    trojita_test(Imap Imap_Model)
    trojita_test(Imap Imap_Parser_parse)
    trojita_test(Imap Imap_Responses)
    trojita_test(Imap Imap_SelectedMailboxUpdates)
    trojita_test(Imap Imap_Tasks_CreateMailbox)
    trojita_test(Imap Imap_Tasks_DeleteMailbox)
    trojita_test(Imap Imap_Tasks_ListChildMailboxes)
    trojita_test(Imap Imap_Tasks_ObtainSynchronizedMailbox)
    trojita_test(Imap Imap_Tasks_OpenConnection)
    trojita_test(Imap Imap_Threading)
    trojita_test(Imap Imap_BodyParts)
    trojita_test(Imap Imap_Offline)
    trojita_test(Imap Imap_CopyAndFlagOperations)
    trojita_test(Misc Rfc5322)
    trojita_test(Misc RingBuffer)
    trojita_test(Misc SenderIdentitiesModel)
    trojita_test(Misc SqlCache)
    trojita_test(Misc algorithms)
    trojita_test(Misc rfccodecs)
endif()

if(WIN32) # Check if we are on Windows
    if(MSVC10) # Check if we are using the Visual Studio compiler 2010
        # Because of linker errors (see http://stackoverflow.com/questions/5625884/conversion-of-stdwstring-to-qstring-throws-linker-error)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:wchar_t-")
    elseif(MINGW)
    else()
        message(WARNING "You are using a compiler which we have not tested yet (not MSVC10 or MINGW).")
        message(WARNING "Please let us know how well it works.")
    endif()
endif()

# FIXME: fix build warnings
