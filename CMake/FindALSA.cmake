# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindALSA
# --------
#
# Find alsa
#
# Find the alsa libraries (asound)
#
# ::
#
#   This module defines the following variables:
#      ALSA_FOUND       - True if ALSA_INCLUDE_DIR & ALSA_LIBRARY are found
#      ALSA_LIBRARIES   - Set when ALSA_LIBRARY is found
#      ALSA_INCLUDE_DIRS - Set when ALSA_INCLUDE_DIR is found
#
#
#
# ::
#
#      ALSA_INCLUDE_DIR - where to find asoundlib.h, etc.
#      ALSA_LIBRARY     - the asound library
#      ALSA_VERSION_STRING - the version of alsa found (since CMake 2.8.8)

find_path(ALSA_INCLUDE_DIR NAMES alsa/asoundlib.h
          DOC "The ALSA (asound) include directory"
)

find_library(ALSA_LIBRARY NAMES asound
          DOC "The ALSA (asound) library"
)

if(ALSA_INCLUDE_DIR AND EXISTS "${ALSA_INCLUDE_DIR}/alsa/version.h")
  file(STRINGS "${ALSA_INCLUDE_DIR}/alsa/version.h" alsa_version_str REGEX "^#define[\t ]+SND_LIB_VERSION_STR[\t ]+\".*\"")

  string(REGEX REPLACE "^.*SND_LIB_VERSION_STR[\t ]+\"([^\"]*)\".*$" "\\1" ALSA_VERSION_STRING "${alsa_version_str}")
  unset(alsa_version_str)
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ALSA
                                  REQUIRED_VARS ALSA_LIBRARY ALSA_INCLUDE_DIR
                                  VERSION_VAR ALSA_VERSION_STRING)

if(ALSA_FOUND)
  set( ALSA_LIBRARIES ${ALSA_LIBRARY} )
  set( ALSA_INCLUDE_DIRS ${ALSA_INCLUDE_DIR} )
  if(NOT TARGET ALSA::ALSA)
    add_library(ALSA::ALSA UNKNOWN IMPORTED)
    set_target_properties(ALSA::ALSA PROPERTIES
      IMPORTED_LOCATION ${ALSA_LIBRARY}
      INTERFACE_INCLUDE_DIRECTORIES ${ALSA_INCLUDE_DIR}
    )
  endif()
endif()

mark_as_advanced(ALSA_INCLUDE_DIR ALSA_LIBRARY)
