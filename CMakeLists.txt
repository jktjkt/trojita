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

SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-common")
#SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weffc++")

find_package(Boost 1.34 REQUIRED)

find_package(Qt4 REQUIRED)
ADD_DEFINITIONS(${QT_DEFINITIONS})
INCLUDE_DIRECTORIES(${QT_INCLUDES})

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory( tests )

set(libImap_SRCS Imap/Parser.cpp Imap/Command.cpp Imap/Response.cpp
    Imap/rfccodecs.cpp Imap/kcodecs.cpp)

set(trojita_SRCS main.cpp)

qt4_automoc(${trojita_SRCS} ${libImap_SRCS})

add_library(Imap ${libImap_SRCS})
add_executable(trojita ${trojita_SRCS})

target_link_libraries(trojita Imap ${QT_QTCORE_LIBRARY} ${QT_QTNETWORK_LIBRARY})
enable_testing()
