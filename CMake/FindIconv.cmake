# Based on CMake's FindIconv.cmake
# Modified to prefer non-built-in iconv over the built-in one
# See https://gitlab.kitware.com/cmake/cmake/-/issues/24695 for details
# This file can be deleted once that issue has been closed and the fix has
# made it into a satisfactory number of cmake versions.  FreeBSD is the only
# system known to hit this so far, so "satisfactory" can probably be defined
# as "enough that most FreeBSD users have a fixed cmake".

find_path(Iconv_INCLUDE_DIR
  NAMES "iconv.h"
  DOC "iconv include directory")
mark_as_advanced(Iconv_INCLUDE_DIR)

find_library(Iconv_LIBRARY
  NAMES iconv libiconv
  NAMES_PER_DIR
  DOC "iconv library (if not in the C library)")
mark_as_advanced(Iconv_LIBRARY)

# iconv can only be provided in libc on a POSIX system.
if(UNIX AND (NOT Iconv_INCLUDE_DIR OR NOT Iconv_LIBRARY))
  include(CMakePushCheckState)
  include(CheckCXXSourceCompiles)
  cmake_push_check_state(RESET)
  # We always suppress the message here: Otherwise on supported systems
  # not having iconv in their C library (e.g. those using libiconv)
  # would always display a confusing "Looking for iconv - not found" message
  set(CMAKE_FIND_QUIETLY TRUE)
  # The following code will not work, but it's sufficient to see if it compiles.
  # Note: libiconv will define the iconv functions as macros, so CheckSymbolExists
  # will not yield correct results.
  set(Iconv_IMPLICIT_TEST_CODE
    "
    #include <stddef.h>
    #include <iconv.h>
    int main() {
      char *a, *b;
      size_t i, j;
      iconv_t ic;
      ic = iconv_open(\"to\", \"from\");
      iconv(ic, &a, &i, &b, &j);
      iconv_close(ic);
    }
    "
  )
  check_cxx_source_compiles("${Iconv_IMPLICIT_TEST_CODE}" Iconv_IS_BUILT_IN)
  cmake_pop_check_state()
  if(Iconv_IS_BUILT_IN)
    unset(Iconv_INCLUDE_DIR)
    unset(Iconv_LIBRARY)
  endif()
else()
  set(Iconv_IS_BUILT_IN FALSE)
endif()

set(_Iconv_REQUIRED_VARS)
if(Iconv_IS_BUILT_IN)
  set(_Iconv_REQUIRED_VARS _Iconv_IS_BUILT_IN_MSG)
  set(_Iconv_IS_BUILT_IN_MSG "built in to C library")
else()
  set(_Iconv_REQUIRED_VARS Iconv_LIBRARY Iconv_INCLUDE_DIR)
endif()

# NOTE: glibc's iconv.h does not define _LIBICONV_VERSION
if(Iconv_INCLUDE_DIR AND EXISTS "${Iconv_INCLUDE_DIR}/iconv.h")
  file(STRINGS ${Iconv_INCLUDE_DIR}/iconv.h Iconv_VERSION_DEFINE REGEX "_LIBICONV_VERSION (.*)")

  if(Iconv_VERSION_DEFINE MATCHES "(0x[A-Fa-f0-9]+)")
    set(Iconv_VERSION_NUMBER "${CMAKE_MATCH_1}")
    # encoding -> version number: (major<<8) + minor
    math(EXPR Iconv_VERSION_MAJOR "${Iconv_VERSION_NUMBER} >> 8" OUTPUT_FORMAT HEXADECIMAL)
    math(EXPR Iconv_VERSION_MINOR "${Iconv_VERSION_NUMBER} - (${Iconv_VERSION_MAJOR} << 8)" OUTPUT_FORMAT HEXADECIMAL)

    math(EXPR Iconv_VERSION_MAJOR "${Iconv_VERSION_MAJOR}" OUTPUT_FORMAT DECIMAL)
    math(EXPR Iconv_VERSION_MINOR "${Iconv_VERSION_MINOR}" OUTPUT_FORMAT DECIMAL)
    set(Iconv_VERSION "${Iconv_VERSION_MAJOR}.${Iconv_VERSION_MINOR}")
  endif()

  unset(Iconv_VERSION_DEFINE)
  unset(Iconv_VERSION_NUMBER)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Iconv
  REQUIRED_VARS ${_Iconv_REQUIRED_VARS}
  VERSION_VAR Iconv_VERSION)

if(Iconv_FOUND)
  if(Iconv_IS_BUILT_IN)
    set(Iconv_INCLUDE_DIRS "")
    set(Iconv_LIBRARIES "")
  else()
    set(Iconv_INCLUDE_DIRS "${Iconv_INCLUDE_DIR}")
    set(Iconv_LIBRARIES "${Iconv_LIBRARY}")
  endif()
  if(NOT TARGET Iconv::Iconv)
    add_library(Iconv::Iconv INTERFACE IMPORTED)
    set_property(TARGET Iconv::Iconv PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${Iconv_INCLUDE_DIRS}")
    set_property(TARGET Iconv::Iconv PROPERTY INTERFACE_LINK_LIBRARIES "${Iconv_LIBRARIES}")
  endif()
endif()
