#.rst:
# FindX11_XCB
# -----------
#
# Sourced from: https://github.com/KDE/extra-cmake-modules/blob/master/find-modules/FindX11_XCB.cmake
#
# Try to find the X11 XCB compatibility library.
#
# This will define the following variables:
#
# ``X11_XCB_FOUND``
#     True if (the requested version of) libX11-xcb is available
# ``X11_XCB_VERSION``
#     The version of libX11-xcb (this is not guaranteed to be set even when
#     X11_XCB_FOUND is true)
# ``X11_XCB_LIBRARIES``
#     This can be passed to target_link_libraries() instead of the ``EGL::EGL``
#     target
# ``X11_XCB_INCLUDE_DIR``
#     This should be passed to target_include_directories() if the target is not
#     used for linking
# ``X11_XCB_DEFINITIONS``
#     This should be passed to target_compile_options() if the target is not
#     used for linking
#
# If ``X11_XCB_FOUND`` is TRUE, it will also define the following imported
# target:
#
# ``X11::XCB``
#     The X11 XCB compatibility library
#
# In general we recommend using the imported target, as it is easier to use.
# Bear in mind, however, that if the target is in the link interface of an
# exported library, it must be made available by the package config file.
#
# Since pre-1.0.0.

#=============================================================================
# Copyright 2014 Alex Merry <alex.merry@kde.org>
# Copyright 2011 Fredrik Höglund <fredrik@kde.org>
# Copyright 2008 Helio Chissini de Castro <helio@kde.org>
# Copyright 2007 Matthias Kretz <kretz@kde.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

#include(${CMAKE_CURRENT_LIST_DIR}/ECMFindModuleHelpersStub.cmake)

#ecm_find_package_version_check(X11_XCB)

# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
find_package(PkgConfig)
pkg_check_modules(PKG_X11_XCB QUIET x11-xcb)

set(X11_XCB_DEFINITIONS ${PKG_X11_XCB_CFLAGS_OTHER})
set(X11_XCB_VERSION ${PKG_X11_XCB_VERSION})

find_path(X11_XCB_INCLUDE_DIR
    NAMES X11/Xlib-xcb.h
    HINTS ${PKG_X11_XCB_INCLUDE_DIRS}
)
find_library(X11_XCB_LIBRARY
    NAMES X11-xcb
    HINTS ${PKG_X11_XCB_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(X11_XCB
    FOUND_VAR
        X11_XCB_FOUND
    REQUIRED_VARS
        X11_XCB_LIBRARY
        X11_XCB_INCLUDE_DIR
    VERSION_VAR
        X11_XCB_VERSION
)

if(X11_XCB_FOUND AND NOT TARGET X11::XCB)
    add_library(X11::XCB UNKNOWN IMPORTED)
    set_target_properties(X11::XCB PROPERTIES
        IMPORTED_LOCATION "${X11_XCB_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${X11_XCB_DEFINITIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${X11_XCB_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(X11_XCB_INCLUDE_DIR X11_XCB_LIBRARY)

# compatibility variables
set(X11_XCB_LIBRARIES ${X11_XCB_LIBRARY})
set(X11_XCB_INCLUDE_DIRS ${X11_XCB_INCLUDE_DIR})
set(X11_XCB_VERSION_STRING ${X11_XCB_VERSION})

include(FeatureSummary)
set_package_properties(X11_XCB PROPERTIES
    URL "http://xorg.freedesktop.org/"
    DESCRIPTION "A compatibility library for code that translates Xlib API calls into XCB calls"
)
