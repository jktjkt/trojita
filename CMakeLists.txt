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

SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-common")
#SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weffc++")

include( cmake/StdTr1Memory.cmake )

find_package(Qt4 REQUIRED)
ADD_DEFINITIONS(${QT_DEFINITIONS})
INCLUDE_DIRECTORIES(${QT_INCLUDES})

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory( tests )

set(libImap_SRCS Imap/Parser.cpp Imap/Command.cpp Imap/Response.cpp
    Imap/LowLevelParser.cpp Imap/Exceptions.cpp Imap/Data.cpp Imap/Message.cpp
    Imap/Cache.cpp Imap/MailboxModel.cpp Imap/SocketFactory.cpp
    Imap/Authenticator.cpp Imap/rfccodecs.cpp Imap/kcodecs.cpp)

set(trojita_SRCS main.cpp Demo/ParserMonitor.cpp)

set(ImapMailboxModelDemo_SRCS Demo/ImapMailboxModelDemo.cpp)

qt4_automoc(${trojita_SRCS} ${libImap_SRCS}
    ${ImapMailboxModelDemo_SRCS})

add_library(Imap ${libImap_SRCS})
add_executable(trojita ${trojita_SRCS})
add_executable(ImapMailboxModelDemo ${ImapMailboxModelDemo_SRCS})

target_link_libraries(trojita Imap ${QT_QTCORE_LIBRARY} ${QT_QTNETWORK_LIBRARY})
target_link_libraries(ImapMailboxModelDemo Imap ${QT_QTCORE_LIBRARY}
    ${QT_QTNETWORK_LIBRARY} ${QT_QTGUI_LIBRARY} )
