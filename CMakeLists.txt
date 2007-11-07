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

find_package(Boost 1.34 REQUIRED)

find_package(Qt4 REQUIRED)
ADD_DEFINITIONS(${QT_DEFINITIONS})
INCLUDE_DIRECTORIES(${QT_INCLUDES})

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

set(libImap_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Command.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Response.cpp
    )

set(trojita_SRCS main.cpp ${libImap_SRCS})

qt4_automoc(${trojita_SRCS})

add_executable(trojita ${trojita_SRCS})

target_link_libraries(trojita ${QT_QTCORE_LIBRARY} ${QT_QTNETWORK_LIBRARY})
