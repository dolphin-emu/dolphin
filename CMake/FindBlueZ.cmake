# - Find BlueZ library
# This module defines
#  BlueZ_INCLUDE_DIR
#  BlueZ_LIBRARIES
#  BlueZ_FOUND
#
# vim: expandtab sw=4 ts=4 sts=4:

include(FindPkgConfig)
pkg_check_modules (BlueZ_PKG QUIET bluez)

find_path(BlueZ_INCLUDE_DIR NAMES bluetooth/bluetooth.h
  PATHS
  ${BlueZ_PKG_INCLUDE_DIRS}
  /usr/include/bluetooth
  /usr/include
  /usr/local/include/bluetooth
  /usr/local/include
)

find_library(BlueZ_LIBRARIES NAMES bluetooth
  PATHS
  ${BlueZ_PKG_LIBRARY_DIRS}
  /usr/lib
  /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BlueZ
                                  REQUIRED_VARS BlueZ_LIBRARIES BlueZ_INCLUDE_DIR)

if(BlueZ_FOUND)
  if(NOT TARGET BlueZ::BlueZ)
    add_library(BlueZ::BlueZ UNKNOWN IMPORTED)
    set_target_properties(BlueZ::BlueZ PROPERTIES
      IMPORTED_LOCATION ${BlueZ_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${BlueZ_INCLUDE_DIR}
    )
  endif()
endif()

mark_as_advanced(BlueZ_INCLUDE_DIR BlueZ_LIBRARIES)
