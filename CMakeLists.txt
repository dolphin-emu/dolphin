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
add_definitions(-DLIBS_DIR="${plugindir}/")

# These just set where the binary files will be built.  The program will not
# execute from here.  You must run "make install" to install these to the
# proper location as defined above.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Binaries)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Binaries/plugins)

include(FindSubversion OPTIONAL) # for revision info
if(Subversion_FOUND)
	Subversion_WC_INFO(${CMAKE_CURRENT_SOURCE_DIR} DOLPHIN) # defines DOLPHIN_WC_REVISION
endif()

# TODO: Make this optional or even implement our own package detection
include(FindPkgConfig REQUIRED)

# Various compile flags
add_definitions(-msse2 -Wall)

# gcc uses some optimizations which might break stuff without this flag
add_definitions(-fno-strict-aliasing)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-fvisibility-inlines-hidden VISIBILITY_INLINES_HIDDEN)
if(VISIBILITY_INLINES_HIDDEN)
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden)
endif(VISIBILITY_INLINES_HIDDEN)

CHECK_CXX_COMPILER_FLAG(-fvisibility=hidden VISIBILITY_HIDDEN)
if(VISIBILITY_HIDDEN)
	add_definitions(-fvisibility=hidden)
endif(VISIBILITY_HIDDEN)

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
# The user is free to explicitely disable it though.  Stuff which is likely to
# be needed by users is made an option with default ON, other stuff (like e.g.
# sound backends) is made completely optionally.

# TODO: wxWidgets: When building the Debug configuration, we should probably
# check if the debug wx libs are available and fall back to the bundled ones
# otherwise.

include(FindOpenGL REQUIRED)

include(FindALSA OPTIONAL)
include(FindOpenAL OPTIONAL)
include(FindwxWidgets OPTIONAL)
if(UNIX)
	include(FindX11 REQUIRED)
endif(UNIX)

pkg_search_module(PULSEAUDIO libpulse)
pkg_search_module(AO ao)
pkg_search_module(BLUEZ bluez)
pkg_search_module(XRANDR xrandr)

# TODO: Make some of these optional like explained above
if(ALSA_FOUND)
	add_definitions(-DHAVE_ALSA=1)
	message("ALSA found, enabling ALSA sound backend")
else()
	add_definitions(-DHAVE_ALSA=0)
	message("ALSA NOT found, disabling ALSA sound backend")
endif(ALSA_FOUND)

if(AO_FOUND)
	add_definitions(-DHAVE_AO=1)
	include_directories(${AO_INCLUDE_DIRS})
	message("ao found, enabling ao sound backend")
else()
	add_definitions(-DHAVE_AO=0)
	message("ao NOT found, disabling ao sound backend")
endif(AO_FOUND)

if(BLUEZ_FOUND)
	add_definitions(-DHAVE_BLUEZ=1)
	include_directories(${BLUEZ_INCLUDE_DIRS})
	message("bluez found, enabling bluetooth support")
else()
	add_definitions(-DHAVE_BLUEZ=0)
	message("bluez NOT found, enabling bluetooth support")
endif(BLUEZ_FOUND)

include_directories(${OPENGL_INCLUDE_DIR})

if(PULSEAUDIO_FOUND)
	add_definitions(-DHAVE_PULSEAUDIO=1)
	include_directories(${PULSEAUDIO_INCLUDE_DIR})
	message("PulseAudio found, enabling PulseAudio sound backend")
else()
	add_definitions(-DHAVE_PULSEAUDIO=0)
	message("PulseAudio NOT found, disabling PulseAudio sound backend")
endif(PULSEAUDIO_FOUND)

if(OPENAL_FOUND)
	add_definitions(-DHAVE_OPENAL=1)
	include_directories(${OPENAL_INCLUDE_DIR})
	message("OpenAL found, enabling OpenAL sound backend")
else()
	add_definitions(-DHAVE_OPENAL=0)
	message("OpenAL NOT found, disabling OpenAL sound backend")
endif(OPENAL_FOUND)

if(wxWidgets_FOUND)
	add_definitions(-DHAVE_WX=1)
	include(${wxWidgets_USE_FILE})

	if(UNIX)
		pkg_search_module(GTK2 REQUIRED gtk+-2.0)
		if(GTK2_FOUND)
			include_directories(${GTK2_INCLUDE_DIRS})
			message("GTK 2 found")
		else(GTK2_FOUND)
			message("GTK 2 NOT found")
		endif(GTK2_FOUND)
	endif(UNIX)

	message("wxWidgets found, enabling GUI build")
else(wxWidgets_FOUND)
	add_definitions(-DHAVE_WX=0)
	message("wxWidgets NOT found, disabling GUI build (using CLI interface)")
endif(wxWidgets_FOUND)

if(X11_FOUND)
	add_definitions(-DHAVE_X11=1)
	include_directories(${X11_INCLUDE_DIR})
	message("X11 found")
else()
	add_definitions(-DHAVE_X11=0)
	message("X11 NOT found")
endif(X11_FOUND)

if(XRANDR_FOUND)
	add_definitions(-DHAVE_XRANDR=1)
	message("Xrandr found")
else()
	add_definitions(-DHAVE_XRANDR=0)
	message("Xrandr NOT found")
endif(XRANDR_FOUND)

find_library(PORTAUDIO portaudio)
if(PORTAUDIO)
	include(CheckFunctionExists)
	set(CMAKE_REQUIRED_LIBRARIES portaudio)
	CHECK_FUNCTION_EXISTS(Pa_GetVersion PORTAUDIO_VERSION_CHECK)
endif(PORTAUDIO)
if(PORTAUDIO AND PORTAUDIO_VERSION_CHECK)
	message("PortAudio found, enabling mic support")
	add_definitions(-DHAVE_PORTAUDIO=1)
	set(PORTAUDIO_FOUND TRUE)
else()
	message("PortAudio not found, disabling mic support")
	add_definitions(-DHAVE_PORTAUDIO=0)
	set(PORTAUDIO_FOUND FALSE)
endif(PORTAUDIO AND PORTAUDIO_VERSION_CHECK)

find_library(OPENCL OpenCL)
find_path(OPENCL_INCLUDE CL/cl.h)
if(OPENCL AND OPENCL_INCLUDE)
	message("OpenCL found")
	add_definitions(-DHAVE_OPENCL=1)
	include_directories(${OPENCL_INCLUDE})
	set(OPENCL_FOUND TRUE)
else()
	message("OpenCL not found")
	add_definitions(-DHAVE_OPENCL=0)
	set(OPENCL_FOUND FALSE)
endif(OPENCL AND OPENCL_INCLUDE)


########################################
# Setup include directories (and make sure they are preferred over the Externals)
#
include_directories(Source/PluginSpecs)
include_directories(Externals)
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

find_library(LZO lzo2)
find_path(LZO_INCLUDE lzo/lzo1x.h)
if(LZO AND LZO_INCLUDE)
	message("Using shared lzo")
	include_directories(${LZO_INCLUDE})
else()
	message("Shared lzo not found, falling back to the static library")
	add_subdirectory(Externals/LZO)
	include_directories(Externals/LZO)
endif(LZO AND LZO_INCLUDE)

include(FindSDL OPTIONAL)
if(SDL_FOUND)
	message("Using shared SDL")
	include_directories(${SDL_INCLUDE_DIR})
else(SDL_FOUND)
	# TODO: No CMakeLists.txt there, yet...
	message("Shared SDL not found, falling back to the static library")
	add_subdirectory(Externals/SDL)
	include_directories(Externals/SDL/include)
endif(SDL_FOUND)

find_library(SFML_NETWORK sfml-network)
find_path(SFML_INCLUDE SFML/Network/Ftp.hpp)
if(SFML_NETWORK AND SFML_INCLUDE)
	message("Using shared sfml-network")
	include_directories(${SFML_INCLUDE})
else()
	message("Shared sfml-network not found, falling back to the static library")
	add_subdirectory(Externals/SFML)
	include_directories(Externals/SFML/include)
endif(SFML_NETWORK AND SFML_INCLUDE)

find_library(SOIL SOIL)
find_path(SOIL_INCLUDE SOIL/SOIL.h)
if(SOIL AND SOIL_INCLUDE)
	message("Using shared SOIL")
	include_directories(${SOIL_INCLUDE})
else()
	message("Shared SOIL not found, falling back to the static library")
	add_subdirectory(Externals/SOIL)
	include_directories(Externals/SOIL)
endif(SOIL AND SOIL_INCLUDE)

include(FindZLIB OPTIONAL) # TODO: Move to top
if(ZLIB_FOUND)
	include_directories(${ZLIB_INCLUDE_DIRS})
else(ZLIB_FOUND)
	# TODO: No CMakeLists.txt there, yet...
	add_subdirectory(Externals/zlib)
	include_directories(Externals/zlib)
endif(ZLIB_FOUND)

add_subdirectory(Externals/WiiUse)
include_directories(Externals/WiiUse/Src)


########################################
# Pre-build events: Define configuration variables and write svnrev header
#
file(WRITE ./Source/Core/Common/Src/svnrev.h
	"#define SVN_REV_STR \"" ${DOLPHIN_WC_REVISION} "-" ${CMAKE_BUILD_TYPE} "\"")


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
include(CPack)
set(CPACK_PACKAGE_NAME "dolphin-emu")
set(CPACK_PACKAGE_VENDOR "Dolphin Team")
set(CPACK_PACKAGE_VERSION_MAJOR "2")
set(CPACK_PACKAGE_VERSION_MINOR "0")

if(DOLPHIN_IS_STABLE)
	set(CPACK_PACKAGE_VERSION_PATCH "0")
else()
	set(CPACK_PACKAGE_VERSION_PATCH ${DOLPHIN_WC_REV})
endif()

# TODO: CPACK_PACKAGE_DESCRIPTION_FILE
# TODO: CPACK_PACKAGE_DESCRIPTION_SUMMARY
# TODO: CPACK_RESOURCE_FILE_README
# TODO: CPACK_RESOURCE_FILE_WELCOME
# TODO: CPACK_PACKAGE_EXECUTABLES
# TODO: CPACK_PACKAGE_ICON
# TODO: CPACK_NSIS_*
# TODO: Use CPack components for DSPSpy, etc => cpack_add_component
