# - Find libusb-1.0 library
# This module defines
#  LIBUSB_INCLUDE_DIR, where to find bluetooth.h
#  LIBUSB_LIBRARIES, the libraries needed to use libusb-1.0.
#  LIBUSB_FOUND, If false, do not try to use libusb-1.0.
#
# Copyright (c) 2009, Michal Cihar, <michal@cihar.com>
#
# vim: expandtab sw=4 ts=4 sts=4:

if(ANDROID)
       set(LIBUSB_FOUND FALSE CACHE INTERNAL "libusb-1.0 found")
       message(STATUS "libusb-1.0 not found.")
elseif (NOT LIBUSB_FOUND)
    pkg_check_modules (LIBUSB_PKG libusb-1.0)

    find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h
       PATHS
       ${LIBUSB_PKG_INCLUDE_DIRS}
       /usr/include/libusb-1.0
       /usr/include
       /usr/local/include/libusb-1.0
       /usr/local/include
    )

    find_library(LIBUSB_LIBRARIES NAMES usb-1.0 usb
       PATHS
       ${LIBUSB_PKG_LIBRARY_DIRS}
       /usr/lib
       /usr/local/lib
    )

    if(LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARIES)
       set(LIBUSB_FOUND TRUE CACHE INTERNAL "libusb-1.0 found")
       message(STATUS "Found libusb-1.0: ${LIBUSB_INCLUDE_DIR}, ${LIBUSB_LIBRARIES}")
    else(LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARIES)
       set(LIBUSB_FOUND FALSE CACHE INTERNAL "libusb-1.0 found")
       message(STATUS "libusb-1.0 not found.")
    endif(LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARIES)

    mark_as_advanced(LIBUSB_INCLUDE_DIR LIBUSB_LIBRARIES)
endif ()

