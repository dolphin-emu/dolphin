########################################
# General setup
#
cmake_minimum_required(VERSION 2.8.8)

option(USE_EGL "Enables EGL OpenGL Interface" OFF)
option(TRY_X11 "Enables X11 Support" ON)
option(USE_UPNP "Enables UPnP port mapping support" ON)
option(DISABLE_WX "Disable wxWidgets (use Qt or CLI interface)" OFF)
option(ENABLE_QT "Enable Qt (use the experimental Qt interface)" OFF)
option(ENABLE_PCH "Use PCH to speed up compilation" ON)
option(ENABLE_LTO "Enables Link Time Optimization" OFF)
option(ENABLE_GENERIC "Enables generic build that should run on any little-endian host" OFF)

option(OPENMP "Enable OpenMP parallelization" ON)
option(ENCODE_FRAMEDUMPS "Encode framedumps in AVI format" ON)

option(FASTLOG "Enable all logs" OFF)
option(OPROFILING "Enable profiling" OFF)
option(GDBSTUB "Enable gdb stub for remote debugging." OFF)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	option(SKIP_POSTPROCESS_BUNDLE "Skip postprocessing bundle for redistributability" OFF)
endif()
########################################
# Optional Targets
# TODO: Add DSPSpy
option(DSPTOOL "Build dsptool" OFF)

# Update compiler before calling project()
if (APPLE)
	# Use clang compiler
	if (NOT DEFINED CMAKE_CXX_COMPILER)
		set(CMAKE_CXX_COMPILER "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++")
		if (NOT EXISTS "${CMAKE_CXX_COMPILER}")
			set(CMAKE_CXX_COMPILER "clang++")
		endif()
	endif()
	if (NOT DEFINED CMAKE_C_COMPILER)
		set(CMAKE_C_COMPILER "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang")
		if (NOT EXISTS "${CMAKE_C_COMPILER}")
			set(CMAKE_C_COMPILER "clang")
		endif()
	endif()
endif()
project(dolphin-emu)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/CMakeTests)
set(DOLPHIN_IS_STABLE FALSE)

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
	EXECUTE_PROCESS(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
		OUTPUT_VARIABLE DOLPHIN_WC_REVISION
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	# defines DOLPHIN_WC_DESCRIBE
	EXECUTE_PROCESS(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} describe --always --long --dirty
		OUTPUT_VARIABLE DOLPHIN_WC_DESCRIBE
		OUTPUT_STRIP_TRAILING_WHITESPACE)

	# remove hash (and trailing "-0" if needed) from description
	STRING(REGEX REPLACE "(-0)?-[^-]+((-dirty)?)$" "\\2" DOLPHIN_WC_DESCRIBE "${DOLPHIN_WC_DESCRIBE}")

	# defines DOLPHIN_WC_BRANCH
	EXECUTE_PROCESS(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
		OUTPUT_VARIABLE DOLPHIN_WC_BRANCH
		OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# version number
set(DOLPHIN_VERSION_MAJOR "4")
set(DOLPHIN_VERSION_MINOR "0")
if(DOLPHIN_IS_STABLE)
	set(DOLPHIN_VERSION_PATCH "0")
else()
	set(DOLPHIN_VERSION_PATCH ${DOLPHIN_WC_REVISION})
endif()

# Architecture detection and arch specific settings
message(${CMAKE_SYSTEM_PROCESSOR})

# Detect 64bit or 32bit
# CMake doesn't provide a simple way to determine 32bit or 64bit
# If we ever support a architecture that is 64bit with 32bit pointers then this'll break
# Of course the chances of that are slim(x32?) so who cares
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_ARCH_64 1)
	add_definitions(-D_ARCH_64=1)
else()
	set(_ARCH_32 1)
	add_definitions(-D_ARCH_32=1)
endif()

if(NOT ENABLE_GENERIC)
	if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "^x86" OR
	   ${CMAKE_SYSTEM_PROCESSOR} MATCHES "i.86" OR
	   ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		if(_ARCH_64)
			set(_M_X86 1)
			set(_M_X86_64 1)
			add_definitions(-D_M_X86=1 -D_M_X86_64=1 -msse2)
		else()
			message(FATAL_ERROR "x86_32 is an unsupported platform. Enable generic build if you really want a JIT-less binary.")
		endif()
	elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "^arm")
		# This option only applies to 32bit ARM
		set(_M_ARM 1)
		set(_M_ARM_32 1)
		add_definitions(-D_M_ARM=1 -D_M_ARM_32=1)
		if(${ANDROID_NDK_ABI_NAME} MATCHES "armeabi-v7a")
			add_definitions(-marm -march=armv7-a)
		endif()
	elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
		# This option only applies to 64bit ARM
		set(_M_ARM 1)
		set(_M_ARM_64 1)
		add_definitions(-D_M_ARM=1 -D_M_ARM_64=1)
		set(ENABLE_GENERIC 1)
	else()
		set(ENABLE_GENERIC 1)
	endif()
endif()

if(ENABLE_GENERIC)
	message("Warning! Building generic build!")
	set(_M_GENERIC 1)
	add_definitions(-D_M_GENERIC=1)
endif()

include(CheckCXXCompilerFlag)
macro(check_and_add_flag var flag)
	CHECK_CXX_COMPILER_FLAG(${flag} FLAG_${var})
	if(FLAG_${var})
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
	endif()
endmacro()

# Enabling all warnings in MSVC spams too much
if(NOT MSVC)
	add_definitions(-Wall)

	# TODO: would like these but they produce overwhelming amounts of warnings
	#check_and_add_flag(EXTRA -Wextra)
	#check_and_add_flag(MISSING_FIELD_INITIALIZERS -Wmissing-field-initializers)
	#check_and_add_flag(SWITCH_DEFAULT -Wswitch-default)
	#check_and_add_flag(FLOAT_EQUAL -Wfloat-equal)
	#check_and_add_flag(CONVERSION -Wconversion)
	#check_and_add_flag(ZERO_AS_NULL_POINTER_CONSTANT -Wzero-as-null-pointer-constant)
	check_and_add_flag(TYPE_LIMITS -Wtype-limits)
	check_and_add_flag(SIGN_COMPARE -Wsign-compare)
	check_and_add_flag(IGNORED_QUALIFIERS -Wignored-qualifiers)
	check_and_add_flag(UNINITIALIZED -Wuninitialized)
	check_and_add_flag(LOGICAL_OP -Wlogical-op)
	check_and_add_flag(SHADOW -Wshadow)
	check_and_add_flag(INIT_SELF -Winit-self)
	check_and_add_flag(MISSING_DECLARATIONS -Wmissing-declarations)
	check_and_add_flag(MISSING_VARIABLE_DECLARATIONS -Wmissing-variable-declarations)
endif(NOT MSVC)

# gcc uses some optimizations which might break stuff without this flag
add_definitions(-fno-strict-aliasing -fno-exceptions)

check_and_add_flag(VISIBILITY_INLINES_HIDDEN -fvisibility-inlines-hidden)

if(UNIX AND NOT APPLE)
	check_and_add_flag(VISIBILITY_HIDDEN -fvisibility=hidden)
endif()

if(ENABLE_LTO)
	check_and_add_flag(LTO -flto)
	if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
		set(CMAKE_AR gcc-ar)
		set(CMAKE_RANLIB gcc-ranlib)
	endif()
endif()

if(APPLE)
	# Ignore MacPorts and Fink and any other locally installed packages that
	# might prevent building a distributable binary.
	set(CMAKE_SYSTEM_PREFIX_PATH /usr)
	set(ENV{PATH} /usr/bin:/bin:/usr/sbin:/sbin)

	# Some of our code contains Objective C constructs.
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -x objective-c -stdlib=libc++")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -x objective-c++ -stdlib=libc++")
	# Avoid mistaking an object file for a source file on the link command line.
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -x none")

	# Identify the target system:
	# Ask for 64-bit binary.
	set(TARGET_FLAGS "-arch x86_64")
	# Minimum OS X version.
	# This is inserted into the Info.plist as well.
	# Note that the SDK determines the maximum version of which optional
	# features can be used, not the minimum required version to run.
	set(OSX_MIN_VERSION "10.7")
	set(TARGET_FLAGS "${TARGET_FLAGS} -mmacosx-version-min=${OSX_MIN_VERSION}")
	set(SYSROOT_LEGACY_PATH "/Developer/SDKs/MacOSX10.7.sdk")
	set(SYSROOT_PATH "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk")
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
	set(TARGET_FLAGS "${TARGET_FLAGS} -mssse3")
	set(TARGET_FLAGS "${TARGET_FLAGS} -march=core2")
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
	find_library(FORCEFEEDBACK ForceFeedback)
	find_library(OPENGL_LIBRARY OpenGL)
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

	option(ENABLE_GPROF "Enable gprof profiling (must be using Debug build)" OFF)
	if(ENABLE_GPROF)
		add_definitions(-pg)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
	endif()
endif(CMAKE_BUILD_TYPE STREQUAL Debug)

if(CMAKE_BUILD_TYPE STREQUAL Release AND NOT APPLE)
	add_definitions(-fomit-frame-pointer)
endif()

if(FASTLOG)
	add_definitions(-DDEBUGFAST)
endif()

if(GDBSTUB)
	add_definitions(-DUSE_GDBSTUB)
endif(GDBSTUB)

if(ANDROID)
	message("Building for Android")
	add_definitions(-DANDROID)
	set(USE_X11 0)
	set(USE_UPNP 0)
	set(USE_EGL 1)
endif()

include_directories(Externals/GL)

add_definitions(-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE)

########################################
# Dependency checking
#
# TODO: We should have options for dependencies included in the externals to
# override autodetection of system libraries and force the usage of the
# externals.
include(CheckLib)
include(CheckCXXSourceRuns)

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

if(NOT ANDROID)

	include(FindOpenGL)
	include_directories(${OPENGL_INCLUDE_DIR})

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

	include(FindLLVM OPTIONAL)
	if (LLVM_FOUND)
		add_definitions(-DHAS_LLVM=1)
		set(HAS_LLVM 1)

		include_directories(${LLVM_INCLUDE_DIRS})

		message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
	endif()

	set(USE_X11 0)

	if(UNIX AND NOT APPLE)
		# Note: We do not need to explicitly check for X11 as it is done in the cmake
		# FindOpenGL module on linux.
		if(TRY_X11 AND X11_FOUND)
			set(USE_X11 1)
			add_definitions(-DHAVE_X11=1)
			include_directories(${X11_INCLUDE_DIR})
			message("X11 support enabled")
		else()
			set(USE_X11 0)
			SET(X11_FOUND "")
			message("X11 support disabled")
			add_definitions(-DHAVE_X11=0)
		endif(TRY_X11 AND X11_FOUND)

		if (NOT USE_X11)
			message(FATAL_ERROR "\n"
				"No suitable display platform found\n"
				"Requires x11 to run")
		endif()
	endif()

	if(USE_X11)
		check_lib(XRANDR Xrandr)
		if(XRANDR_FOUND)
			add_definitions(-DHAVE_XRANDR=1)
		else()
			add_definitions(-DHAVE_XRANDR=0)
		endif(XRANDR_FOUND)

		pkg_check_modules(XINPUT2 xi>=1.5.0)
		if(XINPUT2_FOUND)
			add_definitions(-DHAVE_X11_XINPUT2=1)
		else()
			add_definitions(-DHAVE_X11_XINPUT2=0)
		endif(XINPUT2_FOUND)
	endif()
	if(ENCODE_FRAMEDUMPS)
		check_libav()
	endif()

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
endif()

if(USE_EGL)
	message("EGL OpenGL interface enabled")
	add_definitions(-DUSE_EGL=1)
endif()

########################################
# Setup include directories (and make sure they are preferred over the Externals)
#
include_directories(Source/Core)

########################################
# Process externals and setup their include directories
#
# NOTES about adding Externals:
#   - add the include directory here
#   - make sure to tell cmake to link them statically or dynamically (most
#     should be linked statically)
#   - place the CMakeLists.txt in the first-level subdirectory, e.g.
#     Externals/zlib/CMakeLists.txt (that is: NOT in some Src/ subdirectory)
#
add_subdirectory(Externals/Bochs_disasm)
include_directories(Externals/Bochs_disasm)

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

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT ANDROID)
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

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT ANDROID)
	check_lib(PNG png png.h QUIET)
endif()
if (PNG_FOUND)
	message("Using shared libpng")
else()
	message("Using static libpng from Externals")
	add_subdirectory(Externals/libpng)
	include_directories(Externals/libpng)
	set(PNG png)
endif()

if(OPENAL_FOUND)
	if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		check_lib(SOUNDTOUCH SoundTouch soundtouch/SoundTouch.h QUIET)
	endif()
	if (SOUNDTOUCH_FOUND)
		message("Using shared soundtouch")
	else()
		message("Using static soundtouch from Externals")
		add_subdirectory(Externals/soundtouch)
		include_directories(Externals)
	endif()
endif()

if(NOT ANDROID)
	if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		include(FindSDL2 OPTIONAL)
	endif()
	if(SDL2_FOUND)
		message("Using shared SDL2")
		add_definitions(-DHAVE_SDL=1)
		include_directories(${SDL2_INCLUDE_DIR})
	else(SDL2_FOUND)
		# SDL2 not found, try SDL
		if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
			include(FindSDL OPTIONAL)
		endif()
		if(SDL_FOUND)
			message("Using shared SDL")
			add_definitions(-DHAVE_SDL=1)
			include_directories(${SDL_INCLUDE_DIR})
		else(SDL_FOUND)
			message("SDL NOT found, disabling SDL input")
			add_definitions(-DHAVE_SDL=0)
		endif(SDL_FOUND)
	endif(SDL2_FOUND)
endif()

include(FindLibUSB OPTIONAL)
if(LIBUSB_FOUND)
	message("Using shared LibUSB")
	add_definitions(-D__LIBUSB__)
	include_directories(${LIBUSB_INCLUDE_DIR})
endif(LIBUSB_FOUND)

set(SFML_FIND_VERSION TRUE)
set(SFML_FIND_VERSION_MAJOR 1)
set(SFML_FIND_VERSION_MINOR 5)
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT ANDROID)
	include(FindSFML OPTIONAL)
endif()
if(SFML_FOUND AND NOT SFML_VERSION_MAJOR) # SFML 1.x doesn't define SFML_VERSION_MAJOR
	message("Using shared SFML")
else()
	message("Using static SFML ${SFML_FIND_VERSION_MAJOR}.${SFML_FIND_VERSION_MINOR} from Externals")
	add_subdirectory(Externals/SFML)
	include_directories(BEFORE Externals/SFML/include)
endif()

if(USE_UPNP)
	if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT ANDROID)
		include(FindMiniupnpc)
	endif()
	if(MINIUPNP_FOUND AND MINIUPNPC_VERSION_1_7_OR_HIGHER)
		message("Using shared miniupnpc")
		include_directories(${MINIUPNP_INCLUDE_DIR})
	else()
		message("Using static miniupnpc from Externals")
		add_subdirectory(Externals/miniupnpc)
		include_directories(Externals/miniupnpc/src)
	endif()
	add_definitions(-DUSE_UPNP)
endif()

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT ANDROID)
	include(FindPolarSSL)
endif()
if(POLARSSL_FOUND AND POLARSSL_WORKS)
	message("Using shared PolarSSL")
	include_directories(${POLARSSL_INCLUDE_DIR})
else()
	message("Using PolarSSL from Externals")
	set(POLARSSL_LIBRARY polarssl)
	add_subdirectory(Externals/polarssl/)
	include_directories(Externals/polarssl/include)
endif()

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT ANDROID)
	check_lib(SOIL SOIL SOIL/SOIL.h QUIET)
endif()
if(SOIL_FOUND)
	message("Using shared SOIL")
else()
	message("Using static SOIL from Externals")
	add_subdirectory(Externals/SOIL)
	include_directories(Externals/SOIL)
endif()

if (ANDROID)
	message("Using static iconv from Externals")
	include_directories(Externals/libiconv-1.14/include)
	add_subdirectory(Externals/libiconv-1.14)
endif()

if(ENABLE_QT)
	find_package(Qt5Widgets REQUIRED)
	message("Found Qt version ${Qt5Core_VERSION}, enabling the Qt backend")
endif()

if(NOT DISABLE_WX AND NOT ANDROID)
	include(FindwxWidgets OPTIONAL)
	FIND_PACKAGE(wxWidgets COMPONENTS core aui adv)

	if(wxWidgets_FOUND)
		EXECUTE_PROCESS(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
			COMMAND sh "${wxWidgets_CONFIG_EXECUTABLE}"
			${wxWidgets_CONFIG_OPTIONS} --version
			OUTPUT_VARIABLE wxWidgets_VERSION
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
			)
		message("Found wxWidgets version ${wxWidgets_VERSION}")
		if(UNIX)
			set(wxMIN_VERSION "3.0.0")
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
endif(NOT DISABLE_WX AND NOT ANDROID)


########################################
# Pre-build events: Define configuration variables and write SCM info header
#
if(DOLPHIN_WC_BRANCH STREQUAL "master")
	set(DOLPHIN_WC_IS_MASTER "1")
else()
	set(DOLPHIN_WC_IS_MASTER "0")
endif()

file(WRITE ${PROJECT_BINARY_DIR}/Source/Core/Common/scmrev.h
	"#define SCM_REV_STR \"" ${DOLPHIN_WC_REVISION} "\"\n"
	"#define SCM_DESC_STR \"" ${DOLPHIN_WC_DESCRIBE} "\"\n"
	"#define SCM_BRANCH_STR \"" ${DOLPHIN_WC_BRANCH} "\"\n"
	"#define SCM_IS_MASTER " ${DOLPHIN_WC_IS_MASTER} "\n"
	)
include_directories("${PROJECT_BINARY_DIR}/Source/Core/Common")

########################################
# Unit testing.
#
include_directories(Externals/gtest/include)
add_subdirectory(Externals/gtest)

enable_testing()
add_custom_target(unittests)
add_custom_command(TARGET unittests POST_BUILD COMMAND ${CMAKE_CTEST_COMMAND})


########################################
# Start compiling our code
#
add_definitions(-std=gnu++0x)

# These aren't actually needed for C11/C++11
# but some dependencies require them (LLVM, libav).
add_definitions(-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS)

add_subdirectory(Source)


########################################
# Install shared data files
#
if((NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
	install(DIRECTORY Data/Sys/ DESTINATION ${datadir}/sys PATTERN)
endif()
if((NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD|Darwin"))
	install(FILES Data/license.txt DESTINATION ${datadir})
endif()
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	# Install the application icon and menu item
	install(FILES Installer/dolphin-emu.xpm
		DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pixmaps)
	install(FILES Installer/dolphin-emu.desktop
		DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
endif()

# packaging information
set(CPACK_PACKAGE_NAME "dolphin-emu")
set(CPACK_PACKAGE_VENDOR "Dolphin Team")
set(CPACK_PACKAGE_VERSION_MAJOR ${DOLPHIN_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${DOLPHIN_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${DOLPHIN_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_FILE ${PROJECT_SOURCE_DIR}/Data/cpack_package_description.txt)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A GameCube, Wii and Triforce emulator")

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
Include(CPack)

