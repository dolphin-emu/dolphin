########################################
# General setup
#
cmake_minimum_required(VERSION 2.6)

# Update compiler before calling project()
if (APPLE)
	# Use clang compiler
	set(CMAKE_C_COMPILER "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang")
	set(CMAKE_CXX_COMPILER "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++")
	if (NOT EXISTS "${CMAKE_CXX_COMPILER}")
		set(CMAKE_C_COMPILER "clang")
		set(CMAKE_CXX_COMPILER "clang++")
	endif()
endif()
project(dolphin-emu)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/CMakeTests)
set(DOLPHIN_IS_STABLE TRUE)

# Set up paths
if((${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
	# The gettext module will install the translations unconditionally.
	# Redirect the installation to a build directory where it does no harm.
	set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install-dummy)
else()
	set(bindir  ${CMAKE_INSTALL_PREFIX}/bin               CACHE PATH "bindir")
	set(datadir ${CMAKE_INSTALL_PREFIX}/share/dolphin-emu CACHE PATH "datadir")
	add_definitions(-DDATA_DIR="${datadir}/")
endif()
set(userdir ".dolphin-emu" CACHE STRING "User directory")
add_definitions(-DUSER_DIR="${userdir}")

# Set where the binary files will be built.  The program will not execute from
# here.  You must run "make install" to install these to the proper location
# as defined above.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Binaries)

# Precompiled header support for MSVC:
# Call this after setting the source list (and don't add the source file used
# to generate the pch file, this will be done here automatically)
function(enable_precompiled_headers PRECOMPILED_HEADER SOURCE_FILE SOURCE_VARIABLE_NAME)
	if(MSVC)
		set(files ${${SOURCE_VARIABLE_NAME}})

		# Generate precompiled header translation unit
		get_filename_component(pch_basename ${PRECOMPILED_HEADER} NAME_WE)
		set(pch_abs ${CMAKE_CURRENT_SOURCE_DIR}/${PRECOMPILED_HEADER})
		set(pch_unity ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE})
		set_source_files_properties(${pch_unity}  PROPERTIES COMPILE_FLAGS
			"/Yc\"${pch_abs}\"")

		# Update properties of source files to use the precompiled header.
		# Additionally, force the inclusion of the precompiled header at
		# beginning of each source file.
		foreach(source_file ${files} )
			set_source_files_properties(${source_file} PROPERTIES COMPILE_FLAGS
				"/Yu\"${pch_abs}\" /FI\"${pch_abs}\"")
		endforeach(source_file)

		# Finally, update the source file collection to contain the
		# precompiled header translation unit
		set(${SOURCE_VARIABLE_NAME} ${pch_unity} ${${SOURCE_VARIABLE_NAME}} PARENT_SCOPE)
	endif(MSVC)
endfunction(enable_precompiled_headers)

# for revision info
include(FindGit OPTIONAL)
if(GIT_FOUND AND NOT DOLPHIN_WC_REVISION)
# defines DOLPHIN_WC_REVISION
	EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
	OUTPUT_VARIABLE DOLPHIN_WC_REVISION
	OUTPUT_STRIP_TRAILING_WHITESPACE)
# defines DOLPHIN_WC_DESCRIBE
	EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} describe --always --long --dirty
	OUTPUT_VARIABLE DOLPHIN_WC_DESCRIBE
	OUTPUT_STRIP_TRAILING_WHITESPACE)

# remove hash from description
	STRING(REGEX REPLACE "-[^-]+((-dirty)?)$" "\\1" DOLPHIN_WC_DESCRIBE "${DOLPHIN_WC_DESCRIBE}")

# defines DOLPHIN_WC_BRANCH
	EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
	OUTPUT_VARIABLE DOLPHIN_WC_BRANCH
	OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# version number
set(DOLPHIN_VERSION_MAJOR "3")
set(DOLPHIN_VERSION_MINOR "5")
if(DOLPHIN_IS_STABLE)
	set(DOLPHIN_VERSION_PATCH "0")
else()
	set(DOLPHIN_VERSION_PATCH ${DOLPHIN_WC_REVISION})
endif()

# Various compile flags
add_definitions(-msse2)

# Enabling all warnings in MSVC spams too much
if(NOT MSVC)
	add_definitions(-Wall)
endif(NOT MSVC)

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

if(APPLE)
	# Ignore MacPorts and Fink and any other locally installed packages that
	# might prevent building a distributable binary.
	set(CMAKE_SYSTEM_PREFIX_PATH /usr)
	set(ENV{PATH} /usr/bin:/bin:/usr/sbin:/sbin)

	# Some of our code contains Objective C constructs.
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -x objective-c")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -x objective-c++")
	# Avoid mistaking an object file for a source file on the link command line.
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -x none")

	# Identify the target system:
	# Ask for 32/64-bit fat binary.
	set(TARGET_FLAGS "-arch x86_64 -arch i386")
	# Minimum OS X version.
	# This is inserted into the Info.plist as well.
	# Note that the SDK determines the maximum version of which optional
	# features can be used, not the minimum required version to run.
	set(OSX_MIN_VERSION "10.5.4")
	set(TARGET_FLAGS "${TARGET_FLAGS} -mmacosx-version-min=${OSX_MIN_VERSION}")
	set(SYSROOT_LEGACY_PATH "/Developer/SDKs/MacOSX10.6.sdk")
	set(SYSROOT_PATH "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk")
	if(EXISTS "${SYSROOT_PATH}/")
        set(TARGET_SYSROOT ${SYSROOT_PATH})
	elseif(EXISTS "${SYSROOT_LEGACY_PATH}/")
        set(TARGET_SYSROOT ${SYSROOT_LEGACY_PATH})
	endif()
	if(${TARGET_SYSROOT})
        set(TARGET_FLAGS "${TARGET_FLAGS} -isysroot ${TARGET_SYSROOT}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-syslibroot,${TARGET_SYSROOT}")
	endif()
	# Do not warn about frameworks that are not available on all architectures.
	# This avoids a warning when linking with QuickTime.
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-no_arch_warnings")
	# Specify target CPUs.
	set(TARGET_FLAGS "${TARGET_FLAGS} -Xarch_i386 -msse3")
	set(TARGET_FLAGS "${TARGET_FLAGS} -Xarch_i386 -march=prescott")
	set(TARGET_FLAGS "${TARGET_FLAGS} -Xarch_x86_64 -mssse3")
	set(TARGET_FLAGS "${TARGET_FLAGS} -Xarch_x86_64 -march=core2")
	# Target flags apply to both C and C++ compilation.
	# CMake passes these to the compiler on the link command line as well.
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TARGET_FLAGS}")

	# Linker flags.
	# Drop unreachable code and data.
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dead_strip,-dead_strip_dylibs")
	# Reserve the minimum size for the zero page.
	# Our JIT requires virtual memory space below 2GB, while the default zero
	# page on x86_64 is 4GB in size.
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-pagezero_size,0x1000")

	find_library(APPKIT_LIBRARY AppKit)
	find_library(APPSERV_LIBRARY ApplicationServices)
	find_library(ATB_LIBRARY AudioToolbox)
	find_library(AU_LIBRARY AudioUnit)
	find_library(CARBON_LIBRARY Carbon)
	find_library(COCOA_LIBRARY Cocoa)
	find_library(COREAUDIO_LIBRARY CoreAudio)
	find_library(COREFUND_LIBRARY CoreFoundation)
	find_library(CORESERV_LIBRARY CoreServices)
	find_library(IOB_LIBRARY IOBluetooth)
	find_library(IOK_LIBRARY IOKit)
	find_library(QUICKTIME_LIBRARY QuickTime)
	find_library(WEBKIT_LIBRARY WebKit)
endif()

if(WIN32)
	add_definitions(-D_SECURE_SCL=0)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
endif(WIN32)

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

option(FASTLOG "Enable all logs" OFF)
if(FASTLOG)
	add_definitions(-DDEBUGFAST)
endif()

add_definitions(-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE)

########################################
# Dependency checking
#
# TODO: We should have options for dependencies included in the externals to
# override autodetection of system libraries and force the usage of the
# externals.

include(CheckLib)

include(FindOpenGL)
include_directories(${OPENGL_INCLUDE_DIR})
if(NOT OPENGL_GLU_FOUND)
	message(FATAL_ERROR "GLU is required but not found")
endif()

option(OPENMP "Enable OpenMP parallelization" ON)
if(OPENMP)
	include(FindOpenMP OPTIONAL)
	if(OPENMP_FOUND)
		message("OpenMP parallelization enabled")
		add_definitions("${OpenMP_CXX_FLAGS}")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_CXX_FLAGS}")
	endif()
endif()
if(NOT OPENMP_FOUND)
	add_definitions(-Wno-unknown-pragmas)
	message("OpenMP parallelization disabled")
endif()

include(FindALSA OPTIONAL)
if(ALSA_FOUND)
	add_definitions(-DHAVE_ALSA=1)
	message("ALSA found, enabling ALSA sound backend")
else()
	add_definitions(-DHAVE_ALSA=0)
	message("ALSA NOT found, disabling ALSA sound backend")
endif(ALSA_FOUND)

check_lib(AO ao QUIET)
if(AO_FOUND)
	add_definitions(-DHAVE_AO=1)
	message("ao found, enabling ao sound backend")
else()
	add_definitions(-DHAVE_AO=0)
	message("ao NOT found, disabling ao sound backend")
endif(AO_FOUND)

check_lib(BLUEZ bluez QUIET)
if(BLUEZ_FOUND)
	add_definitions(-DHAVE_BLUEZ=1)
	message("bluez found, enabling bluetooth support")
else()
	add_definitions(-DHAVE_BLUEZ=0)
	message("bluez NOT found, disabling bluetooth support")
endif(BLUEZ_FOUND)

check_lib(PULSEAUDIO libpulse QUIET)
if(PULSEAUDIO_FOUND)
	add_definitions(-DHAVE_PULSEAUDIO=1)
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

if(X11_FOUND)
	check_lib(XRANDR Xrandr)
endif()
if(XRANDR_FOUND)
	add_definitions(-DHAVE_XRANDR=1)
else()
	add_definitions(-DHAVE_XRANDR=0)
endif(XRANDR_FOUND)

option(ENCODE_FRAMEDUMPS "Encode framedumps in AVI format" ON)
if(ENCODE_FRAMEDUMPS)
	check_libav()
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

option(OPROFILING "Enable profiling" OFF)
if(OPROFILING)
	check_lib(OPROFILE opagent opagent.h)
	check_lib(BFD bfd bfd.h)
	if(OPROFILE_FOUND AND BFD_FOUND)
		message("oprofile found, enabling profiling support")
		add_definitions(-DUSE_OPROFILE=1)
	else()
		message(FATAL_ERROR "oprofile or bfd not found. Can't build profiling support.")
	endif()
endif()

########################################
# Setup include directories (and make sure they are preferred over the Externals)
#
include_directories(Source/Core/AudioCommon/Src)
include_directories(Source/Core/Common/Src)
include_directories(Source/Core/Core/Src)
include_directories(Source/Core/DebuggerUICommon/Src)
include_directories(Source/Core/DebuggerWX/Src)
include_directories(Source/Core/DiscIO/Src)
include_directories(Source/Core/DolphinWX/Src)
include_directories(Source/Core/InputCommon/Src)
include_directories(Source/Core/VideoCommon/Src)
include_directories(Source/Core/VideoUICommon/Src)


########################################
# Process externals and setup their include directories
#
# NOTES about adding Externals:
#   - add the include directory here
#   - make sure to tell cmake to link them statically or dynamically (most
#     should be linked statically)
#   - place the CMakeLists.txt in the first-level subdirectory, e.g.
#     Externals/CLRun/CMakeLists.txt (that is: NOT in some Src/ subdirectory)
#
add_subdirectory(Externals/Bochs_disasm)
include_directories(Externals/Bochs_disasm)

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	check_lib(LZO lzo2 lzo/lzo1x.h QUIET)
endif()
if(LZO_FOUND)
	message("Using shared lzo")
else()
	message("Using static lzo from Externals")
	add_subdirectory(Externals/LZO)
	include_directories(Externals/LZO)
	set(LZO lzo2)
endif()

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	include(FindSDL2 OPTIONAL)
endif()
if(SDL2_FOUND)
	message("Using shared SDL2")
	include_directories(${SDL2_INCLUDE_DIR})
else(SDL2_FOUND)
	# SDL2 not found, try SDL
	if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		include(FindSDL OPTIONAL)
	endif()
	if(SDL_FOUND)
		message("Using shared SDL")
		include_directories(${SDL_INCLUDE_DIR})
	else(SDL_FOUND)
		# TODO: Use the prebuilt one on Windows
		message("Using static SDL from Externals")
		include_directories(Externals/SDL Externals/SDL/include)
		add_subdirectory(Externals/SDL)
	endif(SDL_FOUND)
endif(SDL2_FOUND)

set(SFML_FIND_VERSION TRUE)
set(SFML_FIND_VERSION_MAJOR 1)
set(SFML_FIND_VERSION_MINOR 5)
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	include(FindSFML OPTIONAL)
endif()
if(SFML_FOUND AND NOT SFML_VERSION_MAJOR) # SFML 1.x doesn't define SFML_VERSION_MAJOR
       message("Using shared SFML")
else()
       message("Using static SFML ${SFML_FIND_VERSION_MAJOR}.${SFML_FIND_VERSION_MINOR} from Externals")
       add_subdirectory(Externals/SFML)
       include_directories(Externals/SFML/include)
endif()

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	check_lib(SOIL SOIL SOIL/SOIL.h QUIET)
endif()
if(SOIL_FOUND)
	message("Using shared SOIL")
else()
	message("Using static SOIL from Externals")
	add_subdirectory(Externals/SOIL)
	include_directories(Externals/SOIL)
endif()

# If zlib has already been found on a previous run of cmake don't check again
# as the check seems to take a long time.
if(NOT ZLIB_FOUND)
	include(FindZLIB OPTIONAL)
endif()
if(ZLIB_FOUND)
	set(ZLIB_FOUND 1 CACHE INTERNAL "")
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
	if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		check_lib(GLEW GLEW GL/glew.h)
	endif()
	if(NOT GLEW_FOUND)
		message("Using static GLEW from Externals")
		add_subdirectory(Externals/GLew)
		include_directories(Externals/GLew/include)
	endif(NOT GLEW_FOUND)

	if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		check_lib(CG Cg Cg/cg.h)
	endif()
	if(NOT CG_FOUND)
		message("Using static Cg from Externals")
		include_directories(Externals)
	endif(NOT CG_FOUND)
	check_lib(CGGL CgGL Cg/cgGL.h)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	find_library(CL OpenCL)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-weak_framework,OpenCL")
else()
	include_directories(Externals/CLRun/include)
	add_subdirectory(Externals/CLRun)
endif()

option(DISABLE_WX "Disable wxWidgets (use CLI interface)" OFF)
if(NOT DISABLE_WX)
	include(FindwxWidgets OPTIONAL)
	FIND_PACKAGE(wxWidgets COMPONENTS core aui adv)

	if(wxWidgets_FOUND)
		EXECUTE_PROCESS(
			COMMAND sh "${wxWidgets_CONFIG_EXECUTABLE}"
				${wxWidgets_CONFIG_OPTIONS} --version
			OUTPUT_VARIABLE wxWidgets_VERSION
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
			)
		message("Found wxWidgets version ${wxWidgets_VERSION}")
		if(UNIX AND NOT APPLE)
			set(wxMIN_VERSION "2.9.3")
		else()
			set(wxMIN_VERSION "2.9.4")
		endif()
		if(${wxWidgets_VERSION} VERSION_LESS ${wxMIN_VERSION})
			message("At least ${wxMIN_VERSION} is required; ignoring found version")
			unset(wxWidgets_FOUND)
		endif()
	endif(wxWidgets_FOUND)

	if(UNIX AND NOT APPLE)
		# There is a bug in the FindGTK module in cmake version 2.8.2 that
		# does not find gdk-pixbuf-2.0.  On the other hand some 2.8.3
		# users have complained that pkg-config does not find
		# gdk-pixbuf-2.0.  On yet another hand, cmake version 2.8.3 in
		# Ubuntu Natty does not find the glib libraries correctly.
		# Ugly!!!
		execute_process(COMMAND lsb_release -c -s
			OUTPUT_VARIABLE DIST_NAME
			ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
		if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION}
				VERSION_EQUAL 2.8.2 OR "${DIST_NAME}" STREQUAL "natty")
			check_lib(GTK2 gtk+-2.0 gtk.h REQUIRED)
		else()
			include(FindGTK2)
			if(GTK2_FOUND)
				include_directories(${GTK2_INCLUDE_DIRS})
			endif()
		endif()
	endif()

	if(wxWidgets_FOUND)
		include(${wxWidgets_USE_FILE})
		message("wxWidgets found, enabling GUI build")
	else(wxWidgets_FOUND)
		message("Using static wxWidgets from Externals")

		# These definitions and includes are used when building dolphin against wx,
		# not when building wx itself (see wxw3 CMakeLists.txt for that)
		if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
			add_definitions(-D__WXOSX_COCOA__)
		elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
			add_definitions(-D__WXGTK__)

			# Check for required libs
			check_lib(GTHREAD2 gthread-2.0 glib/gthread.h REQUIRED)
			check_lib(PANGOCAIRO pangocairo pango/pangocairo.h REQUIRED)
		elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
			add_definitions(-D__WXMSW__)
		else()
			message(FATAL_ERROR "wxWidgets in Externals is not compatible with your platform")
		endif()
		
		include_directories(
			Externals/wxWidgets3
			Externals/wxWidgets3/include)
		add_subdirectory(Externals/wxWidgets3)
		set(wxWidgets_FOUND TRUE)
		set(wxWidgets_LIBRARIES "wx")
	endif(wxWidgets_FOUND)
	add_definitions(-DHAVE_WX=1)
endif(NOT DISABLE_WX)


########################################
# Pre-build events: Define configuration variables and write SCM info header
#
if(DOLPHIN_WC_BRANCH STREQUAL "master")
	set(DOLPHIN_WC_IS_MASTER "1")
else()
	set(DOLPHIN_WC_IS_MASTER "0")
endif()

file(WRITE ${PROJECT_BINARY_DIR}/Source/Core/Common/Src/scmrev.h
	"#define SCM_REV_STR \"" ${DOLPHIN_WC_REVISION} "\"\n"
	"#define SCM_DESC_STR \"" ${DOLPHIN_WC_DESCRIBE} "\"\n"
	"#define SCM_BRANCH_STR \"" ${DOLPHIN_WC_BRANCH} "\"\n"
	"#define SCM_IS_MASTER " ${DOLPHIN_WC_IS_MASTER} "\n"
	)
include_directories("${PROJECT_BINARY_DIR}/Source/Core/Common/Src")

########################################
# Optional Targets
# TODO: Add DSPSpy and TestSuite.
option(DSPTOOL "Build dsptool" OFF)
option(UNITTESTS "Build unitests" OFF)

########################################
# Start compiling our code
#
add_definitions(-std=gnu++0x)
add_subdirectory(Source)


########################################
# Install shared data files
#
if((NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
	install(DIRECTORY Data/User/ DESTINATION ${datadir}/user PATTERN)
	install(DIRECTORY Data/Sys/ DESTINATION ${datadir}/sys PATTERN)
endif()
include(FindGettext)
if(GETTEXT_FOUND AND NOT DISABLE_WX)
	file(GLOB LINGUAS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} Languages/po/*.po) 
	GETTEXT_CREATE_TRANSLATIONS(Languages/po/dolphin-emu.pot ALL ${LINGUAS})
endif()
if((NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD|Darwin"))
	install(FILES Data/license.txt DESTINATION ${datadir})
endif()

# packaging information
set(CPACK_PACKAGE_NAME "dolphin-emu")
set(CPACK_PACKAGE_VENDOR "Dolphin Team")
set(CPACK_PACKAGE_VERSION_MAJOR ${DOLPHIN_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${DOLPHIN_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${DOLPHIN_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_FILE ${PROJECT_SOURCE_DIR}/Data/cpack_package_description.txt)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Gamecube, Wii and Triforce emulator")

set(CPACK_RPM_PACKAGE_GROUP System/Emulators/Other)
set(CPACK_RPM_PACKAGE_LICENSE GPL-2.0)
# TODO: CPACK_RESOURCE_FILE_README
# TODO: CPACK_RESOURCE_FILE_WELCOME
# TODO: CPACK_PACKAGE_ICON
# TODO: CPACK_NSIS_*
# TODO: Use CPack components for DSPSpy, etc => cpack_add_component

set(CPACK_SET_DESTDIR ON)
set(CPACK_SOURCE_GENERATOR "TGZ;TBZ2;ZIP")
set(CPACK_SOURCE_IGNORE_FILES  "\\\\.#;/#;.*~;\\\\.swp;/\\\\.git")
list(APPEND CPACK_SOURCE_IGNORE_FILES "${CMAKE_BINARY_DIR}")

# CPack must be included after the CPACK_* variables are set in order for those
# variables to take effect.
include(CPack)
