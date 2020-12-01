# - Find PulseAudio library
# This module defines
#  PulseAudio_INCLUDE_DIR
#  PulseAudio_LIBRARIES
#  PulseAudio_FOUND
#
# vim: expandtab sw=4 ts=4 sts=4:

find_package(PkgConfig QUIET)
pkg_check_modules (PulseAudio_PKG QUIET libpulse)

find_path(PulseAudio_INCLUDE_DIR NAMES pulse/pulseaudio.h
  PATHS
  ${PulseAudio_PKG_INCLUDE_DIRS}
  /usr/include/pulse
  /usr/include
  /usr/local/include/pulse
  /usr/local/include
)

find_library(PulseAudio_LIBRARIES NAMES pulse
  PATHS
  ${PulseAudio_PKG_LIBRARY_DIRS}
  /usr/lib
  /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PulseAudio
                                  REQUIRED_VARS PulseAudio_LIBRARIES PulseAudio_INCLUDE_DIR)

if(PulseAudio_FOUND)
  if(NOT TARGET PulseAudio::PulseAudio)
    add_library(PulseAudio::PulseAudio UNKNOWN IMPORTED)
    set_target_properties(PulseAudio::PulseAudio PROPERTIES
      IMPORTED_LOCATION ${PulseAudio_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${PulseAudio_INCLUDE_DIR}
    )
  endif()
endif()

mark_as_advanced(PulseAudio_INCLUDE_DIR PulseAudio_LIBRARIES)
