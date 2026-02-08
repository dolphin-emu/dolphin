# - Try to find LIBUDEV
# Once done this will define
#  LIBUDEV_FOUND - System has LIBUDEV
#  LIBUDEV_INCLUDE_DIRS - The LIBUDEV include directories
#  LIBUDEV_LIBRARIES - The libraries needed to use LIBUDEV

FIND_PATH(
    LIBUDEV_INCLUDE_DIR libudev.h
    /usr/include
    /usr/local/include
    ${LIBUDEV_PATH_INCLUDES}
)

FIND_LIBRARY(
    LIBUDEV_LIBRARY
    NAMES udev libudev
    PATHS ${ADDITIONAL_LIBRARY_PATHS}
        ${LIBUDEV_PATH_LIB}
)

set(LIBUDEV_LIBRARIES ${LIBUDEV_LIBRARY} )
set(LIBUDEV_INCLUDE_DIRS ${LIBUDEV_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBUDEV  DEFAULT_MSG
                                  LIBUDEV_LIBRARY LIBUDEV_INCLUDE_DIR)

mark_as_advanced(LIBUDEV_INCLUDE_DIR LIBUDEV_LIBRARY )
