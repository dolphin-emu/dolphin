# - Find AO library
# This module defines
#  AO_INCLUDE_DIR
#  AO_LIBRARIES
#  AO_FOUND
#
# vim: expandtab sw=4 ts=4 sts=4:

include(FindPkgConfig)
pkg_check_modules (AO_PKG QUIET ao)

find_path(AO_INCLUDE_DIR NAMES ao/ao.h
  PATHS
  ${AO_PKG_INCLUDE_DIRS}
  /usr/include/ao
  /usr/include
  /usr/local/include/ao
  /usr/local/include
)

find_library(AO_LIBRARIES NAMES ao
  PATHS
  ${AO_PKG_LIBRARY_DIRS}
  /usr/lib
  /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AO
                                  REQUIRED_VARS AO_LIBRARIES AO_INCLUDE_DIR)

if(AO_FOUND)
  if(NOT TARGET AO::AO)
    add_library(AO::AO UNKNOWN IMPORTED)
    set_target_properties(AO::AO PROPERTIES
      IMPORTED_LOCATION ${AO_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${AO_INCLUDE_DIR}
    )
  endif()
endif()

mark_as_advanced(AO_INCLUDE_DIR AO_LIBRARIES)
