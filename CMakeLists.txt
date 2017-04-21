project(trojita)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Qt5's qt5_use_modules need 2.8.9+
# On win32, we need QtMain linking, which means 2.8.11+
# EL7 ships 2.8.11, Debian 8 ships 3.0.2, and Ubuntu 14.04 LTS has 2.8.12
cmake_minimum_required(VERSION 2.8.11)
cmake_policy(SET CMP0020 NEW)

if(POLICY CMP0054)
    # Silence warnings in TrojitaOption.cmake; we are fine with only doing the
    # expansion by hand
    cmake_policy(SET CMP0054 NEW)
endif()

if(POLICY CMP0043)
    # We make use of CMAKE_CXXFLAGS_DEBUG...
    cmake_policy(SET CMP0043 OLD)
endif()

if(CMAKE_VERSION VERSION_LESS "3.1")
    # If you aren't using and old Linux with an old GCC and old CMake, please just upgrade.
    set(CMAKE_CXX_FLAGS "--std=c++11 ${CMAKE_CXX_FLAGS}")
else()
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Set a default build type if none was specified. This was shamelessly stolen
# from VTK's cmake setup because these guys produce both CMake and a project that
# manipulates this variable, and the web is full of posts where people say that
# it is apparently evil to just set the build type in a way an earlier version of
# this patch did. Oh, and the location of this check/update matters, apparently.
#
# Yes, this is just plain crazy.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(TrojitaOption)

trojita_option(WITH_DESKTOP "Build desktop version" ON)
trojita_option(WITH_DBUS "Build with DBus library" AUTO)
trojita_option(WITH_RAGEL "Build with Ragel library" AUTO)
trojita_option(WITH_ZLIB "Build with zlib library" AUTO)
trojita_option(WITH_SHARED_PLUGINS "Enable shared dynamic plugins" ON)
trojita_option(BUILD_TESTING "Build tests" ON)
trojita_option(WITH_MIMETIC "Build with client-side MIME parsing" AUTO)
trojita_option(WITH_GPGMEPP "Use GpgME's native C++ bindings" AUTO)
trojita_option(WITH_KF5_GPGMEPP "Use legacy discontinued GpgME++ library from KDE frameworks" AUTO)

if(WIN32)
    trojita_option(WITH_NSIS "Build Windows NSIS installer" AUTO "WITH_DESKTOP")
endif()

if(UNIX AND NOT APPLE)
    set(QTKEYCHAIN_DEPENDS ";WITH_DBUS")
else()
    set(QTKEYCHAIN_DEPENDS "")
endif()

find_package(Qt5Core 5.2 REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Sql REQUIRED)
find_package(Qt5WebKitWidgets REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5LinguistTools)
find_package(Qt5Svg REQUIRED)
trojita_find_package(Qt5DBus "" "http://qt-project.org" "Qt5 D-Bus support" "Needed for IPC and for some plugins" WITH_DBUS)
trojita_find_package(Qt5Test "" "http://qt-project.org" "Qt5 QTest library" "Needed for automated tests" BUILD_TESTING)
if(Qt5LinguistTools_FOUND)
    find_package(Qt5LinguistForTrojita)
endif()

trojita_plugin_option(WITH_ABOOKADDRESSBOOK_PLUGIN "Build AbookAddressbook plugin" STATIC)
trojita_plugin_option(WITH_CLEARTEXT_PLUGIN "Build Cleartext password plugin" STATIC)
trojita_plugin_option(WITH_QTKEYCHAIN_PLUGIN "Build Qtkeychain password plugin" "${QTKEYCHAIN_DEPENDS}")

trojita_find_package(Git "" "" "" "")

trojita_find_package(Mimetic "" "http://www.codesink.org/mimetic_mime_library.html" "C++ MIME Library" "Required for client-side MIME parsing" WITH_MIMETIC)
trojita_find_package(Gpgmepp "1.8.0" "https://gnupg.org/related_software/gpgme/index.html" "C++/Qt bindings for gpgme" "Needed for encrypted/signed e-mails" WITH_GPGMEPP)
if(NOT WITH_GPGMEPP)
    trojita_find_package(KF5Gpgmepp "" "https://commits.kde.org/gpgmepp?path=/" "C++ bindings for gpgme" "Needed for encrypted/signed e-mails" WITH_KF5_GPGMEPP)
    trojita_option(WITH_CRYPTO_MESSAGES "Enable support for encrypted messages" AUTO "WITH_MIMETIC;WITH_KF5_GPGMEPP")
else()
    trojita_option(WITH_CRYPTO_MESSAGES "Enable support for encrypted messages" AUTO "WITH_MIMETIC;WITH_GPGMEPP")
endif()

if(WIN32)
    trojita_find_package(MakeNSIS "" "http://nsis.sourceforge.net" "Nullsoft Scriptable Install System" "Needed for building Windows installer" WITH_NSIS)
endif()

# Add support for Mingw RC compiler
if(WIN32)
    enable_language(RC)
    include(CMakeDetermineRCCompiler)

    if(MINGW)
        set(CMAKE_RC_COMPILER_INIT windres)
        set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -I${CMAKE_CURRENT_BINARY_DIR} -i <SOURCE> -o <OBJECT>")
    endif()
endif()

trojita_find_package(Qt5Keychain QUIET "https://github.com/frankosterfeld/qtkeychain" "QtKeychain library (Qt5 version)" "Needed for QtKeychain password plugin" WITH_QTKEYCHAIN_PLUGIN)
if(Qt5Keychain_FOUND OR QtKeychain_FOUND)
    message(STATUS "Found QtKeychain library (includes at ${QTKEYCHAIN_INCLUDE_DIRS}, lib at ${QTKEYCHAIN_LIBRARIES})")
else()
    message(STATUS "Could not find QtKeychain library")
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
    if(IS_ABSOLUTE ${CMAKE_INSTALL_PLUGIN_DIR})
        set(PLUGIN_DIR "${CMAKE_INSTALL_PLUGIN_DIR}")
    else()
        set(PLUGIN_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_PLUGIN_DIR}")
    endif()
endif()
mark_as_advanced(PLUGIN_DIR)

include(GNUInstallDirs)

# When manipulating CXXFLAGS, we put the user's CXXFLAGS *after* that so that they take priority.
if(MSVC)
    # See below for some reationale for these optimizations
    set(CMAKE_CXX_FLAGS "/O2 ${CMAKE_CXX_FLAGS}")

    # We have no information about the warnings and their usefullness. Reports are welcome.
    # We might enable warnings on MSVC in future.
else()
    # -Werror is not a default for sanity reasons (one cannot know what warnings a future compiler
    # might bring along), but it's a default in debug mode. The idea is that developers should care
    # about a warning-free build, and that this is easier than messing with yet another configure option.
    set(CMAKE_CXX_FLAGS_DEBUG "-Werror ${CMAKE_CXX_FLAGS_DEBUG}")
    # Also see CMP0043...

    # Optimizations are enabled unconditionally because they make a big difference in the speed of the
    # resulting binaries, and that it is better to allow an opt-out from them by adjusting CXXFLAGS through
    # an env var at cmake time if needed.
    # The reason for not manipulating just CMAKE_CXX_FLAGS_DEBUG is that unrecognized build types ("DebugFull")
    # should still benefit from these optimizations. Yup, it would be even better if CMake did a sane thing
    # and warned when users set an unrecognized and unused build type, but that just isn't the case.
    set(CMAKE_CXX_FLAGS "-O2 ${CMAKE_CXX_FLAGS}")

    # Build warnings are useful tools (and Trojita should be warning-free anyway), enable them on all
    # configurations. They are warnings, not errors.
    set(CMAKE_CXX_FLAGS "-Wall -Wsign-compare ${CMAKE_CXX_FLAGS}")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
# The following is required so that the moc_*.cpp and ui_*.h are found
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-DQT_STRICT_ITERATORS)
add_definitions(-DQT_USE_QSTRINGBUILDER)
add_definitions(-DQT_USE_FAST_OPERATOR_PLUS)
add_definitions(-DQT_USE_FAST_CONCATENATION)

if(NOT MSVC)
  # We're using C++11's threading features (std::async in particular), and that requires "some threading". With GCC and
  # Clang, this is implemented through the -pthread build flag. Without using these bits, linking fails on Fedora 23,
  # and this is apparently a slightly different failure than the Kf5::Gpgmepp-pthread one in commit
  # 12e41101070f7073caec653185c0504763672ee7.
  #
  # Apparently, there's been various methods on how to enable this in the most-cmakeish-way throughout the years, with
  # cmake-3.1+ supporting some magic linking via the Threads::Threads option on a per-library basis. However, I am not
  # really looking into that wonderful fun of mixing -pthread and non-pthread translation units, so let's use a big
  # hammer and set it unconditionally on platforms which use it anyway.
  #
  # And because we also support MinGW and its `windres` compiler, we have to avoid passing -pthread to *that* thing, so,
  # well, let's cheat and put it into the CXXFLAGS.
  set(CMAKE_CXX_FLAGS "-pthread ${CMAKE_CXX_FLAGS}")
endif()

# Make sure that plugins not export all symbols, only that which are explicitly marked
include(GenerateExportHeader)
add_compiler_export_flags()

set(CMAKE_AUTOMOC True)

trojita_find_package(RagelForTrojita "" "" "" "" WITH_RAGEL)
trojita_find_package(ZLIB "" "" "" "" WITH_ZLIB)

if(WITH_MIMETIC)
  set(TROJITA_HAVE_MIMETIC True)
else()
  set(TROJITA_HAVE_MIMETIC False)
endif()

if(WITH_GPGMEPP OR WITH_KF5_GPGMEPP)
  set(TROJITA_HAVE_GPGMEPP True)
else()
  set(TROJITA_HAVE_GPGMEPP False)
endif()

if(WITH_CRYPTO_MESSAGES)
  set(TROJITA_HAVE_CRYPTO_MESSAGES True)
else()
  set(TROJITA_HAVE_CRYPTO_MESSAGES False)
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
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/configure-plugins.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/configure-plugins.cmake.h)

feature_summary(FATAL_ON_MISSING_REQUIRED_PACKAGES INCLUDE_QUIET_PACKAGES DESCRIPTION "\n" WHAT ALL)

set(path_Common ${CMAKE_CURRENT_SOURCE_DIR}/src/Common)
set(libCommon_SOURCES
    ${path_Common}/Application.cpp
    ${path_Common}/ConnectionId.cpp
    ${path_Common}/DeleteAfter.cpp
    ${path_Common}/FileLogger.cpp
    ${path_Common}/MetaTypes.cpp
    ${path_Common}/Paths.cpp
    ${path_Common}/SettingsNames.cpp
    ${path_Common}/StashingReverseIterator.h
)

set(path_Plugins ${CMAKE_CURRENT_SOURCE_DIR}/src/Plugins)
set(libPlugins_SOURCES
    ${path_Plugins}/AddressbookPlugin.cpp
    ${path_Plugins}/PasswordPlugin.cpp
    ${path_Plugins}/PluginJob.cpp
    ${path_Plugins}/PluginManager.cpp
)

set(path_UiUtils ${CMAKE_CURRENT_SOURCE_DIR}/src/UiUtils)
set(libUiUtils_SOURCES
    ${path_UiUtils}/Color.cpp
    ${path_UiUtils}/Formatting.cpp
    ${path_UiUtils}/IconLoader.cpp
    ${path_UiUtils}/PartLoadingOptions.h
    ${path_UiUtils}/PartVisitor.h
    ${path_UiUtils}/PartWalker.h
    ${path_UiUtils}/PartWalker_impl.h
    ${path_UiUtils}/PasswordWatcher.cpp
    ${path_UiUtils}/PlainTextFormatter.cpp
    ${path_UiUtils}/QaimDfsIterator.cpp
)

set(path_IPC ${CMAKE_CURRENT_SOURCE_DIR}/src/IPC)

if(WITH_DBUS)
    set(libIPC_SOURCES
        ${path_IPC}/DBusInterface.cpp
        ${path_IPC}/MainWindowBridge.cpp
    )
else()
    set(libIPC_SOURCES
        ${path_IPC}/None.cpp
    )
endif()

set(path_Composer ${CMAKE_CURRENT_SOURCE_DIR}/src/Composer)
set(libComposer_SOURCES
    ${path_Composer}/AbstractComposer.cpp
    ${path_Composer}/ComposerAttachments.cpp
    ${path_Composer}/ExistingMessageComposer.cpp
    ${path_Composer}/Mailto.cpp
    ${path_Composer}/MessageComposer.cpp
    ${path_Composer}/QuoteText.cpp
    ${path_Composer}/Recipients.cpp
    ${path_Composer}/ReplaceSignature.cpp
    ${path_Composer}/SenderIdentitiesModel.cpp
    ${path_Composer}/SubjectMangling.cpp
    ${path_Composer}/Submission.cpp
)

set(path_MSA ${CMAKE_CURRENT_SOURCE_DIR}/src/MSA)
set(libMSA_SOURCES
    ${path_MSA}/AbstractMSA.cpp
    ${path_MSA}/Account.cpp
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

set(path_Cryptography ${CMAKE_CURRENT_SOURCE_DIR}/src/Cryptography)
set(libCryptography_SOURCES
    ${path_Cryptography}/MessageModel.cpp
    ${path_Cryptography}/MessagePart.cpp
    ${path_Cryptography}/PartReplacer.cpp
)
if(WITH_MIMETIC)
    set(libCryptography_SOURCES
        ${libCryptography_SOURCES}
        ${path_Cryptography}/LocalMimeParser.cpp
        ${path_Cryptography}/MimeticUtils.cpp
    )
endif()
if(WITH_CRYPTO_MESSAGES)
    set(libCryptography_SOURCES
        ${libCryptography_SOURCES}
        ${path_Cryptography}/GpgMe++.cpp
    )
endif()

if(WITH_ZLIB)
    set(libStreams_SOURCES ${libStreams_SOURCES}
        ${path_Streams}/3rdparty/rfc1951.cpp)
endif()

set(path_DesktopGui ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui)
set(libDesktopGui_SOURCES
    ${path_DesktopGui}/AddressRowWidget.cpp
    ${path_DesktopGui}/AttachmentView.cpp
    ${path_DesktopGui}/ColoredItemDelegate.cpp
    ${path_DesktopGui}/CompleteMessageWidget.cpp
    ${path_DesktopGui}/ComposeWidget.cpp
    ${path_DesktopGui}/ComposerAttachmentsList.cpp
    ${path_DesktopGui}/ComposerTextEdit.cpp
    ${path_DesktopGui}/EmbeddedWebView.cpp
    ${path_DesktopGui}/EnvelopeView.cpp
    ${path_DesktopGui}/ExternalElementsWidget.cpp
    ${path_DesktopGui}/FindBar.cpp
    ${path_DesktopGui}/FindBarMixin.cpp
    ${path_DesktopGui}/FlowLayout.cpp
    ${path_DesktopGui}/FromAddressProxyModel.cpp
    ${path_DesktopGui}/LineEdit.cpp
    ${path_DesktopGui}/LoadablePartWidget.cpp
    ${path_DesktopGui}/MailBoxTreeView.cpp
    ${path_DesktopGui}/MessageHeadersWidget.cpp
    ${path_DesktopGui}/MessageListWidget.cpp
    ${path_DesktopGui}/MessageSourceWidget.cpp
    ${path_DesktopGui}/MessageView.cpp
    ${path_DesktopGui}/MsgListView.cpp
    ${path_DesktopGui}/OnePanelAtTimeWidget.cpp
    ${path_DesktopGui}/OneEnvelopeAddress.cpp
    ${path_DesktopGui}/OverlayWidget.cpp
    ${path_DesktopGui}/PartWalker.cpp
    ${path_DesktopGui}/PartWidget.cpp
    ${path_DesktopGui}/PartWidgetFactoryVisitor.cpp
    ${path_DesktopGui}/PasswordDialog.cpp
    ${path_DesktopGui}/ProgressPopUp.cpp
    ${path_DesktopGui}/ProtocolLoggerWidget.cpp
    ${path_DesktopGui}/ReplaceCharValidator.cpp
    ${path_DesktopGui}/SettingsDialog.cpp
    ${path_DesktopGui}/SimplePartWidget.cpp
    ${path_DesktopGui}/Spinner.cpp
    ${path_DesktopGui}/TagAddDialog.cpp
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
    ${path_DesktopGui}/EditFavoriteTag.ui
    ${path_DesktopGui}/PasswordDialog.ui
    ${path_DesktopGui}/ProgressPopUp.ui
    ${path_DesktopGui}/SettingsCachePage.ui
    ${path_DesktopGui}/SettingsGeneralPage.ui
    ${path_DesktopGui}/SettingsImapPage.ui
    ${path_DesktopGui}/SettingsOutgoingPage.ui
    ${path_DesktopGui}/SettingsFavoriteTagsPage.ui
    ${path_DesktopGui}/TagAddDialog.ui
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
    ${path_Imap}/Network/QQuickNetworkReplyWrapper.cpp

    ${path_Imap}/Model/Cache.cpp
    ${path_Imap}/Model/CombinedCache.cpp
    ${path_Imap}/Model/DragAndDrop.cpp
    ${path_Imap}/Model/DiskPartCache.cpp
    ${path_Imap}/Model/DummyNetworkWatcher.cpp
    ${path_Imap}/Model/FindInterestingPart.cpp
    ${path_Imap}/Model/FlagsOperation.cpp
    ${path_Imap}/Model/FullMessageCombiner.cpp
    ${path_Imap}/Model/ImapAccess.cpp
    ${path_Imap}/Model/MailboxFinder.cpp
    ${path_Imap}/Model/MailboxMetadata.cpp
    ${path_Imap}/Model/MailboxModel.cpp
    ${path_Imap}/Model/MailboxTree.cpp
    ${path_Imap}/Model/MemoryCache.cpp
    ${path_Imap}/Model/Model.cpp
    ${path_Imap}/Model/MsgListModel.cpp
    ${path_Imap}/Model/NetworkWatcher.cpp
    ${path_Imap}/Model/OneMessageModel.cpp
    ${path_Imap}/Model/FavoriteTagsModel.cpp
    ${path_Imap}/Model/ParserState.cpp
    ${path_Imap}/Model/PrettyMailboxModel.cpp
    ${path_Imap}/Model/PrettyMsgListModel.cpp
    ${path_Imap}/Model/SpecialFlagNames.cpp
    ${path_Imap}/Model/SQLCache.cpp
    ${path_Imap}/Model/SubtreeModel.cpp
    ${path_Imap}/Model/SystemNetworkWatcher.cpp
    ${path_Imap}/Model/TaskFactory.cpp
    ${path_Imap}/Model/TaskPresentationModel.cpp
    ${path_Imap}/Model/ThreadingMsgListModel.cpp
    ${path_Imap}/Model/Utils.cpp
    ${path_Imap}/Model/VisibleTasksModel.cpp

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

# The ModelTest is only needed when debugging manually
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND libImap_SOURCES ${path_Imap}/Model/ModelTest/modeltest.cpp)
endif()

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

set(trojita_desktop_SOURCES
    ${path_DesktopGui}/main.cpp
)

if(WIN32)
    list(APPEND trojita_desktop_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/trojita_win32.rc)
    set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/trojita_win32.rc APPEND PROPERTY OBJECT_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/icons/trojita.ico
        ${CMAKE_CURRENT_BINARY_DIR}/trojita-git-version.h
        ${CMAKE_CURRENT_BINARY_DIR}/trojita-version.h
    )
endif()

if(Qt5LinguistForTrojita_FOUND)
    file(GLOB_RECURSE lang_PO "${CMAKE_CURRENT_SOURCE_DIR}/po/trojita_common_*.po")
    qt5_wrap_po(trojita_QM ${lang_PO})
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
            install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/locale/ DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/trojita/locale" REGEX "(x_test)|(.*\\.ts)" EXCLUDE)
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
    COMMAND ${CMAKE_COMMAND} -DGIT_EXECUTABLE=${GIT_EXECUTABLE} -DNSIS=${NSIS} -DHOST_ARCH=${HOST_ARCH} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/TrojitaVersion.cmake)
set_source_files_properties(${version_files}
    PROPERTIES GENERATED TRUE
    HEADER_FILE_ONLY TRUE)

add_library(Common STATIC ${libCommon_SOURCES})
set_property(TARGET Common APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
add_dependencies(Common version)
target_link_libraries(Common Qt5::Network)

add_library(AppVersion STATIC ${libAppVersion_SOURCES})
set_property(TARGET AppVersion APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
add_dependencies(AppVersion version)
target_link_libraries(AppVersion Common)
target_link_libraries(AppVersion Qt5::Core)

if(WITH_SHARED_PLUGINS)
    add_library(Plugins SHARED ${libPlugins_SOURCES})
    set_property(TARGET Plugins APPEND PROPERTY COMPILE_DEFINITIONS BUILDING_LIBTROJITA_PLUGINS)
else()
    add_library(Plugins STATIC ${libPlugins_SOURCES})
    set_property(TARGET Plugins APPEND PROPERTY COMPILE_DEFINITIONS QT_STATICPLUGIN)
endif()
set_target_properties(Plugins PROPERTIES OUTPUT_NAME trojita_plugins)
set_property(TARGET Plugins APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(Plugins Qt5::Core)

add_library(UiUtils STATIC ${libUiUtils_SOURCES})
set_property(TARGET UiUtils APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(UiUtils Plugins Common Qt5::Gui)

add_library(Streams STATIC ${libStreams_SOURCES})
set_property(TARGET Streams APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
if(WITH_ZLIB)
    set_property(TARGET Streams APPEND PROPERTY INCLUDE_DIRECTORIES ${ZLIB_INCLUDE_DIR})
    target_link_libraries(Streams ${ZLIB_LIBRARIES})
endif()
target_link_libraries(Streams Qt5::Network)

add_library(IPC STATIC ${libIPC_SOURCES})
set_property(TARGET IPC APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
if(WITH_DBUS)
    target_link_libraries(IPC Qt5::DBus Qt5::Widgets)
else()
    target_link_libraries(IPC Qt5::Core)
endif()

add_library(qwwsmtpclient STATIC ${libqwwsmtpclient_SOURCES})
target_link_libraries(qwwsmtpclient Qt5::Network)

add_library(MSA STATIC ${libMSA_SOURCES})
set_property(TARGET MSA APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(MSA Imap Streams qwwsmtpclient)

add_library(Composer STATIC ${libComposer_SOURCES})
set_property(TARGET Composer APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(Composer Common MSA Streams UiUtils qwwsmtpclient)

add_library(Imap STATIC ${libImap_SOURCES})
set_property(TARGET Imap APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(Imap Common Streams UiUtils Qt5::Sql)

add_library(Cryptography STATIC ${libCryptography_SOURCES})
set_property(TARGET Cryptography APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(Cryptography Common Imap)
if(WITH_MIMETIC)
    target_link_libraries(Cryptography ${MIMETIC_LIBRARIES})
    set_property(TARGET Cryptography APPEND PROPERTY INCLUDE_DIRECTORIES ${MIMETIC_INCLUDE_DIRS})
endif()
if(WITH_CRYPTO_MESSAGES)
    if(WITH_GPGMEPP)
        target_link_libraries(Cryptography Gpgmepp QGpgme)
    elseif(WITH_KF5_GPGMEPP)
        if(WIN32)
            target_link_libraries(Cryptography KF5::Gpgmepp KF5::QGpgme)
        else()
            target_link_libraries(Cryptography KF5::Gpgmepp-pthread KF5::QGpgme)
        endif()
    endif()
endif()

## ClearText password plugin
if(WITH_CLEARTEXT_PLUGIN)
    trojita_add_plugin(trojita_plugin_ClearTextPasswordPlugin WITH_CLEARTEXT_PLUGIN src/Plugins/ClearTextPassword/ClearTextPassword.cpp)
endif()

## QtKeyChain plugin
if(WITH_QTKEYCHAIN_PLUGIN)
    trojita_add_plugin(trojita_plugin_QtKeychainPasswordPlugin WITH_QTKEYCHAIN_PLUGIN src/Plugins/QtKeyChain/QtKeyChainPassword.cpp)
    target_link_libraries(trojita_plugin_QtKeychainPasswordPlugin ${QTKEYCHAIN_LIBRARIES} Qt5::DBus)
    set_property(TARGET trojita_plugin_QtKeychainPasswordPlugin APPEND PROPERTY INCLUDE_DIRECTORIES ${QTKEYCHAIN_INCLUDE_DIRS})
endif()

## AbookAddressbook plugin
if(WITH_ABOOKADDRESSBOOK_PLUGIN)
    set(path_AbookAddressbook ${CMAKE_CURRENT_SOURCE_DIR}/src/Plugins/AbookAddressbook)
    set(libAbookAddressbook_HEADERS
        ${path_AbookAddressbook}/AbookAddressbook.h
        ${path_AbookAddressbook}/be-contacts.h
    )
    set(libAbookAddressbook_SOURCES
        ${path_AbookAddressbook}/AbookAddressbook.cpp
        ${path_AbookAddressbook}/be-contacts.cpp
    )
    set(libAbookAddressbook_UI
        ${path_AbookAddressbook}/be-contacts.ui
        ${path_AbookAddressbook}/onecontact.ui
    )

    qt5_wrap_ui(libAbookAddressbook_UI_OUT ${libAbookAddressbook_UI})

    trojita_add_plugin(trojita_plugin_AbookAddressbookPlugin WITH_ABOOKADDRESSBOOK_PLUGIN ${libAbookAddressbook_SOURCES} ${libAbookAddressbook_UI_OUT})
    set_property(TARGET trojita_plugin_AbookAddressbookPlugin APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)

    target_link_libraries(trojita_plugin_AbookAddressbookPlugin Qt5::Widgets)

    set(be_contacts_SOURCES
        ${path_AbookAddressbook}/main.cpp
    )

    add_executable(be.contacts WIN32 ${be_contacts_SOURCES})
    set_property(TARGET be.contacts APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
    target_link_libraries(be.contacts Plugins)
    if("${WITH_ABOOKADDRESSBOOK_PLUGIN}" STREQUAL "STATIC")
        set_property(TARGET be.contacts APPEND PROPERTY COMPILE_DEFINITIONS QT_STATICPLUGIN)
        target_link_libraries(be.contacts trojita_plugin_AbookAddressbookPlugin)
    endif()
    target_link_libraries(be.contacts Qt5::Widgets)
endif()

# Generate file static_plugins.h.in
get_property(STATIC_PLUGINS GLOBAL PROPERTY TROJITA_STATIC_PLUGINS)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/static_plugins.h.in "#include <QtPlugin>\n")
foreach(PLUGIN ${STATIC_PLUGINS})
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/static_plugins.h.in "Q_IMPORT_PLUGIN(${PLUGIN})\n")
endforeach()
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/static_plugins.h.in ${CMAKE_CURRENT_BINARY_DIR}/static_plugins.h)

if(WITH_DESKTOP)
    qt5_wrap_ui(libDesktopGui_UI_OUT ${libDesktopGui_UI})
    qt5_add_resources(libDesktopGui_RESOURCES_OUT ${libDesktopGui_RESOURCES})

    add_library(DesktopGui STATIC ${libDesktopGui_SOURCES} ${libDesktopGui_UI_OUT} ${libDesktopGui_RESOURCES_OUT})
    set_property(TARGET DesktopGui APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
    # The following is needed for the LineEdit widget within the .ui files.
    # The ${path_DesktopGui} is needed so that the generated ui_*.h file can find the headers of the custom widgets
    set_property(TARGET DesktopGui APPEND PROPERTY INCLUDE_DIRECTORIES ${path_DesktopGui})
    target_link_libraries(DesktopGui Common UiUtils Composer Cryptography Imap IPC MSA Plugins Streams qwwsmtpclient Qt5::WebKitWidgets)

    # On Windows build a real Win32 GUI application without console window
    # On other platforms WIN32 flag is ignored
    add_executable(trojita WIN32 ${trojita_desktop_SOURCES} ${trojita_QM})
    set_property(TARGET trojita APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
    target_link_libraries(trojita AppVersion Common UiUtils DesktopGui ${STATIC_PLUGINS})
endif()


if(WITH_SHARED_PLUGINS)
    install(TARGETS Plugins DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

include(SanitizedDesktopFile)

if(WITH_ABOOKADDRESSBOOK_PLUGIN)
    install(TARGETS be.contacts RUNTIME DESTINATION bin)
endif()

if(WITH_DESKTOP)
    copy_desktop_file_without_cruft("${CMAKE_CURRENT_SOURCE_DIR}/src/Gui/org.kde.trojita.desktop" "${CMAKE_CURRENT_BINARY_DIR}/org.kde.trojita-DesktopGui.desktop")
    install(TARGETS trojita RUNTIME DESTINATION bin)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.trojita-DesktopGui.desktop DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications/" RENAME org.kde.trojita.desktop)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui/org.kde.trojita.appdata.xml DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo/")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/icons/trojita.png DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps/")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/icons/trojita.svg DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps/")
endif()

if(WITH_NSIS)
    include(TrojitaNSIS)
endif()


if(BUILD_TESTING)
    set(test_LibMailboxSync_SOURCES
        tests/Utils/ModelEvents.cpp
        tests/Utils/LibMailboxSync.cpp
    )
    add_library(test_LibMailboxSync STATIC ${test_LibMailboxSync_SOURCES})
    set_property(TARGET test_LibMailboxSync APPEND PROPERTY INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/Utils)
    target_link_libraries(test_LibMailboxSync Imap MSA Streams Common Composer Qt5::Test)

    macro(trojita_test dir fname)
        set(test_${fname}_SOURCES tests/${dir}/test_${fname}.cpp)
        add_executable(test_${fname} ${test_${fname}_SOURCES})
        target_link_libraries(test_${fname} Imap MSA Streams Common Composer Cryptography test_LibMailboxSync)
        set_property(TARGET test_${fname} APPEND PROPERTY INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        if(NOT CMAKE_CROSSCOMPILING)
            add_test(test_${fname} test_${fname})
        endif()
    endmacro()

    set(UBSAN_ENV_SUPPRESSIONS "UBSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/tests/ubsan.supp")

    enable_testing()
    trojita_test(Composer Composer_Submission)
    trojita_test(Composer Composer_Existing)
    trojita_test(Composer Composer_responses)
    target_link_libraries(test_Composer_responses Qt5::WebKitWidgets)
    trojita_test(Composer Html_formatting)
    target_link_libraries(test_Html_formatting Qt5::WebKitWidgets)
    trojita_test(Imap Imap_DisappearingMailboxes)
    trojita_test(Imap Imap_Idle)
    trojita_test(Imap Imap_LowLevelParser)
    trojita_test(Imap Imap_Message)
    trojita_test(Imap Imap_Model)
    trojita_test(Imap Imap_MsgPartNetAccessManager)
    set_property(TEST test_Imap_MsgPartNetAccessManager PROPERTY ENVIRONMENT "${UBSAN_ENV_SUPPRESSIONS}")
    trojita_test(Imap Imap_Parser_parse)
    trojita_test(Imap Imap_Parser_write)
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
    trojita_test(Cryptography Cryptography_MessageModel)

    if(WITH_CRYPTO_MESSAGES)
      find_program(GPGCONF_BINARY NAMES gpgconf)
      if(GPGCONF_BINARY_NOTFOUND)
        message(SEND_ERROR "The `gpgconf` binary from GnuPG not found, this is needed for crypto tests.")
      endif()

      if(NOT UNIX)
        message(SEND_ERROR "The Cryptography unit tests really need Unix. Patches welcome.")
      endif()

      add_library(fake-dev-random SHARED ${CMAKE_CURRENT_SOURCE_DIR}/tests/Utils/fake-dev-random.c)
      set_target_properties(fake-dev-random PROPERTIES AUTOMOC off)
      target_link_libraries(fake-dev-random dl)

      # FIXME: it would be nice to depend on the contents of keys/, but in my testing it produces Makefiles
      # which suffer from races (the keygen.sh is run multiple times in parallel within the CI environment).
      # I wasn't able to track down the root cause behind this; it affected all cmake versions within the CI
      # as of Feb 2016, which is 3.1.something up to 3.3.something.
      add_custom_command(OUTPUT crypto_test_data.h
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/Cryptography/keygen.sh ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS fake-dev-random tests/Cryptography/keygen.sh tests/Cryptography/batch-keygen)
      add_custom_target(crypto_test_data DEPENDS crypto_test_data.h)

      trojita_test(Cryptography Cryptography_PGP)
      set_property(TEST test_Cryptography_PGP PROPERTY ENVIRONMENT "${UBSAN_ENV_SUPPRESSIONS}")
      add_dependencies(test_Cryptography_PGP crypto_test_data)
    endif()

    trojita_test(Misc Rfc5322)
    trojita_test(Misc RingBuffer)
    trojita_test(Misc SenderIdentitiesModel)
    trojita_test(Misc SqlCache)
    trojita_test(Misc algorithms)
    trojita_test(Misc rfccodecs)
    trojita_test(Misc prettySize)
    trojita_test(Misc Formatting)
    trojita_test(Misc QaimDfsIterator)
    trojita_test(Misc FavoriteTagsModel)

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
