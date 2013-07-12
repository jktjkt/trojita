if(WITH_QT5)
    # The Qt5's qt5_use_modules is only available on 2.8.9 or later -- sweet, isn't it?
    cmake_minimum_required(VERSION 2.8.9)
else()
    cmake_minimum_required(VERSION 2.8.7)
endif()
project(trojita)
SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if(HARMATTAN)
    set(BUILD_DESKTOP_GUI false)
    set(BUILD_HARMATTAN true)
    set(WITHOUT_TESTS true)
    message(STATUS "Building version for MeeGo Harmattan")
else()
    set(BUILD_DESKTOP_GUI true)
    set(BUILD_HARMATTAN false)
    message(STATUS "Building the desktop version")
endif()

if(WITH_QT5)
    message(STATUS "Building the Qt5 version")
    find_package(Qt5Gui REQUIRED)
    find_package(Qt5Network REQUIRED)
    find_package(Qt5Sql REQUIRED)
    find_package(Qt5WebKitWidgets REQUIRED)
    find_package(Qt5Widgets REQUIRED)
    find_package(Qt5LinguistTools REQUIRED)
    if(NOT WITHOUT_TESTS)
        find_package(Qt5Test REQUIRED)
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
    find_package(Qt5LinguistForTrojita)
else()
    message(STATUS "Building the Qt4 version")
    set(QT_USE_QTNETWORK 1)
    set(QT_USE_QTSQL 1)
    set(QT_USE_QTWEBKIT 1)
    if(NOT WITHOUT_TESTS)
        set(QT_USE_QTTEST 1)
    endif()
    if(BUILD_HARMATTAN)
        set(QT_USE_QTDECLARATIVE 1)
    endif()
    find_package(Qt4 4.6 REQUIRED)
    include(${QT_USE_FILE})
    add_definitions(${QT_DEFINITIONS})
    find_package(LinguistForTrojita)
endif()
find_package(Git)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
# The following is required so that the moc_*.cpp and ui_*.h are found
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_definitions(-DQT_STRICT_ITERATORS)

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

if(WITH_RAGEL)
    find_package(RagelForTrojita)
endif()

if(NOT WITHOUT_ZLIB)
    find_package(ZLIB)
endif()

if(ZLIB_FOUND)
    add_definitions(-DTROJITA_HAVE_ZLIB)
    message(STATUS "Support for COMPRESS=DEFLATE enabled")
else()
    message(STATUS "Disabling COMPRESS=DEFLATE, zlib is not available")
endif()


set(path_Common ${CMAKE_CURRENT_SOURCE_DIR}/src/Common)
set(libCommon_HEADERS
    ${path_Common}/ConnectionId.h
    ${path_Common}/DeleteAfter.h
    ${path_Common}/FileLogger.h
    ${path_Common}/SettingsNames.h
)
set(libCommon_SOURCES
    ${path_Common}/ConnectionId.cpp
    ${path_Common}/DeleteAfter.cpp
    ${path_Common}/FileLogger.cpp
    ${path_Common}/SettingsNames.cpp
)

set(path_Composer ${CMAKE_CURRENT_SOURCE_DIR}/src/Composer)
set(libComposer_HEADERS
    ${path_Composer}/ComposerAttachments.h
    ${path_Composer}/MessageComposer.h
    ${path_Composer}/PlainTextFormatter.h
    ${path_Composer}/Recipients.h
    ${path_Composer}/ReplaceSignature.h
    ${path_Composer}/SenderIdentitiesModel.h
    ${path_Composer}/SubjectMangling.h
    ${path_Composer}/Submission.h
)
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
set(libMSA_HEADERS
    ${path_MSA}/AbstractMSA.h
    ${path_MSA}/FakeMSA.h
    ${path_MSA}/ImapSubmit.h
    ${path_MSA}/SMTP.h
    ${path_MSA}/Sendmail.h
)
set(libMSA_SOURCES
    ${path_MSA}/AbstractMSA.cpp
    ${path_MSA}/FakeMSA.cpp
    ${path_MSA}/ImapSubmit.cpp
    ${path_MSA}/SMTP.cpp
    ${path_MSA}/Sendmail.cpp
)

set(path_Streams ${CMAKE_CURRENT_SOURCE_DIR}/src/Streams)
set(libStreams_HEADERS
    ${path_Streams}/DeletionWatcher.h
    ${path_Streams}/FakeSocket.h
    ${path_Streams}/IODeviceSocket.h
    ${path_Streams}/Socket.h
    ${path_Streams}/SocketFactory.h
)
set(libStreams_SOURCES
    ${path_Streams}/DeletionWatcher.cpp
    ${path_Streams}/FakeSocket.cpp
    ${path_Streams}/IODeviceSocket.cpp
    ${path_Streams}/Socket.cpp
    ${path_Streams}/SocketFactory.cpp
)

if(ZLIB_FOUND)
    set(libStreams_SOURCES ${libStreams_SOURCES}
        ${path_Streams}/3rdparty/rfc1951.cpp)
endif()

set(path_mimetypesqt4 ${CMAKE_CURRENT_SOURCE_DIR}/src/mimetypes-qt4)
set(libMimetypesQt4_HEADERS
    ${path_mimetypesqt4}/io/qstandardpaths.h
    ${path_mimetypesqt4}/mimetypes/qmimedatabase_p.h
    ${path_mimetypesqt4}/mimetypes/qmimeglobpattern_p.h
    ${path_mimetypesqt4}/mimetypes/qmimemagicrule_p.h
    ${path_mimetypesqt4}/mimetypes/qmimemagicrulematcher_p.h
    ${path_mimetypesqt4}/mimetypes/qmimeprovider_p.h
    ${path_mimetypesqt4}/mimetypes/qmimetype_p.h
    ${path_mimetypesqt4}/mimetypes/qmimetypeparser_p.h
    ${path_mimetypesqt4}/mimetypes/qmimetype.h
    ${path_mimetypesqt4}/mimetypes/qmimedatabase.h
)
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

set(path_DesktopGui ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui)
set(libDesktopGui_HEADERS
    ${path_DesktopGui}/AttachmentView.h
    ${path_DesktopGui}/AutoCompletion.h
    ${path_DesktopGui}/CompleteMessageWidget.h
    ${path_DesktopGui}/ComposeWidget.h
    ${path_DesktopGui}/ComposerAttachmentsList.h
    ${path_DesktopGui}/ComposerTextEdit.h
    ${path_DesktopGui}/EmbeddedWebView.h
    ${path_DesktopGui}/EnvelopeView.h
    ${path_DesktopGui}/ExternalElementsWidget.h
    ${path_DesktopGui}/FindBar.h
    ${path_DesktopGui}/FlowLayout.h
    ${path_DesktopGui}/FromAddressProxyModel.h
    ${path_DesktopGui}/LineEdit.h
    ${path_DesktopGui}/LoadablePartWidget.h
    ${path_DesktopGui}/MailBoxTreeView.h
    ${path_DesktopGui}/MessageListWidget.h
    ${path_DesktopGui}/MessageSourceWidget.h
    ${path_DesktopGui}/MessageView.h
    ${path_DesktopGui}/MsgListView.h
    ${path_DesktopGui}/OnePanelAtTimeWidget.h
    ${path_DesktopGui}/OverlayWidget.h
    ${path_DesktopGui}/PartWidget.h
    ${path_DesktopGui}/PartWidgetFactory.h
    ${path_DesktopGui}/PasswordDialog.h
    ${path_DesktopGui}/ProgressPopUp.h
    ${path_DesktopGui}/ProtocolLoggerWidget.h
    ${path_DesktopGui}/ReplaceCharValidator.h
    ${path_DesktopGui}/SettingsDialog.h
    ${path_DesktopGui}/SimplePartWidget.h
    ${path_DesktopGui}/TagListWidget.h
    ${path_DesktopGui}/TagWidget.h
    ${path_DesktopGui}/TaskProgressIndicator.h
    ${path_DesktopGui}/UserAgentWebPage.h
    ${path_DesktopGui}/Util.h
    ${path_DesktopGui}/Window.h
    ${path_DesktopGui}/ShortcutHandler/ShortcutConfigDialog.h
    ${path_DesktopGui}/ShortcutHandler/ShortcutConfigWidget.h
    ${path_DesktopGui}/ShortcutHandler/ShortcutHandler.h
    )
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
    ${path_DesktopGui}/CreateMailboxDialog.ui
    ${path_DesktopGui}/SettingsOutgoingPage.ui
    ${path_DesktopGui}/SettingsGeneralPage.ui
    ${path_DesktopGui}/ComposeWidget.ui
    ${path_DesktopGui}/ProgressPopUpOld.ui
    ${path_DesktopGui}/ShortcutHandler/ShortcutConfigWidget.ui
    ${path_DesktopGui}/SettingsImapPage.ui
    ${path_DesktopGui}/PasswordDialog.ui
    ${path_DesktopGui}/SettingsCachePage.ui
    ${path_DesktopGui}/EditIdentity.ui
    ${path_DesktopGui}/ProgressPopUp.ui
)
set(libDesktopGui_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/icons.qrc
)

set(libqwwsmtpclient_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/qwwsmtpclient/qwwsmtpclient.h)
set(libqwwsmtpclient_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/qwwsmtpclient/qwwsmtpclient.cpp)

set(libAppVersion_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/AppVersion/SetCoreApplication.h)
set(libAppVersion_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/AppVersion/SetCoreApplication.cpp)

set(path_Imap ${CMAKE_CURRENT_SOURCE_DIR}/src/Imap)
set(libImap_HEADERS
    ${path_Imap}/ConnectionState.h
    ${path_Imap}/Exceptions.h
    ${path_Imap}/Encoders.h
    ${path_Imap}/Parser/3rdparty/kcodecs.h
    ${path_Imap}/Parser/3rdparty/rfccodecs.h

    ${path_Imap}/Parser/Command.h
    ${path_Imap}/Parser/Data.h
    ${path_Imap}/Parser/LowLevelParser.h
    ${path_Imap}/Parser/MailAddress.h
    ${path_Imap}/Parser/Message.h
    ${path_Imap}/Parser/Parser.h
    ${path_Imap}/Parser/Response.h
    ${path_Imap}/Parser/Rfc5322HeaderParser.h
    ${path_Imap}/Parser/Sequence.h
    ${path_Imap}/Parser/ThreadingNode.h

    ${path_Imap}/Network/FileDownloadManager.h
    ${path_Imap}/Network/ForbiddenReply.h
    ${path_Imap}/Network/MsgPartNetAccessManager.h
    ${path_Imap}/Network/MsgPartNetworkReply.h

    ${path_Imap}/Model/Cache.h
    ${path_Imap}/Model/CatenateData.h
    ${path_Imap}/Model/CombinedCache.h
    ${path_Imap}/Model/CopyMoveOperation.h
    ${path_Imap}/Model/DelayedPopulation.h
    ${path_Imap}/Model/DiskPartCache.h
    ${path_Imap}/Model/FindInterestingPart.h
    ${path_Imap}/Model/FlagsOperation.h
    ${path_Imap}/Model/FullMessageCombiner.h
    ${path_Imap}/Model/ItemRoles.h
    ${path_Imap}/Model/MailboxMetadata.h
    ${path_Imap}/Model/MailboxModel.h
    ${path_Imap}/Model/MailboxTree.h
    ${path_Imap}/Model/MemoryCache.h
    ${path_Imap}/Model/Model.h
    ${path_Imap}/Model/ModelTest/modeltest.h
    ${path_Imap}/Model/ModelWatcher.h
    ${path_Imap}/Model/MsgListModel.h
    ${path_Imap}/Model/OneMessageModel.h
    ${path_Imap}/Model/ParserState.h
    ${path_Imap}/Model/PrettyMailboxModel.h
    ${path_Imap}/Model/PrettyMsgListModel.h
    ${path_Imap}/Model/QAIM_reset.h
    ${path_Imap}/Model/SQLCache.h
    ${path_Imap}/Model/SubscribeUnSubscribeOperation.h
    ${path_Imap}/Model/SubtreeModel.h
    ${path_Imap}/Model/TaskFactory.h
    ${path_Imap}/Model/TaskPresentationModel.h
    ${path_Imap}/Model/ThreadingMsgListModel.h
    ${path_Imap}/Model/UidSubmitData.h
    ${path_Imap}/Model/Utils.h
    ${path_Imap}/Model/VisibleTasksModel.h

    ${path_Imap}/Model/kdeui-itemviews/kbihash_p.h
    # this file is left out intentionally; otherwise cmake's automoc won't work with it
    #${path_Imap}/Model/kdeui-itemviews/kdescendantsproxymodel.h

    ${path_Imap}/Tasks/AppendTask.h
    ${path_Imap}/Tasks/CopyMoveMessagesTask.h
    ${path_Imap}/Tasks/CreateMailboxTask.h
    ${path_Imap}/Tasks/DeleteMailboxTask.h
    ${path_Imap}/Tasks/EnableTask.h
    ${path_Imap}/Tasks/ExpungeMailboxTask.h
    ${path_Imap}/Tasks/ExpungeMessagesTask.h
    ${path_Imap}/Tasks/Fake_ListChildMailboxesTask.h
    ${path_Imap}/Tasks/Fake_OpenConnectionTask.h
    ${path_Imap}/Tasks/FetchMsgMetadataTask.h
    ${path_Imap}/Tasks/FetchMsgPartTask.h
    ${path_Imap}/Tasks/GenUrlAuthTask.h
    ${path_Imap}/Tasks/GetAnyConnectionTask.h
    ${path_Imap}/Tasks/IdTask.h
    ${path_Imap}/Tasks/IdleLauncher.h
    ${path_Imap}/Tasks/ImapTask.h
    ${path_Imap}/Tasks/KeepMailboxOpenTask.h
    ${path_Imap}/Tasks/ListChildMailboxesTask.h
    ${path_Imap}/Tasks/NoopTask.h
    ${path_Imap}/Tasks/NumberOfMessagesTask.h
    ${path_Imap}/Tasks/ObtainSynchronizedMailboxTask.h
    ${path_Imap}/Tasks/OfflineConnectionTask.h
    ${path_Imap}/Tasks/OpenConnectionTask.h
    ${path_Imap}/Tasks/SortTask.h
    ${path_Imap}/Tasks/SubscribeUnsubscribeTask.h
    ${path_Imap}/Tasks/ThreadTask.h
    ${path_Imap}/Tasks/UidSubmitTask.h
    ${path_Imap}/Tasks/UnSelectTask.h
    ${path_Imap}/Tasks/UpdateFlagsTask.h
    ${path_Imap}/Tasks/UpdateFlagsOfAllMessagesTask.h
)
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
    ${path_Imap}/Model/DelayedPopulation.cpp
    ${path_Imap}/Model/DiskPartCache.cpp
    ${path_Imap}/Model/FindInterestingPart.cpp
    ${path_Imap}/Model/FlagsOperation.cpp
    ${path_Imap}/Model/FullMessageCombiner.cpp
    ${path_Imap}/Model/MailboxMetadata.cpp
    ${path_Imap}/Model/MailboxModel.cpp
    ${path_Imap}/Model/MailboxTree.cpp
    ${path_Imap}/Model/MemoryCache.cpp
    ${path_Imap}/Model/Model.cpp
    ${path_Imap}/Model/ModelWatcher.cpp
    ${path_Imap}/Model/MsgListModel.cpp
    ${path_Imap}/Model/OneMessageModel.cpp
    ${path_Imap}/Model/ParserState.cpp
    ${path_Imap}/Model/PrettyMailboxModel.cpp
    ${path_Imap}/Model/PrettyMsgListModel.cpp
    ${path_Imap}/Model/SQLCache.cpp
    ${path_Imap}/Model/SubtreeModel.cpp
    ${path_Imap}/Model/TaskFactory.cpp
    ${path_Imap}/Model/TaskPresentationModel.cpp
    ${path_Imap}/Model/ThreadingMsgListModel.cpp
    ${path_Imap}/Model/Utils.cpp
    ${path_Imap}/Model/VisibleTasksModel.cpp

    ${path_Imap}/Model/ModelTest/modeltest.cpp
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

if(WITH_RAGEL AND RAGEL_FOUND)
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

set(trojita_desktop_SOURCES
    ${path_DesktopGui}/main.cpp
)

set(be_contacts_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/be.contacts/main.cpp
)

set(libQmlModelGlue_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/QmlSupport/ModelGlue/ImapAccess.cpp
)
set(libQmlModelGlue_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/QmlSupport/ModelGlue/ImapAccess.h
)

set(libQNAMWebView_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/QmlSupport/QNAMWebView/plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/QmlSupport/QNAMWebView/qdeclarativewebview.cpp
)
set(libQNAMWebView_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/QmlSupport/QNAMWebView/plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/QmlSupport/QNAMWebView/qdeclarativewebview_p.h
)

set(trojitaHarmattan_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Harmattan/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Harmattan/qmlapplicationviewer/qmlapplicationviewer.cpp
)

if(Linguist_FOUND)
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
        if(BUILD_DESKTOP_GUI)
           install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/locale/ DESTINATION share/trojita/locale FILES_MATCHING PATTERN *.qm)
        endif()
    else()
        message(STATUS "No .po files found, will not install any languages")
    endif()
else()
    message(STATUS "Qt Linguist (lupdate/lrelease/lconvert) not found, disabling localization support")
endif()

if(GIT_FOUND AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
    add_custom_target(git_version DEPENDS git_version_fake_file)
    add_custom_command(OUTPUT git_version_fake_file ${CMAKE_CURRENT_BINARY_DIR}/gitversion.hpp
        COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DGIT_EXECUTABLE=${GIT_EXECUTABLE} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitVersion.cmake)
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/gitversion.hpp
        PROPERTIES GENERATED TRUE
        HEADER_FILE_ONLY TRUE)
endif()

add_library(AppVersion ${libAppVersion_SOURCES})

if(GIT_FOUND AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
    set_property(TARGET AppVersion APPEND PROPERTY COMPILE_DEFINITIONS HAS_GITVERSION)
    add_dependencies(AppVersion git_version git_version_fake_file)
endif()

add_library(Common ${libCommon_SOURCES})

add_library(Streams ${libStreams_SOURCES})
if(ZLIB_FOUND)
    target_link_libraries(Streams ${ZLIB_LIBRARIES})
endif()

add_library(qwwsmtpclient ${libqwwsmtpclient_SOURCES})

add_library(MSA ${libMSA_SOURCES})
target_link_libraries(MSA Streams qwwsmtpclient)

add_library(Composer ${libComposer_SOURCES})
target_link_libraries(Composer Common MSA Streams qwwsmtpclient)
if(NOT WITH_QT5)
    target_link_libraries(Composer MimetypesQt4)
endif()

add_library(Imap ${libImap_SOURCES})
target_link_libraries(Imap Common Streams ${QT_QTNETWORK_LIBRARY} ${QT_QTSQL_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})
if(ZLIB_FOUND)
    target_link_libraries(Imap ${ZLIB_LIBRARIES})
endif()

if(NOT WITH_QT5)
    add_library(MimetypesQt4 ${libMimetypesQt4_SOURCES})
    if(SUPPORTS_TARGET_INCLUDES)
        set_property(TARGET MimetypesQt4 APPEND PROPERTY INCLUDE_DIRECTORIES
            ${QT_MKSPECS_DIR}/default ${path_mimetypesqt4}/io/)
    endif()
endif()

if(BUILD_DESKTOP_GUI)
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

    add_library(DesktopGui ${libDesktopGui_SOURCES} ${libDesktopGui_UI_OUT} ${libDesktopGui_RESOURCES_OUT})
    # The following is needed for the LineEdit widget within the .ui files.
    # The ${path_DesktopGui} is needed so that the generated ui_*.h file can find the headers of the custom widgets
    if(SUPPORTS_TARGET_INCLUDES)
        set_property(TARGET DesktopGui APPEND PROPERTY INCLUDE_DIRECTORIES ${path_DesktopGui})
    endif()
    target_link_libraries(DesktopGui Common Composer Imap MSA Streams qwwsmtpclient AbookAddressbook)

    add_executable(trojita ${trojita_desktop_SOURCES} ${trojita_QM})
    target_link_libraries(trojita AppVersion Imap MSA Streams qwwsmtpclient Common Composer AbookAddressbook DesktopGui
        ${QT_QTSQL_LIBRARY} ${QT_QTNETWORK_LIBRARY} ${QT_QTWEBKIT_LIBRARY})
    if(NOT WITH_QT5)
        target_link_libraries(trojita MimetypesQt4 ${QT_QTGUI_LIBRARY} ${QT_QTNETWORK_LIBRARY} ${QT_QTCORE_LIBRARY})
    endif()
    if(ZLIB_FOUND)
        target_link_libraries(trojita ${ZLIB_LIBRARIES})
    endif()

    add_executable(be.contacts ${be_contacts_SOURCES})
    target_link_libraries(be.contacts AbookAddressbook ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})
elseif(BUILD_HARMATTAN)
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
    qt5_use_modules(Common Core)
    qt5_use_modules(Streams Network)
    qt5_use_modules(qwwsmtpclient Network)
    qt5_use_modules(MSA Network)
    qt5_use_modules(Composer Gui Network)
    qt5_use_modules(Imap Gui Network Sql)
    qt5_use_modules(DesktopGui Network WebKitWidgets)
    qt5_use_modules(AbookAddressbook Widgets)
    qt5_use_modules(be.contacts Widgets)
    qt5_use_modules(trojita Widgets)
endif()

if(BUILD_DESKTOP_GUI)
    install(TARGETS trojita be.contacts RUNTIME DESTINATION bin)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui/trojita.desktop DESTINATION share/applications/)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/icons/trojita.png DESTINATION share/icons/hicolor/32x32/apps/)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/icons/trojita.svg DESTINATION share/icons/hicolor/scalable/apps/)
elseif(BUILD_HARMATTAN)
    set(trojita_harmattan_path ${CMAKE_CURRENT_SOURCE_DIR}/src/Harmattan)
    install(TARGETS trojita-tp RUNTIME DESTINATION opt/trojita-tp/bin)
    install(TARGETS trojitaqnamwebviewplugin LIBRARY DESTINATION opt/trojita-tp/bin/net/flaska/QNAMWebView)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/QmlSupport/QNAMWebView/qmldir DESTINATION opt/trojita-tp/bin/net/flaska/QNAMWebView)
    install(DIRECTORY ${trojita_harmattan_path}/qml DESTINATION opt/trojita-tp/)
    install(FILES ${trojita_harmattan_path}/trojita-tp.desktop DESTINATION usr/share/applications/)
    install(FILES ${trojita_harmattan_path}/trojita-tp80.png DESTINATION usr/share/icons/hicolor/80x80/apps/)
endif()


if(NOT WITHOUT_TESTS)
    set(test_LibMailboxSync_SOURCES
        tests/test_LibMailboxSync/ModelEvents.cpp
        tests/test_LibMailboxSync/test_LibMailboxSync.cpp
    )
    add_library(test_LibMailboxSync ${test_LibMailboxSync_SOURCES})
    if(WITH_QT5)
        qt5_use_modules(test_LibMailboxSync Test Network)
    endif()
    if(SUPPORTS_TARGET_INCLUDES)
        set_property(TARGET test_LibMailboxSync APPEND PROPERTY INCLUDE_DIRECTORIES
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_LibMailboxSync)
    endif()
    target_link_libraries(test_LibMailboxSync Imap MSA Streams Common Composer)

    macro(trojita_test fname)
        set(test_${fname}_SOURCES tests/tests/test_${fname}/test_${fname}.cpp)
        set(test_${fname}_HEADERS tests/tests/test_${fname}/test_${fname}.h)
        add_executable(test_${fname} ${test_${fname}_SOURCES})
        target_link_libraries(test_${fname} Imap MSA Streams Common Composer test_LibMailboxSync)
        if(WITH_QT5)
            qt5_use_modules(test_${fname} Network Sql Test Widgets)
        else()
            target_link_libraries(test_${fname} ${QT_QTSQL_LIBRARY} ${QT_QTTEST_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTNETWORK_LIBRARY})
        endif()
        if(ZLIB_FOUND)
            target_link_libraries(test_${fname} ${ZLIB_LIBRARIES})
        endif()
        add_test(test_${fname} test_${fname})
        if(SUPPORTS_TARGET_INCLUDES)
            set_property(TARGET test_${fname} APPEND PROPERTY INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        endif()
    endmacro()

    enable_testing()
    trojita_test(Composer_Submission)
    trojita_test(Composer_responses)
    trojita_test(Html_formatting)
    if(WITH_QT5)
        qt5_use_modules(test_Composer_responses WebKitWidgets)
        qt5_use_modules(test_Html_formatting WebKitWidgets)
    else()
        target_link_libraries(test_Html_formatting ${QT_QTWEBKIT_LIBRARY})
    endif()
    trojita_test(Imap_DisappearingMailboxes)
    trojita_test(Imap_Idle)
    trojita_test(Imap_LowLevelParser)
    trojita_test(Imap_Message)
    trojita_test(Imap_Model)
    trojita_test(Imap_Parser_parse)
    trojita_test(Imap_Responses)
    trojita_test(Imap_SelectedMailboxUpdates)
    trojita_test(Imap_Tasks_CreateMailbox)
    trojita_test(Imap_Tasks_DeleteMailbox)
    trojita_test(Imap_Tasks_ListChildMailboxes)
    trojita_test(Imap_Tasks_ObtainSynchronizedMailbox)
    trojita_test(Imap_Tasks_OpenConnection)
    trojita_test(Imap_Threading)
    trojita_test(Rfc5322)
    trojita_test(RingBuffer)
    trojita_test(SenderIdentitiesModel)
    trojita_test(SqlCache)
    trojita_test(algorithms)
    trojita_test(rfccodecs)
endif()

# FIXME: fix build warnings
