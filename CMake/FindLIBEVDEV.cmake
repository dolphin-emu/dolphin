# - Try to find libevdev
# Once done this will define
#  LIBEVDEV_FOUND - System has libevdev
#  LIBEVDEV_INCLUDE_DIRS - The libevdev include directories
#  LIBEVDEV_LIBRARIES - The libraries needed to use libevdev

find_package(PkgConfig)
pkg_check_modules(PC_LIBEVDEV QUIET libevdev)

FIND_PATH(
    LIBEVDEV_INCLUDE_DIR libevdev/libevdev.h
    HINTS ${PC_LIBEVDEV_INCLUDEDIR} ${PC_LIBEVDEV_INCLUDE_DIRS}
    /usr/include
    /usr/local/include
    ${LIBEVDEV_PATH_INCLUDES}
)

FIND_LIBRARY(
    LIBEVDEV_LIBRARY
    NAMES evdev libevdev
    HINTS ${PC_LIBEVDEV_LIBDIR} ${PC_LIBEVDEV_LIBRARY_DIRS}
    PATHS ${ADDITIONAL_LIBRARY_PATHS}
        ${LIBEVDEV_PATH_LIB}
)

set(LIBEVDEV_LIBRARIES ${LIBEVDEV_LIBRARY} )
set(LIBEVDEV_INCLUDE_DIRS ${LIBEVDEV_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBEVDEV  DEFAULT_MSG
                                  LIBEVDEV_LIBRARY LIBEVDEV_INCLUDE_DIR)

mark_as_advanced(LIBEVDEV_INCLUDE_DIR LIBEVDEV_LIBRARY )
