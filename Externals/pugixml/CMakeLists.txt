project(pugixml)

cmake_minimum_required(VERSION 2.8.12)

option(BUILD_SHARED_LIBS "Build shared instead of static library" OFF)

set(BUILD_DEFINES "" CACHE STRING "Build defines")

if(MSVC)
	option(STATIC_CRT "Use static CRT libraries" OFF)

	# Rewrite command line flags to use /MT if necessary
	if(STATIC_CRT)
		foreach(flag_var
				CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
				CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
			if(${flag_var} MATCHES "/MD")
				string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
			endif(${flag_var} MATCHES "/MD")
		endforeach(flag_var)
	endif()
endif()

set(HEADERS pugixml.hpp pugiconfig.hpp)
set(SOURCES ${HEADERS} pugixml.cpp)

if(DEFINED BUILD_DEFINES)
	foreach(DEFINE ${BUILD_DEFINES})
		add_definitions("-D" ${DEFINE})
	endforeach()
endif()

if(BUILD_SHARED_LIBS)
	add_library(pugixml SHARED ${SOURCES})
else()
	add_library(pugixml STATIC ${SOURCES})
endif()
dolphin_disable_warnings_msvc(pugixml)

set_target_properties(pugixml PROPERTIES VERSION 1.8 SOVERSION 1)
get_target_property(PUGIXML_VERSION_STRING pugixml VERSION)

target_include_directories(pugixml PUBLIC ./)
