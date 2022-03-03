# Copyright (C) 2014-2015 Stephan Platz <trojita@paalsteek.de>
#
# This file is part of the Trojita Qt IMAP e-mail client,
# http://trojita.flaska.net/
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License or (at your option) version 3 or any later version
# accepted by the membership of KDE e.V. (or its successor approved
# by the membership of KDE e.V.), which shall act as a proxy
# defined in Section 14 of version 3 of the license.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# - Try to find the mimetic library
# Once done this will define
#  Mimetic_FOUND - System has the mimetic library
#  Mimetic_INCLUDE_DIRS - The MIMETIC include directories
#  Mimetic_LIBRARIES - The libraries for use with target_link_libraries()
#  Mimetic_DEFINITIONS - Compiler switches required for using MIMETIC
#
# If Mimetic_Found is TRUE, it will also define the following imported
# target:
# Mimetic::Mimetic

find_package(PkgConfig)
pkg_check_modules(PC_MIMETIC QUIET mimetic)
set(MIMETIC_DEFINITIONS ${PC_MIMETIC_CFLAGS_OTHER})

find_path(Mimetic_INCLUDE_DIRS mimetic.h
          HINTS ${PC_MIMETIC_INCLUDEDIR} ${PC_MIMETIC_INCLUDE_DIRS}
          PATH_SUFFIXES mimetic)

find_library(Mimetic_LIBRARIES NAMES mimetic libmimetic
             HINTS ${PC_MIMETIC_LIBDIR} ${PC_MIMETIC_LIBRARY_DIRS} )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Mimetic
    FOUND_VAR
        Mimetic_FOUND
    REQUIRED_VARS
        Mimetic_LIBRARIES
        Mimetic_INCLUDE_DIRS
)

if (Mimetic_FOUND AND NOT TARGET Mimetic::Mimetic)
    add_library(Mimetic::Mimetic UNKNOWN IMPORTED)
    set_target_properties(Mimetic::Mimetic PROPERTIES
        IMPORTED_LOCATION "${Mimetic_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${Mimetic_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(Mimetic_INCLUDE_DIRS Mimetic_LIBRARIES)

include(FeatureSummary)
set_package_properties(Mimetic PROPERTIES
    URL "https://www.codesink.org/mimetic_mime_library.html"
    DESCRIPTION "A full featured, STL-based, standards compliant C++ MIME library"
)
