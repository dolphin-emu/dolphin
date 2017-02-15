# - Find SDL2
# Find the SDL2 headers and libraries
#
#  SDL2::SDL2 - Imported target to use for building a library
#  SDL2::SDL2main - Imported interface target to use if you want SDL and SDLmain.
#  SDL2_FOUND - True if SDL2 was found.
#  SDL2_DYNAMIC - If we found a DLL version of SDL (meaning you might want to copy a DLL from SDL2::SDL2)
#
# Original Author:
# 2015 Ryan Pavlik <ryan.pavlik@gmail.com> <abiryan@ryand.net>
#
# Copyright Sensics, Inc. 2015.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# Set up architectures (for windows) and prefixes (for mingw builds)
if(WIN32)
	if(MINGW)
		include(MinGWSearchPathExtras OPTIONAL)
		if(MINGWSEARCH_TARGET_TRIPLE)
			set(SDL2_PREFIX ${MINGWSEARCH_TARGET_TRIPLE})
		endif()
	endif()
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(SDL2_LIB_PATH_SUFFIX lib/x64)
		if(NOT MSVC AND NOT SDL2_PREFIX)
			set(SDL2_PREFIX x86_64-w64-mingw32)
		endif()
	else()
		set(SDL2_LIB_PATH_SUFFIX lib/x86)
		if(NOT MSVC AND NOT SDL2_PREFIX)
			set(SDL2_PREFIX i686-w64-mingw32)
		endif()
	endif()
endif()

if(SDL2_PREFIX)
	set(SDL2_ORIGPREFIXPATH ${CMAKE_PREFIX_PATH})
	if(SDL2_ROOT_DIR)
		list(APPEND CMAKE_PREFIX_PATH "${SDL2_ROOT_DIR}")
	endif()
	if(CMAKE_PREFIX_PATH)
		foreach(_prefix ${CMAKE_PREFIX_PATH})
			list(APPEND CMAKE_PREFIX_PATH "${_prefix}/${SDL2_PREFIX}")
		endforeach()
	endif()
	if(MINGWSEARCH_PREFIXES)
		list(APPEND CMAKE_PREFIX_PATH ${MINGWSEARCH_PREFIXES})
	endif()
endif()

# Invoke pkgconfig for hints
find_package(PkgConfig QUIET)
set(SDL2_INCLUDE_HINTS)
set(SDL2_LIB_HINTS)
if(PKG_CONFIG_FOUND)
	pkg_search_module(SDL2PC QUIET sdl2)
	if(SDL2PC_INCLUDE_DIRS)
		set(SDL2_INCLUDE_HINTS ${SDL2PC_INCLUDE_DIRS})
	endif()
	if(SDL2PC_LIBRARY_DIRS)
		set(SDL2_LIB_HINTS ${SDL2PC_LIBRARY_DIRS})
	endif()
endif()

include(FindPackageHandleStandardArgs)

find_library(SDL2_LIBRARY
	NAMES
	SDL2
	HINTS
	${SDL2_LIB_HINTS}
	PATHS
	${SDL2_ROOT_DIR}
	ENV SDL2DIR
	PATH_SUFFIXES lib SDL2 ${SDL2_LIB_PATH_SUFFIX})

set(_sdl2_framework FALSE)
# Some special-casing if we've found/been given a framework.
# Handles whether we're given the library inside the framework or the framework itself.
if(APPLE AND "${SDL2_LIBRARY}" MATCHES "(/[^/]+)*.framework(/.*)?$")
	set(_sdl2_framework TRUE)
	set(SDL2_FRAMEWORK "${SDL2_LIBRARY}")
	# Move up in the directory tree as required to get the framework directory.
	while("${SDL2_FRAMEWORK}" MATCHES "(/[^/]+)*.framework(/.*)$" AND NOT "${SDL2_FRAMEWORK}" MATCHES "(/[^/]+)*.framework$")
		get_filename_component(SDL2_FRAMEWORK "${SDL2_FRAMEWORK}" DIRECTORY)
	endwhile()
	if("${SDL2_FRAMEWORK}" MATCHES "(/[^/]+)*.framework$")
		set(SDL2_FRAMEWORK_NAME ${CMAKE_MATCH_1})
		# If we found a framework, do a search for the header ahead of time that will be more likely to get the framework header.
		find_path(SDL2_INCLUDE_DIR
			NAMES
			SDL_haptic.h # this file was introduced with SDL2
			HINTS
			"${SDL2_FRAMEWORK}/Headers/")
	else()
		# For some reason we couldn't get the framework directory itself.
		# Shouldn't happen, but might if something is weird.
		unset(SDL2_FRAMEWORK)
	endif()
endif()

find_path(SDL2_INCLUDE_DIR
	NAMES
	SDL_haptic.h # this file was introduced with SDL2
	HINTS
	${SDL2_INCLUDE_HINTS}
	PATHS
	${SDL2_ROOT_DIR}
	ENV SDL2DIR
	PATH_SUFFIXES include include/sdl2 include/SDL2 SDL2)

if(WIN32 AND SDL2_LIBRARY)
	find_file(SDL2_RUNTIME_LIBRARY
		NAMES
		SDL2.dll
		libSDL2.dll
		HINTS
		${SDL2_LIB_HINTS}
		PATHS
		${SDL2_ROOT_DIR}
		ENV SDL2DIR
		PATH_SUFFIXES bin lib ${SDL2_LIB_PATH_SUFFIX})
endif()


if(WIN32 OR ANDROID OR IOS OR (APPLE AND NOT _sdl2_framework))
	set(SDL2_EXTRA_REQUIRED SDL2_SDLMAIN_LIBRARY)
	find_library(SDL2_SDLMAIN_LIBRARY
		NAMES
		SDL2main
		PATHS
		${SDL2_ROOT_DIR}
		ENV SDL2DIR
		PATH_SUFFIXES lib ${SDL2_LIB_PATH_SUFFIX})
endif()

if(MINGW AND NOT SDL2PC_FOUND)
	find_library(SDL2_MINGW_LIBRARY mingw32)
	find_library(SDL2_MWINDOWS_LIBRARY mwindows)
endif()

if(SDL2_PREFIX)
	# Restore things the way they used to be.
	set(CMAKE_PREFIX_PATH ${SDL2_ORIGPREFIXPATH})
endif()

# handle the QUIETLY and REQUIRED arguments and set QUATLIB_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2
	DEFAULT_MSG
	SDL2_LIBRARY
	SDL2_INCLUDE_DIR
	${SDL2_EXTRA_REQUIRED})

if(SDL2_FOUND)
	if(NOT TARGET SDL2::SDL2)
		# Create SDL2::SDL2
		if(WIN32 AND SDL2_RUNTIME_LIBRARY)
			set(SDL2_DYNAMIC TRUE)
			add_library(SDL2::SDL2 SHARED IMPORTED)
			set_target_properties(SDL2::SDL2
				PROPERTIES
				IMPORTED_IMPLIB "${SDL2_LIBRARY}"
				IMPORTED_LOCATION "${SDL2_RUNTIME_LIBRARY}"
				INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
			)
		else()
			add_library(SDL2::SDL2 UNKNOWN IMPORTED)
			if(SDL2_FRAMEWORK AND SDL2_FRAMEWORK_NAME)
				# Handle the case that SDL2 is a framework and we were able to decompose it above.
				set_target_properties(SDL2::SDL2 PROPERTIES
					IMPORTED_LOCATION "${SDL2_FRAMEWORK}/${SDL2_FRAMEWORK_NAME}")
			elseif(_sdl2_framework AND SDL2_LIBRARY MATCHES "(/[^/]+)*.framework$")
				# Handle the case that SDL2 is a framework and SDL_LIBRARY is just the framework itself.

				# This takes the basename of the framework, without the extension,
				# and sets it (as a child of the framework) as the imported location for the target.
				# This is the library symlink inside of the framework.
				set_target_properties(SDL2::SDL2 PROPERTIES
					IMPORTED_LOCATION "${SDL2_LIBRARY}/${CMAKE_MATCH_1}")
			else()
				# Handle non-frameworks (including non-Mac), as well as the case that we're given the library inside of the framework
				set_target_properties(SDL2::SDL2 PROPERTIES
					IMPORTED_LOCATION "${SDL2_LIBRARY}")
			endif()
			set_target_properties(SDL2::SDL2
				PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
			)
		endif()

		if(APPLE)
			# Need Cocoa here, is always a framework
			find_library(SDL2_COCOA_LIBRARY Cocoa)
			list(APPEND SDL2_EXTRA_REQUIRED SDL2_COCOA_LIBRARY)
			if(SDL2_COCOA_LIBRARY)
				set_target_properties(SDL2::SDL2 PROPERTIES
						IMPORTED_LINK_INTERFACE_LIBRARIES ${SDL2_COCOA_LIBRARY})
			endif()
		endif()


		# Compute what to do with SDL2main
		set(SDL2MAIN_LIBRARIES SDL2::SDL2)
		add_library(SDL2::SDL2main INTERFACE IMPORTED)
		if(SDL2_SDLMAIN_LIBRARY)
			add_library(SDL2::SDL2main_real STATIC IMPORTED)
			set_target_properties(SDL2::SDL2main_real
				PROPERTIES
				IMPORTED_LOCATION "${SDL2_SDLMAIN_LIBRARY}")
			set(SDL2MAIN_LIBRARIES SDL2::SDL2main_real ${SDL2MAIN_LIBRARIES})
		endif()
		if(MINGW)
			# MinGW requires some additional libraries to appear earlier in the link line.
			if(SDL2PC_LIBRARIES)
				# Use pkgconfig-suggested extra libraries if available.
				list(REMOVE_ITEM SDL2PC_LIBRARIES SDL2main SDL2)
				set(SDL2MAIN_LIBRARIES ${SDL2PC_LIBRARIES} ${SDL2MAIN_LIBRARIES})
			else()
				# fall back to extra libraries specified in pkg-config in
				# an official binary distro of SDL2 for MinGW I downloaded
				if(SDL2_MINGW_LIBRARY)
					set(SDL2MAIN_LIBRARIES ${SDL2_MINGW_LIBRARY} ${SDL2MAIN_LIBRARIES})
				endif()
				if(SDL2_MWINDOWS_LIBRARY)
					set(SDL2MAIN_LIBRARIES ${SDL2_MWINDOWS_LIBRARY} ${SDL2MAIN_LIBRARIES})
				endif()
			endif()
			set_target_properties(SDL2::SDL2main
				PROPERTIES
				INTERFACE_COMPILE_DEFINITIONS "main=SDL_main")
		endif()
		set_target_properties(SDL2::SDL2main
			PROPERTIES
			INTERFACE_LINK_LIBRARIES "${SDL2MAIN_LIBRARIES}")
	endif()
	mark_as_advanced(SDL2_ROOT_DIR)
endif()

mark_as_advanced(SDL2_LIBRARY
	SDL2_RUNTIME_LIBRARY
	SDL2_INCLUDE_DIR
	SDL2_SDLMAIN_LIBRARY
	SDL2_COCOA_LIBRARY
	SDL2_MINGW_LIBRARY
	SDL2_MWINDOWS_LIBRARY)

