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

# make sure we can use std::tr1::memory
include( CheckCXXSourceCompiles )
CHECK_CXX_SOURCE_COMPILES(
    "#include <tr1/memory>
    int main() { std::tr1::shared_ptr<int> i;
    return 0;}"
    TR1_SHARED_PTR_FOUND)
if(NOT TR1_SHARED_PTR_FOUND)
    find_package(Boost 1.34 REQUIRED)
    # FIXME: FindBoost.cmake doesn't check version, so we should check it here...
    FILE(MAKE_DIRECTORY tr1)
    FILE(WRITE "tr1/memory"
"
#ifndef TROJITA_INCLUDED_TR1_MEMORY
#define TROJITA_INCLUDED_TR1_MEMORY 1
// This file was created because CMake didn't find working
// std::tr1::shared_ptr<>. Its purpose is to use classes from Boost for such
// purposes.
#include <boost/shared_ptr.hpp>
namespace std
{
    namespace tr1
    {
        using boost::shared_ptr;
    }
}
#endif // TROJITA_INCLUDED_TR1_MEMORY
")
    SET(CMAKE_REQUIRED_LIBRARIES Boost)
endif(NOT TR1_SHARED_PTR_FOUND)

find_package(Qt4 REQUIRED)
ADD_DEFINITIONS(${QT_DEFINITIONS})
INCLUDE_DIRECTORIES(${QT_INCLUDES})

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory( tests )

set(libImap_SRCS Imap/Parser.cpp Imap/Command.cpp Imap/Response.cpp
    Imap/LowLevelParser.cpp Imap/Exceptions.cpp
    Imap/rfccodecs.cpp Imap/kcodecs.cpp)

set(libDemo_SRCS Demo/ParserMonitor.cpp)

set(trojita_SRCS main.cpp)

qt4_automoc(${trojita_SRCS} ${libImap_SRCS} ${libDemo_SRCS})

add_library(Imap ${libImap_SRCS})
add_library(Demo ${libDemo_SRCS})
add_executable(trojita ${trojita_SRCS})

target_link_libraries(trojita Imap Demo ${QT_QTCORE_LIBRARY} ${QT_QTNETWORK_LIBRARY})
