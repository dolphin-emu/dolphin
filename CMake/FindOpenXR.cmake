# Copyright 2019 Collabora, Ltd.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
# Original Author:
# 2019 Ryan Pavlik <ryan.pavlik@collabora.com>

#.rst:
# FindOpenXR
# ----------
#
# Find various parts of OpenXR 1.0.
#
# COMPONENTS
# ^^^^^^^^^^
#
# This module respects several COMPONENTS: ``headers``, ``loader``, ``registry``, ``specscripts``, and
# ``sdkscripts``. If no components are specified, ``headers`` and ``loader`` are assumed.
#
# Cache variables
# ^^^^^^^^^^^^^^^
#
# The following cache variables may also be set to assist/control the operation of this module:
#
# Related to the ``OpenXR-SDK`` repo <https://github.com/KhronosGroup/OpenXR-SDK> (this is the repo with pre-built headers),
# the ``OpenXR-SDK-Source`` repo <https://github.com/KhronosGroup/OpenXR-SDK>, or the internal Khronos GitLab ``openxr`` repo:
#
# ``OPENXR_SDK_SRC_DIR``
#  Path to the root of the ``openxr``, ``OpenXR-SDK``, or ``OpenXR-SDK-Source`` repo source.
# ``OPENXR_SDK_BUILD_DIR``
#  Path to the root of the build directory corresponding to the ``openxr``, ``OpenXR-SDK``, or ``OpenXR-SDK-Source`` repo.
#

# Normalize paths
foreach(PATHVAR OPENXR_SDK_DIR OPENXR_SDK_BUILD_DIR)
    if(${${PATHVAR}})
        file(TO_CMAKE_PATH ${${PATHVAR}} ${PATHVAR})
    endif()
endforeach()

# Set up cache variables
set(OPENXR_SDK_SRC_DIR "${OPENXR_SDK_SRC_DIR}"
    CACHE PATH "The root of your OpenXR-SDK, OpenXR-SDK-Source, or GitLab openxr source directory - see https://github.com/KhronosGroup/OpenXR-SDK")
set(OPENXR_SDK_BUILD_DIR "${OPENXR_SDK_BUILD_DIR}"
    CACHE PATH "The root of your OpenXR-SDK, OpenXR-SDK-Source, or GitLab openxr build directory.")

# Currently only explicitly supporting 1.0
set(OPENXR_MAJOR_VER 1)
set(OPENXR_MINOR_VER 0)
set(OPENXR_OUT_DIR ${OPENXR_MAJOR_VER}.${OPENXR_MINOR_VER})
if(WIN32)
    set(OPENXR_STATIC ON)
else()
    set(OPENXR_STATIC OFF)
endif()

###
# Assemble lists of places to look
###
set(_oxr_include_search_dirs)
set(_oxr_loader_search_dirs)
set(_oxr_specscripts_search_dirs)
set(_oxr_sdkscripts_search_dirs)
set(_oxr_registry_search_dirs)

# These are macros to avoid a new variable scope.

# Macro to extend search locations given a source dir.
macro(_oxr_handle_potential_root_src_dir _dir)
    list(APPEND _oxr_include_search_dirs
        "${_dir}/include"
        "${_dir}/specification/out/${OPENXR_OUT_DIR}")
    list(APPEND _oxr_registry_search_dirs
        "${_dir}/specification/registry/")
    list(APPEND _oxr_specscripts_search_dirs
        "${_dir}/specification/scripts/")
    list(APPEND _oxr_sdkscripts_search_dirs
        "${_dir}/src/scripts/")
endmacro()

# Macro to extend search locations given a build dir.
macro(_oxr_handle_potential_root_build_dir _dir)
    list(APPEND _oxr_include_search_dirs
        "${_dir}/include")
    list(APPEND _oxr_loader_search_dirs
        "${_dir}/src/loader")
endmacro()

set(_oxr_build_tag)
if(OPENXR_STATIC)
    set(_oxr_build_type static)
else()
    set(_oxr_build_type dynamic)
endif()
if(MSVC_VERSION GREATER 1919)
    # Can use 2019
    set(_oxr_build_tag msvs2019_${_oxr_build_type})
endif()

set(_oxr_lib_path_suffixes)
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        list(APPEND _oxr_lib_path_suffixes lib)
        if(_oxr_build_tag)
            list(APPEND _oxr_lib_path_suffixes ${_oxr_build_tag}/lib)
        endif()
    else()
        list(APPEND _oxr_lib_path_suffixes lib32)
        if(_oxr_build_tag)
            list(APPEND _oxr_lib_path_suffixes ${_oxr_build_tag}/lib32)
        endif()
        list(APPEND _oxr_lib_path_suffixes lib)
    endif()
endif()

# User-supplied directories
if(OPENXR_SDK_SRC_DIR)
    _oxr_handle_potential_root_src_dir(${OPENXR_SDK_SRC_DIR})
endif()
if(OPENXR_ROOT_BUILD_DIR)
    _oxr_handle_potential_root_build_dir(${OPENXR_ROOT_BUILD_DIR})
endif()

# Guessed build dir based on src dir
if(OPENXR_SDK_SRC_DIR)
    _oxr_handle_potential_root_build_dir("${OPENXR_SDK_SRC_DIR}/build")
endif()

# Guesses of sibling directories by name - last resort.
foreach(_dir "${CMAKE_SOURCE_DIR}/../OpenXR-SDK" "${CMAKE_SOURCE_DIR}/../OpenXR-SDK-Source" "${CMAKE_SOURCE_DIR}/../openxr")
    _oxr_handle_potential_root_src_dir(${_dir})
    _oxr_handle_potential_root_build_dir(${_dir}/build)
endforeach()

###
# Search for includes
###

# This must also contain openxr/openxr_platform.h
find_path(OPENXR_OPENXR_INCLUDE_DIR
    NAMES
    openxr/openxr.h
    PATHS
    ${_oxr_include_search_dirs}
)

find_path(OPENXR_PLATFORM_DEFINES_INCLUDE_DIR
    NAMES
    openxr/openxr_platform_defines.h
    PATHS
    ${_oxr_include_search_dirs}
)

###
# Search for other parts
###
find_library(OPENXR_loader_LIBRARY
    NAMES
    libopenxr_loader
    openxr_loader
    openxr_loader-${OPENXR_MAJOR_VER}_${OPENXR_MINOR_VER}
    PATHS
    ${_oxr_loader_search_dirs}
    PATH_SUFFIXES
    ${_oxr_lib_path_suffixes})

find_path(OPENXR_SPECSCRIPTS_DIR
    NAMES
    reg.py
    PATHS
    ${_oxr_specscripts_search_dirs})

find_path(OPENXR_SDKSCRIPTS_DIR
    NAMES
    automatic_source_generator.py
    PATHS
    ${_oxr_sdkscripts_search_dirs})

find_file(OPENXR_REGISTRY
    NAMES xr.xml
    PATHS
    ${_oxr_registry_search_dirs})

###
# Fix up list of requested components
###
if(NOT OpenXR_FIND_COMPONENTS)
    # Default to headers and loader
    set(OpenXR_FIND_COMPONENTS headers loader)
endif()

if("${OpenXR_FIND_COMPONENTS}" MATCHES "scripts" AND NOT "${OpenXR_FIND_COMPONENTS}" MATCHES "registry")
    # scripts depend on registry (mostly).
    list(APPEND OpenXR_FIND_COMPONENTS registry)
endif()

if("${OpenXR_FIND_COMPONENTS}" MATCHES "loader" AND NOT "${OpenXR_FIND_COMPONENTS}" MATCHES "headers")
    # loader depends on headers.
    list(APPEND OpenXR_FIND_COMPONENTS headers)
endif()

if("${OpenXR_FIND_COMPONENTS}" MATCHES "sdkscripts" AND NOT "${OpenXR_FIND_COMPONENTS}" MATCHES "specscripts")
    # source scripts depend on spec scripts.
    list(APPEND OpenXR_FIND_COMPONENTS specscripts)
endif()

###
# Determine if the various requested components are found.
###
set(_oxr_component_required_vars)
foreach(_comp IN LISTS OpenXR_FIND_COMPONENTS)

    if(${_comp} STREQUAL "headers")
        list(APPEND _oxr_component_required_vars
            OPENXR_OPENXR_INCLUDE_DIR
            OPENXR_PLATFORM_DEFINES_INCLUDE_DIR)
        if(EXISTS "${OPENXR_OPENXR_INCLUDE_DIR}/openxr/openxr.h"
                AND EXISTS "${OPENXR_OPENXR_INCLUDE_DIR}/openxr/openxr_platform.h"
                AND EXISTS "${OPENXR_PLATFORM_DEFINES_INCLUDE_DIR}/openxr/openxr_platform_defines.h"
                AND EXISTS "${OPENXR_PLATFORM_DEFINES_INCLUDE_DIR}/openxr/openxr_reflection.h")
            set(OpenXR_headers_FOUND TRUE)
            mark_as_advanced(OPENXR_OPENXR_INCLUDE_DIR OPENXR_PLATFORM_DEFINES_INCLUDE_DIR)
        else()
            set(OpenXR_headers_FOUND FALSE)
        endif()

    elseif(${_comp} STREQUAL "loader")
        list(APPEND _oxr_component_required_vars OPENXR_loader_LIBRARY)
        if(EXISTS "${OPENXR_loader_LIBRARY}")
            set(OpenXR_loader_FOUND TRUE)
            mark_as_advanced(OPENXR_loader_LIBRARY)
        else()
            set(OpenXR_loader_FOUND FALSE)
        endif()

    elseif(${_comp} STREQUAL "registry")
        list(APPEND _oxr_component_required_vars OPENXR_REGISTRY)
        if(EXISTS "${OPENXR_REGISTRY}")
            set(OpenXR_registry_FOUND TRUE)
            mark_as_advanced(OPENXR_REGISTRY)
        else()
            set(OpenXR_registry_FOUND FALSE)
        endif()

    elseif(${_comp} STREQUAL "specscripts")
        list(APPEND _oxr_component_required_vars OPENXR_SPECSCRIPTS_DIR)
        if(EXISTS "${OPENXR_SPECSCRIPTS_DIR}/generator.py"
                AND EXISTS "${OPENXR_SPECSCRIPTS_DIR}/reg.py"
                AND EXISTS "${OPENXR_SPECSCRIPTS_DIR}/genxr.py")
            set(OpenXR_specscripts_FOUND TRUE)
            mark_as_advanced(OPENXR_SPECSCRIPTS_DIR)
        else()
            set(OpenXR_specscripts_FOUND FALSE)
        endif()

    elseif(${_comp} STREQUAL "sdkscripts")
        list(APPEND _oxr_component_required_vars OPENXR_SDKSCRIPTS_DIR)
        if(EXISTS "${OPENXR_SDKSCRIPTS_DIR}/automatic_source_generator.py"
                AND EXISTS "${OPENXR_SDKSCRIPTS_DIR}/src_genxr.py")
            set(OpenXR_sdkscripts_FOUND TRUE)
            mark_as_advanced(OPENXR_SDKSCRIPTS_DIR)
        else()
            set(OpenXR_sdkscripts_FOUND FALSE)
        endif()

    else()
        message(WARNING "${_comp} is not a recognized OpenXR component")
        set(OpenXR_${_comp}_FOUND FALSE)
    endif()
endforeach()
unset(_comp)

###
# FPHSA call
###
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenXR
    REQUIRED_VARS
    ${_oxr_component_required_vars}
    HANDLE_COMPONENTS
    FAIL_MESSAGE
    "Could NOT find the requested OpenXR components, try setting OPENXR_SDK_SRC_DIR and/or OPENXR_SDK_BUILD_DIR"
)
###
# If found, set variables and create targets
###

# Component: headers
if(OpenXR_headers_FOUND)
    set(OPENXR_INCLUDE_DIRS
        ${OPENXR_OPENXR_INCLUDE_DIR}
        ${OPENXR_PLATFORM_DEFINES_INCLUDE_DIR})
        list(REMOVE_DUPLICATES OPENXR_INCLUDE_DIRS)

    # This target just provides the headers with prototypes.
    # You may have linker errors if you try using this
    # without linking to the loader
    if(NOT TARGET OpenXR::Headers)
        add_library(OpenXR::Headers INTERFACE IMPORTED)
    endif()
    set_target_properties(OpenXR::Headers PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_oxr_include_dirs}")

    # This target just provides the headers, without any prototypes.
    # Finding and loading the loader at runtime is your problem.
    if(NOT TARGET OpenXR::HeadersNoPrototypes)
        add_library(OpenXR::HeadersNoPrototypes INTERFACE IMPORTED)
    endif()
    set_target_properties(OpenXR::HeadersNoPrototypes PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "XR_NO_PROTOTYPES"
        INTERFACE_LINK_LIBRARIES "OpenXR::Headers")

endif()

# Component: loader
if(OpenXR_loader_FOUND AND OpenXR_headers_FOUND)
    set(_oxr_loader_interface_libs OpenXR::Headers)
    # include dl library for statically-linked loader
    get_filename_component(_oxr_loader_ext ${OPENXR_loader_LIBRARY} EXT)
    if(_oxr_loader_ext STREQUAL CMAKE_STATIC_LIBRARY_SUFFIX)
        set(_oxr_loader_lib_type STATIC)
        list(APPEND _oxr_loader_interface_libs ${CMAKE_DL_LIBS})
    else()
        set(_oxr_loader_lib_type SHARED)
    endif()

    if(NOT TARGET OpenXR::Loader)
        add_library(OpenXR::Loader UNKNOWN IMPORTED)
    endif()
    set_target_properties(OpenXR::Loader PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${OPENXR_loader_LIBRARY}"
        INTERFACE_LINK_LIBRARIES "${_oxr_loader_interface_libs}")
endif()
