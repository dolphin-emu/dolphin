# This script locates the SFML library
# ------------------------------------
#
# Usage
# -----
#
# When you try to locate the SFML libraries, you must specify which modules you want to use (system, window, graphics, network, audio, main).
# If none is given, the SFML_LIBRARIES variable will be empty and you'll end up linking to nothing.
# example:
#   find_package(SFML COMPONENTS graphics window system) # find the graphics, window and system modules
#
# You can enforce a specific version, either MAJOR.MINOR or only MAJOR.
# If nothing is specified, the version won't be checked (i.e. any version will be accepted).
# example:
#   find_package(SFML COMPONENTS ...)     # no specific version required
#   find_package(SFML 2 COMPONENTS ...)   # any 2.x version
#   find_package(SFML 2.4 COMPONENTS ...) # version 2.4 or greater
#
# By default, the dynamic libraries of SFML will be found. To find the static ones instead,
# you must set the SFML_STATIC_LIBRARIES variable to TRUE before calling find_package(SFML ...).
# Since you have to link yourself all the SFML dependencies when you link it statically, the following
# additional variables are defined: SFML_XXX_DEPENDENCIES and SFML_DEPENDENCIES (see their detailed
# description below).
# In case of static linking, the SFML_STATIC macro will also be defined by this script.
# example:
#   set(SFML_STATIC_LIBRARIES TRUE)
#   find_package(SFML 2 COMPONENTS network system)
#
# On Mac OS X if SFML_STATIC_LIBRARIES is not set to TRUE then by default CMake will search for frameworks unless
# CMAKE_FIND_FRAMEWORK is set to "NEVER" for example. Please refer to CMake documentation for more details.
# Moreover, keep in mind that SFML frameworks are only available as release libraries unlike dylibs which
# are available for both release and debug modes.
#
# If SFML is not installed in a standard path, you can use the SFML_ROOT CMake (or environment) variable
# to tell CMake where SFML is.
#
# Output
# ------
#
# This script defines the following variables:
# - For each specified module XXX (system, window, graphics, network, audio, main):
#   - SFML_XXX_LIBRARY_DEBUG:   the name of the debug library of the xxx module (set to SFML_XXX_LIBRARY_RELEASE is no debug version is found)
#   - SFML_XXX_LIBRARY_RELEASE: the name of the release library of the xxx module (set to SFML_XXX_LIBRARY_DEBUG is no release version is found)
#   - SFML_XXX_LIBRARY:         the name of the library to link to for the xxx module (includes both debug and optimized names if necessary)
#   - SFML_XXX_FOUND:           true if either the debug or release library of the xxx module is found
#   - SFML_XXX_DEPENDENCIES:    the list of libraries the module depends on, in case of static linking
# - SFML_LIBRARIES:    the list of all libraries corresponding to the required modules
# - SFML_FOUND:        true if all the required modules are found
# - SFML_INCLUDE_DIR:  the path where SFML headers are located (the directory containing the SFML/Config.hpp file)
# - SFML_DEPENDENCIES: the list of libraries SFML depends on, in case of static linking
#
# example:
#   find_package(SFML 2 COMPONENTS system window graphics audio REQUIRED)
#   include_directories(${SFML_INCLUDE_DIR})
#   add_executable(myapp ...)
#   target_link_libraries(myapp ${SFML_LIBRARIES})

# define the SFML_STATIC macro if static build was chosen
if(SFML_STATIC_LIBRARIES)
    add_definitions(-DSFML_STATIC)
endif()

# define the list of search paths for headers and libraries
set(FIND_SFML_PATHS
    ${SFML_ROOT}
    $ENV{SFML_ROOT}
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local
    /usr
    /sw
    /opt/local
    /opt/csw
    /opt)

# find the SFML include directory
find_path(SFML_INCLUDE_DIR SFML/Config.hpp
          PATH_SUFFIXES include
          PATHS ${FIND_SFML_PATHS})

# check the version number
set(SFML_VERSION_OK TRUE)
if(SFML_FIND_VERSION AND SFML_INCLUDE_DIR)
    # extract the major and minor version numbers from SFML/Config.hpp
    # we have to handle framework a little bit differently:
    if("${SFML_INCLUDE_DIR}" MATCHES "SFML.framework")
        set(SFML_CONFIG_HPP_INPUT "${SFML_INCLUDE_DIR}/Headers/Config.hpp")
    else()
        set(SFML_CONFIG_HPP_INPUT "${SFML_INCLUDE_DIR}/SFML/Config.hpp")
    endif()
    FILE(READ "${SFML_CONFIG_HPP_INPUT}" SFML_CONFIG_HPP_CONTENTS)
    STRING(REGEX REPLACE ".*#define SFML_VERSION_MAJOR ([0-9]+).*" "\\1" SFML_VERSION_MAJOR "${SFML_CONFIG_HPP_CONTENTS}")
    STRING(REGEX REPLACE ".*#define SFML_VERSION_MINOR ([0-9]+).*" "\\1" SFML_VERSION_MINOR "${SFML_CONFIG_HPP_CONTENTS}")
    STRING(REGEX REPLACE ".*#define SFML_VERSION_PATCH ([0-9]+).*" "\\1" SFML_VERSION_PATCH "${SFML_CONFIG_HPP_CONTENTS}")
    if (NOT "${SFML_VERSION_PATCH}" MATCHES "^[0-9]+$")
        set(SFML_VERSION_PATCH 0)
    endif()
    math(EXPR SFML_REQUESTED_VERSION "${SFML_FIND_VERSION_MAJOR} * 10000 + ${SFML_FIND_VERSION_MINOR} * 100 + ${SFML_FIND_VERSION_PATCH}")

    # if we could extract them, compare with the requested version number
    if (SFML_VERSION_MAJOR)
        # transform version numbers to an integer
        math(EXPR SFML_VERSION "${SFML_VERSION_MAJOR} * 10000 + ${SFML_VERSION_MINOR} * 100 + ${SFML_VERSION_PATCH}")

        # compare them
        if(SFML_VERSION LESS SFML_REQUESTED_VERSION)
            set(SFML_VERSION_OK FALSE)
        endif()
    else()
        # SFML version is < 2.0
        if (SFML_REQUESTED_VERSION GREATER 10900)
            set(SFML_VERSION_OK FALSE)
            set(SFML_VERSION_MAJOR 1)
            set(SFML_VERSION_MINOR x)
            set(SFML_VERSION_PATCH x)
        endif()
    endif()
endif()

# find the requested modules
set(SFML_FOUND TRUE) # will be set to false if one of the required modules is not found
foreach(FIND_SFML_COMPONENT ${SFML_FIND_COMPONENTS})
    string(TOLOWER ${FIND_SFML_COMPONENT} FIND_SFML_COMPONENT_LOWER)
    string(TOUPPER ${FIND_SFML_COMPONENT} FIND_SFML_COMPONENT_UPPER)
    set(FIND_SFML_COMPONENT_NAME sfml-${FIND_SFML_COMPONENT_LOWER})

    # no suffix for sfml-main, it is always a static library
    if(FIND_SFML_COMPONENT_LOWER STREQUAL "main")
        # release library
        find_library(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_RELEASE
                     NAMES ${FIND_SFML_COMPONENT_NAME}
                     PATH_SUFFIXES lib64 lib
                     PATHS ${FIND_SFML_PATHS})

        # debug library
        find_library(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG
                     NAMES ${FIND_SFML_COMPONENT_NAME}-d
                     PATH_SUFFIXES lib64 lib
                     PATHS ${FIND_SFML_PATHS})
    else()
        # static release library
        find_library(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_STATIC_RELEASE
                     NAMES ${FIND_SFML_COMPONENT_NAME}-s
                     PATH_SUFFIXES lib64 lib
                     PATHS ${FIND_SFML_PATHS})

        # static debug library
        find_library(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_STATIC_DEBUG
                     NAMES ${FIND_SFML_COMPONENT_NAME}-s-d
                     PATH_SUFFIXES lib64 lib
                     PATHS ${FIND_SFML_PATHS})

        # dynamic release library
        find_library(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DYNAMIC_RELEASE
                     NAMES ${FIND_SFML_COMPONENT_NAME}
                     PATH_SUFFIXES lib64 lib
                     PATHS ${FIND_SFML_PATHS})

        # dynamic debug library
        find_library(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DYNAMIC_DEBUG
                     NAMES ${FIND_SFML_COMPONENT_NAME}-d
                     PATH_SUFFIXES lib64 lib
                     PATHS ${FIND_SFML_PATHS})

        # choose the entries that fit the requested link type
        if(SFML_STATIC_LIBRARIES)
            if(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_STATIC_RELEASE)
                set(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_RELEASE ${SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_STATIC_RELEASE})
            endif()
            if(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_STATIC_DEBUG)
                set(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG ${SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_STATIC_DEBUG})
            endif()
        else()
            if(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DYNAMIC_RELEASE)
                set(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_RELEASE ${SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DYNAMIC_RELEASE})
            endif()
            if(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DYNAMIC_DEBUG)
                set(SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG ${SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DYNAMIC_DEBUG})
            endif()
        endif()
    endif()

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
                     SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DEBUG
                     SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_STATIC_RELEASE
                     SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_STATIC_DEBUG
                     SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DYNAMIC_RELEASE
                     SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY_DYNAMIC_DEBUG)

    # add to the global list of libraries
    set(SFML_LIBRARIES ${SFML_LIBRARIES} "${SFML_${FIND_SFML_COMPONENT_UPPER}_LIBRARY}")
endforeach()

# in case of static linking, we must also define the list of all the dependencies of SFML libraries
if(SFML_STATIC_LIBRARIES)

    # detect the OS
    if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        set(FIND_SFML_OS_WINDOWS 1)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set(FIND_SFML_OS_LINUX 1)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
        set(FIND_SFML_OS_FREEBSD 1)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(FIND_SFML_OS_MACOSX 1)
    endif()

    # start with an empty list
    set(SFML_DEPENDENCIES)
    set(FIND_SFML_DEPENDENCIES_NOTFOUND)

    # macro that searches for a 3rd-party library
    macro(find_sfml_dependency output friendlyname)
        # No lookup in environment variables (PATH on Windows), as they may contain wrong library versions
        find_library(${output} NAMES ${ARGN} PATHS ${FIND_SFML_PATHS} PATH_SUFFIXES lib NO_SYSTEM_ENVIRONMENT_PATH)
        if(${${output}} STREQUAL "${output}-NOTFOUND")
            unset(output)
            set(FIND_SFML_DEPENDENCIES_NOTFOUND "${FIND_SFML_DEPENDENCIES_NOTFOUND} ${friendlyname}")
        endif()
    endmacro()

    # sfml-system
    list(FIND SFML_FIND_COMPONENTS "system" FIND_SFML_SYSTEM_COMPONENT)
    if(NOT ${FIND_SFML_SYSTEM_COMPONENT} EQUAL -1)

        # update the list -- these are only system libraries, no need to find them
        if(FIND_SFML_OS_LINUX OR FIND_SFML_OS_FREEBSD OR FIND_SFML_OS_MACOSX)
            set(SFML_SYSTEM_DEPENDENCIES "pthread")
        endif()
        if(FIND_SFML_OS_LINUX)
            set(SFML_SYSTEM_DEPENDENCIES ${SFML_SYSTEM_DEPENDENCIES} "rt")
        endif()
        if(FIND_SFML_OS_WINDOWS)
            set(SFML_SYSTEM_DEPENDENCIES "winmm")
        endif()
        set(SFML_DEPENDENCIES ${SFML_SYSTEM_DEPENDENCIES} ${SFML_DEPENDENCIES})
    endif()

    # sfml-network
    list(FIND SFML_FIND_COMPONENTS "network" FIND_SFML_NETWORK_COMPONENT)
    if(NOT ${FIND_SFML_NETWORK_COMPONENT} EQUAL -1)

        # update the list -- these are only system libraries, no need to find them
        if(FIND_SFML_OS_WINDOWS)
            set(SFML_NETWORK_DEPENDENCIES "ws2_32")
        endif()
        set(SFML_DEPENDENCIES ${SFML_NETWORK_DEPENDENCIES} ${SFML_DEPENDENCIES})
    endif()

    # sfml-window
    list(FIND SFML_FIND_COMPONENTS "window" FIND_SFML_WINDOW_COMPONENT)
    if(NOT ${FIND_SFML_WINDOW_COMPONENT} EQUAL -1)

        # find libraries
        if(FIND_SFML_OS_LINUX OR FIND_SFML_OS_FREEBSD)
            find_sfml_dependency(X11_LIBRARY "X11" X11)
            find_sfml_dependency(LIBXCB_LIBRARIES "XCB" xcb libxcb)
            find_sfml_dependency(X11_XCB_LIBRARY "X11-xcb" X11-xcb libX11-xcb)
            find_sfml_dependency(XCB_RANDR_LIBRARY "xcb-randr" xcb-randr libxcb-randr)
            find_sfml_dependency(XCB_IMAGE_LIBRARY "xcb-image" xcb-image libxcb-image)
        endif()

        if(FIND_SFML_OS_LINUX)
            find_sfml_dependency(UDEV_LIBRARIES "UDev" udev libudev)
        endif()

        # update the list
        if(FIND_SFML_OS_WINDOWS)
            set(SFML_WINDOW_DEPENDENCIES ${SFML_WINDOW_DEPENDENCIES} "opengl32" "winmm" "gdi32")
        elseif(FIND_SFML_OS_LINUX)
            set(SFML_WINDOW_DEPENDENCIES ${SFML_WINDOW_DEPENDENCIES} "GL" ${X11_LIBRARY} ${LIBXCB_LIBRARIES} ${X11_XCB_LIBRARY} ${XCB_RANDR_LIBRARY} ${XCB_IMAGE_LIBRARY} ${UDEV_LIBRARIES})
        elseif(FIND_SFML_OS_FREEBSD)
            set(SFML_WINDOW_DEPENDENCIES ${SFML_WINDOW_DEPENDENCIES} "GL" ${X11_LIBRARY} ${LIBXCB_LIBRARIES} ${X11_XCB_LIBRARY} ${XCB_RANDR_LIBRARY} ${XCB_IMAGE_LIBRARY} "usbhid")
        elseif(FIND_SFML_OS_MACOSX)
            set(SFML_WINDOW_DEPENDENCIES ${SFML_WINDOW_DEPENDENCIES} "-framework OpenGL -framework Foundation -framework AppKit -framework IOKit -framework Carbon")
        endif()
        set(SFML_DEPENDENCIES ${SFML_WINDOW_DEPENDENCIES} ${SFML_DEPENDENCIES})
    endif()

    # sfml-graphics
    list(FIND SFML_FIND_COMPONENTS "graphics" FIND_SFML_GRAPHICS_COMPONENT)
    if(NOT ${FIND_SFML_GRAPHICS_COMPONENT} EQUAL -1)

        # find libraries
        find_sfml_dependency(FREETYPE_LIBRARY "FreeType" freetype)
        find_sfml_dependency(JPEG_LIBRARY "libjpeg" jpeg)

        # update the list
        set(SFML_GRAPHICS_DEPENDENCIES ${FREETYPE_LIBRARY} ${JPEG_LIBRARY})
        set(SFML_DEPENDENCIES ${SFML_GRAPHICS_DEPENDENCIES} ${SFML_DEPENDENCIES})
    endif()

    # sfml-audio
    list(FIND SFML_FIND_COMPONENTS "audio" FIND_SFML_AUDIO_COMPONENT)
    if(NOT ${FIND_SFML_AUDIO_COMPONENT} EQUAL -1)

        # find libraries
        find_sfml_dependency(OPENAL_LIBRARY "OpenAL" openal openal32)
        find_sfml_dependency(OGG_LIBRARY "Ogg" ogg)
        find_sfml_dependency(VORBIS_LIBRARY "Vorbis" vorbis)
        find_sfml_dependency(VORBISFILE_LIBRARY "VorbisFile" vorbisfile)
        find_sfml_dependency(VORBISENC_LIBRARY "VorbisEnc" vorbisenc)
        find_sfml_dependency(FLAC_LIBRARY "FLAC" FLAC)

        # update the list
        set(SFML_AUDIO_DEPENDENCIES ${OPENAL_LIBRARY} ${FLAC_LIBRARY} ${VORBISENC_LIBRARY} ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARY} ${OGG_LIBRARY})
        set(SFML_DEPENDENCIES ${SFML_DEPENDENCIES} ${SFML_AUDIO_DEPENDENCIES})
    endif()

endif()

# handle errors
if(NOT SFML_VERSION_OK)
    # SFML version not ok
    set(FIND_SFML_ERROR "SFML found but version too low (requested: ${SFML_FIND_VERSION}, found: ${SFML_VERSION_MAJOR}.${SFML_VERSION_MINOR}.${SFML_VERSION_PATCH})")
    set(SFML_FOUND FALSE)
elseif(SFML_STATIC_LIBRARIES AND FIND_SFML_DEPENDENCIES_NOTFOUND)
    set(FIND_SFML_ERROR "SFML found but some of its dependencies are missing (${FIND_SFML_DEPENDENCIES_NOTFOUND})")
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
if(SFML_FOUND AND NOT SFML_FIND_QUIETLY)
    message(STATUS "Found SFML ${SFML_VERSION_MAJOR}.${SFML_VERSION_MINOR}.${SFML_VERSION_PATCH} in ${SFML_INCLUDE_DIR}")
endif()
