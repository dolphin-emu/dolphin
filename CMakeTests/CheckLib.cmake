find_package(PkgConfig)

macro(_internal_message msg)
	if(NOT ${_is_quiet})
		message(STATUS "${msg}")
	endif()
endmacro()

macro(check_lib var pc lib)
	set(_is_required 0)
	set(_is_quiet 0)
	set(_arg_list ${ARGN})
	foreach(_arg ${ARGN})
		if(_arg STREQUAL "REQUIRED")
			list(REMOVE_ITEM _arg_list "REQUIRED")
			set(_is_required 1)
		endif()
		if(_arg STREQUAL "QUIET")
			list(REMOVE_ITEM _arg_list "QUIET")
			set(_is_quiet 1)
		endif()
	endforeach()

	if(PKG_CONFIG_FOUND AND NOT ${var}_FOUND)
		pkg_search_module(${var} QUIET ${pc})
	endif()

	if(${var}_FOUND)
		if(${var}_INCLUDE_DIRS)
			include_directories(${${var}_INCLUDE_DIRS})
		endif()
		# Make sure include directories for headers found using find_path below
		# are re-added when reconfiguring
		if(${var}_INCLUDE)
			include_directories(${${var}_INCLUDE})
		endif()
		_internal_message("${lib} found")
	else()
		find_library(${var} ${lib})
		if(_arg_list)
			find_path(${var}_INCLUDE ${_arg_list})
		else()
			set(${var}_INCLUDE FALSE)
		endif()
		if(${var} AND ${var}_INCLUDE)
			include_directories(${${var}_INCLUDE})
			_internal_message("${lib} found")
			set(${var}_FOUND 1 CACHE INTERNAL "")
		else()
			if(_is_required)
				message(FATAL_ERROR "${lib} is required but not found")
			else()
				_internal_message("${lib} not found")
			endif()
		endif()
	endif()
endmacro()

macro(check_libav)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(LIBAV libavcodec>=54.35.0 libavformat>=54.20.4
			libswscale>=2.1.1 libavutil>=52.3.0)
	else()
		if(WIN32)
			add_library(avcodec STATIC IMPORTED)
			set_target_properties(avcodec PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/Externals/ffmpeg/include
				IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Externals/ffmpeg/lib/avcodec.lib
			)

			add_library(avformat STATIC IMPORTED)
			set_target_properties(avformat PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/Externals/ffmpeg/include
				IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Externals/ffmpeg/lib/avformat.lib
			)

			add_library(avutil STATIC IMPORTED)
			set_target_properties(avutil PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/Externals/ffmpeg/include
				IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Externals/ffmpeg/lib/avutil.lib
			)

			add_library(swresample STATIC IMPORTED)
			set_target_properties(swresample PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/Externals/ffmpeg/include
				IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Externals/ffmpeg/lib/swresample.lib
			)

			add_library(swscale STATIC IMPORTED)
			set_target_properties(swscale PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/Externals/ffmpeg/include
				IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Externals/ffmpeg/lib/swscale.lib
			)

			set(LIBAV_FOUND ON)
			set(LIBAV_LIBRARIES avcodec avformat avutil swresample swscale)
		else()
			# Attempt to find it through static means
			set(LIBAV_LDFLAGS avformat avcodec swscale avutil)
			set(CMAKE_REQUIRED_LIBRARIES ${LIBAV_LDFLAGS})
			CHECK_CXX_SOURCE_COMPILES(
				"extern \"C\" {
				#include <libavcodec/avcodec.h>
				#include <libavformat/avformat.h>
				#include <libavutil/mathematics.h>
				#include <libswscale/swscale.h>
				}
				int main(int argc, char **argv)
				{
					av_register_all();
					return 0;
				}"
				LIBAV_FOUND)
			unset(CMAKE_REQUIRED_LIBRARIES)
	  endif()
	endif()
	if(LIBAV_FOUND)
		message(STATUS "libav/ffmpeg found, enabling AVI frame dumps")
		add_definitions(-DHAVE_LIBAV)
		if(LIBAV_INCLUDE_DIRS)
			include_directories(${LIBAV_INCLUDE_DIRS})
		endif()
	else()
		message(STATUS "libav/ffmpeg not found, disabling AVI frame dumps")
	endif()
endmacro()
