# Locate the SFML library
#
# This module defines the following variables:
# - For each module XXX (SYSTEM, WINDOW, GRAPHICS, NETWORK, AUDIO, MAIN):
#   - SFML_XXX_LIBRARY_DEBUG, the name of the debug library of the xxx module (set to SFML_XXX_LIBRARY_RELEASE is no debug version is found)
#   - SFML_XXX_LIBRARY_RELEASE, the name of the release library of the xxx module (set to SFML_XXX_LIBRARY_DEBUG is no release version is found)
#   - SFML_XXX_LIBRARY, the name of the library to link to for the xxx module (includes both debug and optimized names if necessary)
#   - SFML_XXX_FOUND, true if either the debug or release library of the xxx module is found
# - SFML_LIBRARIES, the list of all libraries corresponding to the required modules
# - SFML_FOUND, true if all the required modules are found
# - SFML_INCLUDE_DIR, the path where SFML headers are located (the directory containing the SFML/Config.hpp file)
#
# By default, the dynamic libraries of SFML will be found. To find the static ones instead,
# you must set the SFML_STATIC_LIBRARIES variable to TRUE before calling find_package(SFML ...).
#
# If SFML is not installed in a standard path, you can use the SFMLDIR CMake variable or environment variable
# to tell CMake where SFML is.

# deduce the libraries suffix from the options
set(FIND_SFML_LIB_SUFFIX "")
if(SFML_STATIC_LIBRARIES)
    set(FIND_SFML_LIB_SUFFIX "${FIND_SFML_LIB_SUFFIX}-s")
endif()

# find the SFML include directory
find_path(SFML_INCLUDE_DIR SFML/Config.hpp
          PATH_SUFFIXES include
          PATHS
          ~/Library/Frameworks
          /Library/Frameworks
          /usr/local/
          /usr/
          /sw          # Fink
          /opt/local/  # DarwinPorts
          /opt/csw/    # Blastwave
          /opt/
          ${SFMLDIR}
          $ENV{SFMLDIR})


# will be set to false if one of the required modules is not found
set(SFML_FOUND TRUE)
set(SFML_VERSION_OK TRUE)

# check the version number
if(SFML_FIND_VERSION AND SFML_INCLUDE_DIR AND NOT (SFML_INCLUDE_DIR STREQUAL "SFML_INCLUDE_DIR-NOTFOUND"))
    # extract the major and minor version numbers from SFML/Config.hpp
    FILE(READ "${SFML_INCLUDE_DIR}/SFML/Config.hpp" SFML_CONFIG_HPP_CONTENTS)
    STRING(REGEX MATCH ".*#define SFML_VERSION_MAJOR ([0-9]+).*#define SFML_VERSION_MINOR ([0-9]+).*" SFML_CONFIG_HPP_CONTENTS "${SFML_CONFIG_HPP_CONTENTS}")
    STRING(REGEX REPLACE ".*#define SFML_VERSION_MAJOR ([0-9]+).*" "\\1" SFML_VERSION_MAJOR "${SFML_CONFIG_HPP_CONTENTS}")
    STRING(REGEX REPLACE ".*#define SFML_VERSION_MINOR ([0-9]+).*" "\\1" SFML_VERSION_MINOR "${SFML_CONFIG_HPP_CONTENTS}")
    math(EXPR SFML_REQUESTED_VERSION "${SFML_FIND_VERSION_MAJOR} * 10 + ${SFML_FIND_VERSION_MINOR}")

    # if we could extract them, compare with the requested version number
    if (SFML_VERSION_MAJOR)
        # transform version numbers to an integer
        math(EXPR SFML_VERSION "${SFML_VERSION_MAJOR} * 10 + ${SFML_VERSION_MINOR}")

        # compare them
        if(SFML_VERSION LESS SFML_REQUESTED_VERSION)
            set(SFML_VERSION_OK FALSE)
        endif()
    else()
        # SFML version is < 2.0
        if (SFML_REQUESTED_VERSION GREATER 19)
            set(SFML_VERSION_OK FALSE)
            set(SFML_VERSION_MAJOR 1)
            set(SFML_VERSION_MINOR x)
        endif()
    endif()
elseif(SFML_INCLUDE_DIR STREQUAL "SFML_INCLUDE_DIR-NOTFOUND")
    set(SFML_FOUND FALSE)
    set(FIND_SFML_MISSING "${FIND_SFML_MISSING} SFML_INCLUDE_DIR")
endif()

# find the requested modules
set(FIND_SFML_LIB_PATHS ~/Library/Frameworks
                        /Library/Frameworks
                        /usr/local
                        /usr
                        /sw
                        /opt/local
                        /opt/csw
                        /opt
                        ${SFMLDIR}
                        $ENV{SFMLDIR})
foreach(FIND_SFML_COMPONENT ${SFML_FIND_COMPONENTS})
    string(TOLOWER ${FIND_SFML_COMPONENT} FIND_SFML_COMPONENT_LOWER)
    string(TOUPPER ${FIND_SFML_COMPONENT} FIND_SFML_COMPONENT_UPPER)
    set(FIND_SFML_COMPONENT_NAME sfml-${FIND_SFML_COMPONENT_LOWER}${FIND_SFML_LIB_SUFFIX})

    # no suffix for sfml-main, it is always a static library
    if(FIND_SFML_COMPONENT_LOWER STREQUAL "main")
        set(FIND_SFML_COMPONENT_NAME sfml-${FIND_SFML_COMPONENT_LOWER})
    endif()

    # debug library
    find_library(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG
                 NAMES ${FIND_SFML_COMPONENT_NAME}-d
                 PATH_SUFFIXES lib64 lib
                 PATHS ${FIND_SFML_LIB_PATHS})

    # release library
    find_library(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_RELEASE
                 NAMES ${FIND_SFML_COMPONENT_NAME}
                 PATH_SUFFIXES lib64 lib
                 PATHS ${FIND_SFML_LIB_PATHS})

    if (SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG OR SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_RELEASE)
        # library found
        set(SFML_${FIND_SFML_COMPONENT_UPPER}_FOUND TRUE)
        
        # if both are found, set SFML_XXX_LIBRARY to contain both
        if (SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG AND SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_RELEASE)
            set(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY debug     ${SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG}
                                                          optimized ${SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_RELEASE})
        endif()

        # if only one debug/release variant is found, set the other to be equal to the found one
        if (SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG AND NOT SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_RELEASE)
            # debug and not release
            set(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_RELEASE ${SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG})
            set(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY         ${SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG})
        endif()
        if (SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_RELEASE AND NOT SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG)
            # release and not debug
            set(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG ${SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_RELEASE})
            set(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY       ${SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_RELEASE})
        endif()
    else()
        # library not found
        set(SFML_FOUND FALSE)
        set(SFML_${FIND_SFML_COMPONENT_UPPER}_FOUND FALSE)
        set(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY "")
        set(FIND_SFML_MISSING "${FIND_SFML_MISSING} SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY")
    endif()

    # mark as advanced
    MARK_AS_ADVANCED(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY
                     SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_RELEASE
                     SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG)

    # add to the global list of libraries
    set(SFML_LIBRARIES ${SFML_LIBRARIES} "${SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY}")
endforeach()

# handle errors
if(NOT SFML_VERSION_OK)
    # SFML version not ok
    set(FIND_SFML_ERROR "SFML found but version too low (requested: ${SFML_FIND_VERSION}, found: ${SFML_VERSION_MAJOR}.${SFML_VERSION_MINOR})")
    set(SFML_FOUND FALSE)
elseif(NOT SFML_FOUND)
    # include directory or library not found
    set(FIND_SFML_ERROR "Could NOT find SFML (missing: ${FIND_SFML_MISSING})")
endif()
if (NOT SFML_FOUND)
    if(SFML_FIND_REQUIRED)
        # fatal error
        message(FATAL_ERROR ${FIND_SFML_ERROR})
    elseif(NOT SFML_FIND_QUIETLY)
        # error but continue
        message("${FIND_SFML_ERROR}")
    endif()
endif()

# handle success
if(SFML_FOUND)
    message("Found SFML: ${SFML_INCLUDE_DIR}")
endif()
