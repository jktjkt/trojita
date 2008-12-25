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

SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-common -ggdb")
#SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weffc++")

include( cmake/StdTr1Memory.cmake )

find_package(Qt4 4.4 REQUIRED)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Parser/rfccodecs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Parser/kcodecs.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Exceptions.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Cache.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/MailboxModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/MsgListModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/MailboxTree.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Streams/SocketFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Streams/Socket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Streams/IODeviceSocket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Streams/UnixSocket.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Authenticator.cpp
    )

set(libModelTest_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/ModelTest/modeltest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Model/ModelTest/ModelTestMoc.cpp
    )

set(trojita_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow/Window.cpp
    )

qt4_automoc(${trojita_SRCS} ${libImap_SRCS}
    ${libModelTest_SRCS})

add_library(Imap ${libImap_SRCS})
add_library(ModelTest ${libModelTest_SRCS})
add_executable(trojita ${trojita_SRCS})

target_link_libraries(Imap ModelTest ${QT_QTGUI_LIBRARY})
target_link_libraries(trojita Imap)
