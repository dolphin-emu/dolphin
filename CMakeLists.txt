########################################
# General setup
#
cmake_minimum_required(VERSION 2.6)
project(dolphin-emu)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/CMakeTests)

set(DOLPHIN_IS_STABLE FALSE)
set(prefix		${CMAKE_INSTALL_PREFIX}						CACHE PATH "prefix")
set(bindir		${CMAKE_INSTALL_PREFIX}/bin					CACHE PATH "bindir")
set(libdir		${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}	CACHE PATH "libdir")
set(plugindir	${libdir}/dolphin-emu						CACHE PATH "plugindir")
set(datadir		${CMAKE_INSTALL_PREFIX}/share/dolphin-emu	CACHE PATH "datadir")

# Set up paths
set(userdir ".dolphin-emu" CACHE STRING "User directory")
add_definitions(-DUSER_DIR="${userdir}")
add_definitions(-DDATA_DIR="${datadir}/")
add_definitions(-DLIBS_DIR="${plugindir}")

# These just set where the binary files will be built.  The program will not
# execute from here.  You must run "make install" to install these to the
# proper location as defined above.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Binaries)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Binaries/plugins)

include(FindSubversion OPTIONAL) # for revision info
if(Subversion_FOUND AND NOT DOLPHIN_WC_REVISION)
	Subversion_WC_INFO(${CMAKE_CURRENT_SOURCE_DIR} DOLPHIN) # defines DOLPHIN_WC_REVISION
endif()

# TODO: Make this optional or even implement our own package detection
include(FindPkgConfig REQUIRED)

# Various compile flags
add_definitions(-msse2 -Wall)

# gcc uses some optimizations which might break stuff without this flag
add_definitions(-fno-strict-aliasing -fno-exceptions)

include(CheckCXXCompilerFlag)

# We call fread numerous times without checking return values.  Hide the
# corresponding compiler warnings if the compiler supports doing so.
CHECK_CXX_COMPILER_FLAG(-Wunused-result NO_UNUSED_RESULT)
if(NO_UNUSED_RESULT)
	add_definitions(-Wno-unused-result)
endif(NO_UNUSED_RESULT)

CHECK_CXX_COMPILER_FLAG(-fvisibility-inlines-hidden VISIBILITY_INLINES_HIDDEN)
if(VISIBILITY_INLINES_HIDDEN)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
endif(VISIBILITY_INLINES_HIDDEN)

if(UNIX AND NOT APPLE)
	CHECK_CXX_COMPILER_FLAG(-fvisibility=hidden VISIBILITY_HIDDEN)
	if(VISIBILITY_HIDDEN)
		add_definitions(-fvisibility=hidden)
	endif(VISIBILITY_HIDDEN)
endif()

if(WIN32)
	add_definitions(-D_SECURE_SCL=0)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
endif(WIN32)

add_definitions(-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING
		"Build type (Release/Debug/RelWithDebInfo/MinSizeRe)" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
	add_definitions(-D_DEBUG -ggdb)
	set(wxWidgets_USE_DEBUG ON CACHE BOOL "Use wxWidgets Debugging")
endif(CMAKE_BUILD_TYPE STREQUAL Debug)

if(CMAKE_BUILD_TYPE STREQUAL Release)
	add_definitions(-fomit-frame-pointer)
endif(CMAKE_BUILD_TYPE STREQUAL Release)

########################################
# Dependency checking
#
# NOTES:
# There are numerous possible cases:
# - dependency may be required or optional
# - dependency may be already installed (but optionally the bundled one may be used)
# 
# TODO: We should have a number of options for optional dependencies (disable,
# force bundled, bundled or native, force native).  For example the OpenGL
# plugin defaults to force native, so we error out if no GL libs are found.
# The user is free to explicitly disable it though.  Stuff which is likely to
# be needed by users is optional defaulting to ON, other stuff (like e.g.
# sound backends) is completely optional.

include(FindOpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

include(FindALSA OPTIONAL)
if(ALSA_FOUND)
	add_definitions(-DHAVE_ALSA=1)
	message("ALSA found, enabling ALSA sound backend")
else()
	add_definitions(-DHAVE_ALSA=0)
	message("ALSA NOT found, disabling ALSA sound backend")
endif(ALSA_FOUND)

pkg_search_module(AO ao)
if(AO_FOUND)
	add_definitions(-DHAVE_AO=1)
	include_directories(${AO_INCLUDE_DIRS})
	message("ao found, enabling ao sound backend")
else()
	add_definitions(-DHAVE_AO=0)
	message("ao NOT found, disabling ao sound backend")
endif(AO_FOUND)

pkg_search_module(BLUEZ bluez)
if(BLUEZ_FOUND)
	add_definitions(-DHAVE_BLUEZ=1)
	include_directories(${BLUEZ_INCLUDE_DIRS})
	message("bluez found, enabling bluetooth support")
else()
	add_definitions(-DHAVE_BLUEZ=0)
	message("bluez NOT found, disabling bluetooth support")
endif(BLUEZ_FOUND)

pkg_search_module(PULSEAUDIO libpulse)
if(PULSEAUDIO_FOUND)
	add_definitions(-DHAVE_PULSEAUDIO=1)
	include_directories(${PULSEAUDIO_INCLUDE_DIR})
	message("PulseAudio found, enabling PulseAudio sound backend")
else()
	add_definitions(-DHAVE_PULSEAUDIO=0)
	message("PulseAudio NOT found, disabling PulseAudio sound backend")
endif(PULSEAUDIO_FOUND)

include(FindOpenAL OPTIONAL)
if(OPENAL_FOUND)
	add_definitions(-DHAVE_OPENAL=1)
	include_directories(${OPENAL_INCLUDE_DIR})
	message("OpenAL found, enabling OpenAL sound backend")
else()
	add_definitions(-DHAVE_OPENAL=0)
	message("OpenAL NOT found, disabling OpenAL sound backend")
endif(OPENAL_FOUND)

# Note: We do not need to explicitly check for X11 as it is done in the cmake
# FindOpenGL module on linux.
if(UNIX AND NOT APPLE)
	if(X11_FOUND)
		add_definitions(-DHAVE_X11=1)
		include_directories(${X11_INCLUDE_DIR})
		message("X11 found")
	else()
		message(FATAL_ERROR "X11 is required but not found")
	endif(X11_FOUND)
else()
	add_definitions(-DHAVE_X11=0)
endif()

pkg_search_module(XRANDR xrandr)
if(XRANDR_FOUND)
	add_definitions(-DHAVE_XRANDR=1)
	message("Xrandr found")
else()
	add_definitions(-DHAVE_XRANDR=0)
	message("Xrandr NOT found")
endif(XRANDR_FOUND)

option(ENCODE_FRAMEDUMPS "Encode framedumps in AVI format" ON)
if(ENCODE_FRAMEDUMPS)
	pkg_search_module(AVCODEC libavcodec>=52.72.2)
	pkg_search_module(AVFORMAT libavformat>=52.64.2)
	pkg_search_module(SWSCALE libswscale>=0.11.0)
	if(AVCODEC_FOUND AND AVFORMAT_FOUND AND SWSCALE_FOUND)
		message("libav found, enabling AVI frame dumps")
		set(ENCODE_FRAMEDUMPS ON)
		add_definitions(-DHAVE_AVCODEC)
	else()
		set(ENCODE_FRAMEDUMPS OFF)
		message("libav not found, disabling AVI frame dumps")
	endif()
endif()

find_program(XDG_SCREENSAVER xdg-screensaver)
if(XDG_SCREENSAVER)
	message("xdg-screensaver found, enabling screensaver inhibition")
	add_definitions(-DHAVE_XDG_SCREENSAVER=1)
else()
	message("xdg-screensaver not found, disabling screensaver inhibition")
	add_definitions(-DHAVE_XDG_SCREENSAVER=0)
endif()

include(CheckCXXSourceRuns)
set(CMAKE_REQUIRED_LIBRARIES portaudio)
CHECK_CXX_SOURCE_RUNS(
	"#include <portaudio.h>
	int main(int argc, char **argv)
	{ if(Pa_GetVersion() >= 1890) return 0; else return 1; }"
	PORTAUDIO)
if(PORTAUDIO)
	message("PortAudio found, enabling mic support")
	add_definitions(-DHAVE_PORTAUDIO=1)
	set(PORTAUDIO_FOUND TRUE)
else()
	message("PortAudio not found, disabling mic support")
	add_definitions(-DHAVE_PORTAUDIO=0)
	set(PORTAUDIO_FOUND FALSE)
endif(PORTAUDIO)


########################################
# Setup include directories (and make sure they are preferred over the Externals)
#
include_directories(Source/PluginSpecs)
include_directories(Source/Core/AudioCommon/Src)
include_directories(Source/Core/Common/Src)
include_directories(Source/Core/Core/Src)
include_directories(Source/Core/DebuggerUICommon/Src)
include_directories(Source/Core/DebuggerWX/Src)
include_directories(Source/Core/DiscIO/Src)
include_directories(Source/Core/DolphinWX/Src)
include_directories(Source/Core/DSPCore/Src)
include_directories(Source/Core/InputCommon/Src)
include_directories(Source/Core/InputUICommon/Src)
include_directories(Source/Core/VideoCommon/Src)
include_directories(Source/Core/VideoUICommon/Src)
include_directories(Source/Core/wiiuse/Src)


########################################
# Process externals and setup their include directories
#
# NOTES about adding Externals:
#   - add the include directory here
#   - make sure to tell cmake to link them statically or dynamically (most
#     should be linked statically)
#   - place the CMakeLists.txt in the first-level subdirectory, e.g.
#     Externals/WiiUse/CMakeLists.txt (that is: NOT in some Src/ subdirectory)
#
add_subdirectory(Externals/Bochs_disasm)
include_directories(Externals/Bochs_disasm)

# TODO: Try using the native lib first
# To use the native lib for Lua the dolphin code will need to be changed.
# Currently the file lstate.h is improperly included.
add_subdirectory(Externals/Lua)
include_directories(Externals/Lua)

include(CheckLib)

check_lib_and_header(LZO lzo2 lzo/lzo1x.h OPTIONAL)
if(LZO_FOUND)
	message("Using shared lzo")
	include_directories(${LZO_INCLUDE})
else()
	message("Shared lzo not found, falling back to the static library")
	add_subdirectory(Externals/LZO)
	include_directories(Externals/LZO)
endif()

include(FindSDL OPTIONAL)
if(SDL_FOUND)
	message("Using shared SDL")
	include_directories(${SDL_INCLUDE_DIR})
else(SDL_FOUND)
	# TODO: Use the prebuilt one on Windows
	message("Shared SDL not found, falling back to the static library")
	include_directories(Externals/SDL Externals/SDL/include)
	add_subdirectory(Externals/SDL)
endif(SDL_FOUND)

check_lib_and_header(SFML sfml-network SFML/Network/Ftp.hpp OPTIONAL)
if(SFML_FOUND)
	message("Using shared sfml-network")
	include_directories(${SFML_INCLUDE})
else()
	message("Shared sfml-network not found, falling back to the static library")
	add_subdirectory(Externals/SFML)
	include_directories(Externals/SFML/include)
endif()

check_lib_and_header(SOIL SOIL SOIL/SOIL.h OPTIONAL)
if(SOIL_FOUND)
	message("Using shared SOIL")
	include_directories(${SOIL_INCLUDE})
else()
	message("Shared SOIL not found, falling back to the static library")
	add_subdirectory(Externals/SOIL)
	include_directories(Externals/SOIL)
endif()

include(FindZLIB OPTIONAL)
if(ZLIB_FOUND)
	message("Using shared zlib")
	include_directories(${ZLIB_INCLUDE_DIRS})
else(ZLIB_FOUND)
	message("Shared zlib not found, falling back to the static library")
	add_subdirectory(Externals/zlib)
	include_directories(Externals/zlib)
endif(ZLIB_FOUND)

if(WIN32)
	find_library(GLEW glew32s PATHS Externals/GLew)
	include_directories(Externals/GLew/include)
else()
	check_lib(GLU glu REQUIRED)
	check_lib_and_header(GLEW GLEW GL/glew.h OPTIONAL)
	check_lib_and_header(CG Cg Cg/cg.h OPTIONAL)
	check_lib_and_header(CGGL CgGL Cg/cgGL.h OPTIONAL)
endif()

if(NOT GLEW_FOUND)
	message("Shared GLEW not found, falling back to the static library")
	add_subdirectory(Externals/GLew)
	include_directories(Externals/GLew/include)
endif(NOT GLEW_FOUND)

if(NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
	include_directories(Externals/CLRun/include)
	add_subdirectory(Externals/CLRun)
endif()

option(DISABLE_WX "Disable wxWidgets (use CLI interface)" OFF)
if(NOT DISABLE_WX)
	include(FindwxWidgets OPTIONAL)

	if(wxWidgets_FOUND)
		include(${wxWidgets_USE_FILE})

		if(UNIX AND NOT APPLE)
			check_lib(GTK2 gtk+-2.0 REQUIRED)
			if(GTK2_FOUND)
				include_directories(${GTK2_INCLUDE_DIRS})
			endif(GTK2_FOUND)
		endif()

		message("wxWidgets found, enabling GUI build")
	else(wxWidgets_FOUND)
		message("Shared wxWidgets not found, falling back to the static library")
		include_directories(Externals/wxWidgets/include)
		add_subdirectory(Externals/wxWidgets)
	endif(wxWidgets_FOUND)
	add_definitions(-DHAVE_WX=1)
endif(NOT DISABLE_WX)


########################################
# Pre-build events: Define configuration variables and write svnrev header
#
file(WRITE ${PROJECT_BINARY_DIR}/Source/Core/Common/Src/svnrev.h
	"#define SVN_REV_STR \"" ${DOLPHIN_WC_REVISION} "-" ${CMAKE_BUILD_TYPE} "\"\n")

########################################
# Start compiling our code
#
add_subdirectory(Source)


########################################
# Install shared data files
#
install(DIRECTORY Data/User/ DESTINATION ${datadir}/user PATTERN .svn EXCLUDE)
install(DIRECTORY Data/Sys/ DESTINATION ${datadir}/sys PATTERN .svn EXCLUDE)
install(FILES Data/license.txt DESTINATION ${datadir})

# packaging information
set(CPACK_PACKAGE_NAME "dolphin-emu")
set(CPACK_PACKAGE_VENDOR "Dolphin Team")
set(CPACK_PACKAGE_VERSION_MAJOR "2")
set(CPACK_PACKAGE_VERSION_MINOR "0")

if(DOLPHIN_IS_STABLE)
	set(CPACK_PACKAGE_VERSION_PATCH "0")
else()
	set(CPACK_PACKAGE_VERSION_PATCH ${DOLPHIN_WC_REVISION})
endif()

# TODO: CPACK_PACKAGE_DESCRIPTION_FILE
# TODO: CPACK_PACKAGE_DESCRIPTION_SUMMARY
# TODO: CPACK_RESOURCE_FILE_README
# TODO: CPACK_RESOURCE_FILE_WELCOME
# TODO: CPACK_PACKAGE_EXECUTABLES
# TODO: CPACK_PACKAGE_ICON
# TODO: CPACK_NSIS_*
# TODO: Use CPack components for DSPSpy, etc => cpack_add_component

# CPack must be included after the CPACK_* variables are set in order for those
# variables to take effect.
include(CPack)
