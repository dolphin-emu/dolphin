#.rst:
# FindFreetype
# ------------
#
# Locate FreeType library
#
# This module defines
#
# ::
#
#   FREETYPE_LIBRARIES, the library to link against
#   FREETYPE_FOUND, if false, do not try to link to FREETYPE
#   FREETYPE_INCLUDE_DIRS, where to find headers.
#   FREETYPE_VERSION_STRING, the version of freetype found (since CMake 2.8.8)
#   This is the concatenation of the paths:
#   FREETYPE_INCLUDE_DIR_ft2build
#   FREETYPE_INCLUDE_DIR_freetype2
#
#
#
# $FREETYPE_DIR is an environment variable that would correspond to the
# ./configure --prefix=$FREETYPE_DIR used in building FREETYPE.

#=============================================================================
# Copyright 2007-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

# Created by Eric Wing.
# Modifications by Alexander Neundorf.
# This file has been renamed to "FindFreetype.cmake" instead of the correct
# "FindFreeType.cmake" in order to be compatible with the one from KDE4, Alex.

# Ugh, FreeType seems to use some #include trickery which
# makes this harder than it should be. It looks like they
# put ft2build.h in a common/easier-to-find location which
# then contains a #include to a more specific header in a
# more specific location (#include <freetype/config/ftheader.h>).
# Then from there, they need to set a bunch of #define's
# so you can do something like:
# #include FT_FREETYPE_H
# Unfortunately, using CMake's mechanisms like include_directories()
# wants explicit full paths and this trickery doesn't work too well.
# I'm going to attempt to cut out the middleman and hope
# everything still works.
find_path(
  FREETYPE_INCLUDE_DIR_ft2build
  ft2build.h
  HINTS
    ENV FREETYPE_DIR
  PATHS
    /usr/X11R6
    /usr/local/X11R6
    /usr/local/X11
    /usr/freeware
    ENV GTKMM_BASEPATH
    [HKEY_CURRENT_USER\\SOFTWARE\\gtkmm\\2.4;Path]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\gtkmm\\2.4;Path]
  PATH_SUFFIXES
    include/freetype2
    include
    freetype2
)

find_path(
  FREETYPE_INCLUDE_DIR_freetype2
  NAMES
    freetype/config/ftheader.h
    config/ftheader.h
  HINTS
    ENV FREETYPE_DIR
  PATHS
    /usr/X11R6
    /usr/local/X11R6
    /usr/local/X11
    /usr/freeware
    ENV GTKMM_BASEPATH
    [HKEY_CURRENT_USER\\SOFTWARE\\gtkmm\\2.4;Path]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\gtkmm\\2.4;Path]
  PATH_SUFFIXES
    include/freetype2
    include
    freetype2
)

find_library(FREETYPE_LIBRARY
  NAMES
    freetype
    libfreetype
    freetype219
  HINTS
    ENV FREETYPE_DIR
  PATHS
    /usr/X11R6
    /usr/local/X11R6
    /usr/local/X11
    /usr/freeware
    ENV GTKMM_BASEPATH
    [HKEY_CURRENT_USER\\SOFTWARE\\gtkmm\\2.4;Path]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\gtkmm\\2.4;Path]
  PATH_SUFFIXES
    lib
)

# set the user variables
if(FREETYPE_INCLUDE_DIR_ft2build AND FREETYPE_INCLUDE_DIR_freetype2)
  set(FREETYPE_INCLUDE_DIRS "${FREETYPE_INCLUDE_DIR_ft2build};${FREETYPE_INCLUDE_DIR_freetype2}")
  list(REMOVE_DUPLICATES FREETYPE_INCLUDE_DIRS)
endif()
set(FREETYPE_LIBRARIES "${FREETYPE_LIBRARY}")

if(EXISTS "${FREETYPE_INCLUDE_DIR_freetype2}/freetype/freetype.h")
  set(FREETYPE_H "${FREETYPE_INCLUDE_DIR_freetype2}/freetype/freetype.h")
elseif(EXISTS "${FREETYPE_INCLUDE_DIR_freetype2}/freetype.h")
  set(FREETYPE_H "${FREETYPE_INCLUDE_DIR_freetype2}/freetype.h")
endif()

if(FREETYPE_INCLUDE_DIR_freetype2 AND FREETYPE_H)
  file(STRINGS "${FREETYPE_H}" freetype_version_str
       REGEX "^#[\t ]*define[\t ]+FREETYPE_(MAJOR|MINOR|PATCH)[\t ]+[0-9]+$")

  unset(FREETYPE_VERSION_STRING)
  foreach(VPART MAJOR MINOR PATCH)
    foreach(VLINE ${freetype_version_str})
      if(VLINE MATCHES "^#[\t ]*define[\t ]+FREETYPE_${VPART}[\t ]+([0-9]+)$")
        set(FREETYPE_VERSION_PART "${CMAKE_MATCH_1}")
        if(FREETYPE_VERSION_STRING)
          set(FREETYPE_VERSION_STRING "${FREETYPE_VERSION_STRING}.${FREETYPE_VERSION_PART}")
        else()
          set(FREETYPE_VERSION_STRING "${FREETYPE_VERSION_PART}")
        endif()
        unset(FREETYPE_VERSION_PART)
      endif()
    endforeach()
  endforeach()
endif()


# set FREETYPE_FOUND to TRUE if all listed variables are TRUE
if(FREETYPE_LIBRARY AND FREETYPE_INCLUDE_DIRS AND FREETYPE_VERSION_STRING)
  set(FREETYPE_FOUND TRUE)
else()
  set(FREETYPE_FOUND FALSE)
endif()

mark_as_advanced(
  FREETYPE_LIBRARY
  FREETYPE_INCLUDE_DIR_freetype2
  FREETYPE_INCLUDE_DIR_ft2build
)
