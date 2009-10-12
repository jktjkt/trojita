#  Copyright (C) 2007 Jan Kundrát <jkt@gentoo.org>
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public
#  License as published by the Free Software Foundation; either
#  version 2 of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; see the file COPYING.  If not, write to
#  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
#  Boston, MA 02110-1301, USA.
#

project(trojita)
enable_testing()

cmake_minimum_required(VERSION 2.6)

SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-common -ggdb")
#SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weffc++")
#SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -fdiagnostics-show-option" )

find_package(Qt4 4.5 REQUIRED)
ADD_DEFINITIONS(${QT_DEFINITIONS})
INCLUDE_DIRECTORIES(${QT_INCLUDES})

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory( tests )

set(libImap_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Parser/Parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Parser/Command.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Parser/Response.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Parser/LowLevelParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Parser/Data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Parser/Message.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Parser/3rdparty/rfccodecs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Parser/3rdparty/kcodecs.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Encoders.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Parser/3rdparty/qmailcodec.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Exceptions.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/MailboxMetadata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/UnauthenticatedHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/AuthenticatedHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/SelectedHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/SelectingHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/MailboxModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/PrettyMailboxModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/MsgListModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/MailboxTree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/MemoryCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/Utils.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/ModelWatcher.cpp
)

set(libMessageView_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/MessageView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/EmbeddedWebView.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/PartWidgetFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/SimplePartWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/Rfc822HeaderView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/AttachmentView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/LoadablePartWidget.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Network/MsgPartNetworkReply.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Network/ForbiddenReply.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Network/MsgPartNetAccessManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Network/AuxiliaryReply.cpp
)

set(libModelTest_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/ModelTest/modeltest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/ModelTest/ModelTestMoc.cpp
    )

set(libStreams_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/Streams/SocketFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Streams/Socket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Streams/IODeviceSocket.cpp
)

set(libMSA_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/MSA/AbstractMSA.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MSA/Sendmail.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MSA/SMTP.cpp
)

set(trojita_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/Window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/SettingsDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/SettingsNames.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/ComposeWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/MsgListWidget.cpp
    )

set(Gui_UI_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/CreateMailboxDialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/Gui/ComposeWidget.ui
)

SET(trojita_RCC_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/icons.qrc
)

set(libQwwSmtpClient_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/qwwsmtpclient/qwwsmtpclient.cpp
)

set(libQtIconLoader_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/iconloader/qticonloader.cpp
)


qt4_automoc(${trojita_SRCS} ${libImap_SRCS} ${libStreams_SRCS}
    ${libMessageView_SRCS} ${libModelTest_SRCS} ${libMSA_SRCS}
    ${libQwwSmtpClient_SRCS} ${libQtIconLoader_SRCS})

qt4_add_resources(trojita_RCCS ${trojita_RCC_SRCS})
qt4_wrap_ui(Gui_UIS ${Gui_UI_SRCS})

add_library(Imap ${libImap_SRCS})
add_library(MessageView ${libMessageView_SRCS})
add_library(Streams ${libStreams_SRCS})
add_library(MSA ${libMSA_SRCS})
add_library(ModelTest ${libModelTest_SRCS})
add_library(QwwSmtpClient ${libQwwSmtpClient_SRCS})
add_library(QtIconLoader ${libQtIconLoader_SRCS})
add_executable(trojita ${trojita_SRCS} ${trojita_RCCS} ${Gui_UIS})

target_link_libraries(Streams ${QT_QTNETWORK_LIBRARY})
target_link_libraries(Imap Streams ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})
target_link_libraries(MessageView ${QT_QTWEBKIT_LIBRARY} ${QT_QTNETWORK_LIBRARY})
target_link_libraries(QwwSmtpClient ${QT_QTNETWORK_LIBRARY})
target_link_libraries(trojita Imap MessageView ModelTest MSA QwwSmtpClient QtIconLoader)

install(TARGETS trojita
    RUNTIME DESTINATION bin)
