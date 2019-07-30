########################################
# General setup
#
cmake_minimum_required(VERSION 3.10)
set(CMAKE_OSX_ARCHITECTURES "x86_64")
# Minimum OS X version.
# This is inserted into the Info.plist as well.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.10.0" CACHE STRING "")

project(dolphin-emu)

# Name of the Dolphin distributor. If you redistribute Dolphin builds (forks,
# unofficial builds) please consider identifying your distribution with a
# unique name here.
set(DISTRIBUTOR "None" CACHE STRING "Name of the distributor.")

if(UNIX AND NOT APPLE AND NOT ANDROID)
  option(ENABLE_X11 "Enables X11 Support" ON)
endif()
if(NOT WIN32 AND NOT APPLE)
  option(ENABLE_EGL "Enables EGL OpenGL Interface" ON)
endif()

option(USE_SHARED_ENET "Use shared libenet if found rather than Dolphin's soon-to-compatibly-diverge version" OFF)
option(USE_UPNP "Enables UPnP port mapping support" ON)
option(ENABLE_NOGUI "Enable NoGUI frontend" ON)
option(ENABLE_QT "Enable Qt (Default)" ON)
option(ENABLE_LTO "Enables Link Time Optimization" OFF)
option(ENABLE_GENERIC "Enables generic build that should run on any little-endian host" OFF)
option(ENABLE_HEADLESS "Enables running Dolphin as a headless variant" OFF)
option(ENABLE_ALSA "Enables ALSA sound backend" ON)
option(ENABLE_PULSEAUDIO "Enables PulseAudio sound backend" ON)
option(ENABLE_LLVM "Enables LLVM support, for disassembly" ON)
option(ENABLE_TESTS "Enables building the unit tests" ON)
option(USE_DISCORD_PRESENCE "Enables Discord Rich Presence, show the current game on Discord" ON)

# Maintainers: if you consider blanket disabling this for your users, please
# consider the following points:
#  * No data is being sent without explicit user approval (pop up box at first
#    launch).
#  * The Dolphin team relies on the data in order to understand the behavior
#    of our software in the wild.
option(ENABLE_ANALYTICS "Enables opt-in Analytics collection" ON)

option(ENCODE_FRAMEDUMPS "Encode framedumps in AVI format" ON)

option(ENABLE_GPROF "Enable gprof profiling (must be using Debug build)" OFF)
option(FASTLOG "Enable all logs" OFF)
option(GDBSTUB "Enable gdb stub for remote debugging." OFF)
option(OPROFILING "Enable profiling" OFF)

# TODO: Add DSPSpy
option(DSPTOOL "Build dsptool" OFF)

# Enable SDL for default on operating systems that aren't OSX, Android, Linux or Windows.
if(NOT APPLE AND NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT MSVC)
  option(ENABLE_SDL "Enables SDL as a generic controller backend" ON)
else()
  option(ENABLE_SDL "Enables SDL as a generic controller backend" OFF)
endif()

if(APPLE)
  option(OSX_USE_DEFAULT_SEARCH_PATH "Don't prioritize system library paths" OFF)
  option(SKIP_POSTPROCESS_BUNDLE "Skip postprocessing bundle for redistributability" OFF)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  option(ENABLE_VTUNE "Enable Intel VTune integration for JIT code." OFF)

  if(NOT ANDROID)
    option(ENABLE_EVDEV "Enables the evdev controller backend" ON)
  endif()
endif()

if(UNIX)
  # Builds a relocatable binary on Linux.
  # The Sys folder will need to be copied to the Binaries folder.
  option(LINUX_LOCAL_DEV "Enable relocatable binary" OFF)
endif()

list(APPEND CMAKE_MODULE_PATH
  ${CMAKE_SOURCE_DIR}/CMake
)

# Support functions
include(CheckAndAddFlag)
include(CheckCCompilerFlag)
include(DolphinCompileDefinitions)

# Enable folders for IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set up paths
set(bindir  ${CMAKE_INSTALL_PREFIX}/bin               CACHE PATH "bindir")
set(datadir ${CMAKE_INSTALL_PREFIX}/share/dolphin-emu CACHE PATH "datadir")
set(mandir  ${CMAKE_INSTALL_PREFIX}/share/man         CACHE PATH "mandir")
add_definitions(-DDATA_DIR="${datadir}/")

if(CMAKE_SYSROOT)
  # If we should use a sysroot, tell pkg-config to search for packages in there, not on the host
  set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
  set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
endif()

# Set where the binary files will be built.  The program will not execute from
# here.  You must run "make install" to install these to the proper location
# as defined above.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Binaries)

if (WIN32)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Binary)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

# setup CCache
include(CCache)

# for revision info
find_package(Git)
if(GIT_FOUND)
  # make sure version information gets re-run when the current Git HEAD changes
  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse --git-path HEAD
      OUTPUT_VARIABLE dolphin_git_head_filename
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${dolphin_git_head_filename}")

  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse --symbolic-full-name HEAD
      OUTPUT_VARIABLE dolphin_git_head_symbolic
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      COMMAND ${GIT_EXECUTABLE} rev-parse --git-path ${dolphin_git_head_symbolic}
      OUTPUT_VARIABLE dolphin_git_head_symbolic_filename
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${dolphin_git_head_symbolic_filename}")

  # defines DOLPHIN_WC_REVISION
  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
      OUTPUT_VARIABLE DOLPHIN_WC_REVISION
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  # defines DOLPHIN_WC_DESCRIBE
  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} describe --always --long --dirty
      OUTPUT_VARIABLE DOLPHIN_WC_DESCRIBE
      OUTPUT_STRIP_TRAILING_WHITESPACE)

  # remove hash (and trailing "-0" if needed) from description
  string(REGEX REPLACE "(-0)?-[^-]+((-dirty)?)$" "\\2" DOLPHIN_WC_DESCRIBE "${DOLPHIN_WC_DESCRIBE}")

  # defines DOLPHIN_WC_BRANCH
  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
      OUTPUT_VARIABLE DOLPHIN_WC_BRANCH
      OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# version number
set(DOLPHIN_VERSION_MAJOR "5")
set(DOLPHIN_VERSION_MINOR "0")
if(DOLPHIN_WC_BRANCH STREQUAL "stable")
  set(DOLPHIN_VERSION_PATCH "0")
else()
  set(DOLPHIN_VERSION_PATCH ${DOLPHIN_WC_REVISION})
endif()

# If Dolphin is not built from a Git repository, default the version info to
# reasonable values.
if(NOT DOLPHIN_WC_REVISION)
  set(DOLPHIN_WC_DESCRIBE "${DOLPHIN_VERSION_MAJOR}.${DOLPHIN_VERSION_MINOR}")
  set(DOLPHIN_WC_REVISION "${DOLPHIN_WC_DESCRIBE} (no further info)")
  set(DOLPHIN_WC_BRANCH "master")
endif()

# Architecture detection and arch specific settings
message(STATUS "Detected architecture: ${CMAKE_SYSTEM_PROCESSOR}")

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

if(ENABLE_GENERIC)
  message(STATUS "Warning! Building generic build!")
  set(_M_GENERIC 1)
  add_definitions(-D_M_GENERIC=1)
elseif(_ARCH_64 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
  set(_M_X86 1)
  set(_M_X86_64 1)
  add_definitions(-D_M_X86=1)
  add_definitions(-D_M_X86_64=1)
  check_and_add_flag(HAVE_SSE2 -msse2)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
  set(_M_ARM 1)
  set(_M_ARM_64 1)
  add_definitions(-D_M_ARM=1)
  add_definitions(-D_M_ARM_64=1)
  # CRC instruction set is used in the CRC32 hash function
  check_and_add_flag(HAVE_ARCH_ARMV8 -march=armv8-a+crc)
else()
  message(FATAL_ERROR "You're building on an unsupported platform: "
      "'${CMAKE_SYSTEM_PROCESSOR}' with ${CMAKE_SIZEOF_VOID_P}-byte pointers."
      " Enable generic build if you really want a JIT-less binary.")
endif()


# Enforce minimum GCC version
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
  message(FATAL_ERROR "Dolphin requires at least GCC 7.0 (found ${CMAKE_CXX_COMPILER_VERSION})")
endif()

if(CMAKE_GENERATOR MATCHES "Ninja")
  check_and_add_flag(DIAGNOSTICS_COLOR -fdiagnostics-color)
elseif(CMAKE_GENERATOR MATCHES "Visual Studio")
  # Only MSBuild needs this, other generators will compile one file at a time
  add_compile_options("/MP")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  check_and_add_flag(EXCEPTIONS /EHsc)
  dolphin_compile_definitions(_DEBUG DEBUG_ONLY)

  # Enforce C++ standard conforming conversion rules to catch possible bugs
  add_compile_options(/permissive-)
  # Remove unreferenced inline functions/data to reduce link time and catch bugs
  add_compile_options(/Zc:inline)
  # Assume `new` (w/o std::nothrow) throws to reduce binary size
  add_compile_options(/Zc:throwingNew)
  # Enforce strict volatile semantics as per ISO C++
  add_compile_options(/volatile:iso)

  string(APPEND CMAKE_EXE_LINKER_FLAGS " /NXCOMPAT")
else()
  add_definitions(-D_DEFAULT_SOURCE)
  check_and_add_flag(HAVE_WALL -Wall)
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

  # gcc uses some optimizations which might break stuff without this flag
  check_and_add_flag(NO_STRICT_ALIASING -fno-strict-aliasing)
  check_and_add_flag(NO_EXCEPTIONS -fno-exceptions)

  check_and_add_flag(VISIBILITY_INLINES_HIDDEN -fvisibility-inlines-hidden)
  check_and_add_flag(VISIBILITY_HIDDEN -fvisibility=hidden)

  check_and_add_flag(FOMIT_FRAME_POINTER -fomit-frame-pointer RELEASE_ONLY)

  dolphin_compile_definitions(_DEBUG DEBUG_ONLY)
  check_and_add_flag(GGDB -ggdb DEBUG_ONLY)

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # GNU ar: Create thin archive files.
    # Requires binutils-2.19 or later.
    set(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> qcTP <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_APPEND   "<CMAKE_AR> qTP  <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcTP <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_APPEND "<CMAKE_AR> qTP  <TARGET> <LINK_FLAGS> <OBJECTS>")
  endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  if(NOT OSX_USE_DEFAULT_SEARCH_PATH)
    # Hack up the path to prioritize the path to built-in OS libraries to
    # increase the chance of not depending on a bunch of copies of them
    # installed by MacPorts, Fink, Homebrew, etc, and ending up copying
    # them into the bundle.  Since we optionally depend on libraries which
    # are not part of OS X (ffmpeg, etc.), however, don't remove the default
    # path entirely as was done in a previous version of this file.  This is
    # still kinda evil, since it defeats the user's path settings...
    # See http://www.cmake.org/cmake/help/v3.0/command/find_program.html
    list(APPEND CMAKE_PREFIX_PATH "/usr")
  endif()

  # Specify target CPUs.
  check_and_add_flag(HAVE_MSSSE3 -mssse3)
  check_and_add_flag(HAVE_ARCH_CORE2 -march=core2)

  # Linker flags.
  # Drop unreachable code and data.
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dead_strip,-dead_strip_dylibs")

  find_library(APPKIT_LIBRARY AppKit)
  find_library(APPSERV_LIBRARY ApplicationServices)
  find_library(CARBON_LIBRARY Carbon)
  find_library(COCOA_LIBRARY Cocoa)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  find_library(CORESERV_LIBRARY CoreServices)
  find_library(FORCEFEEDBACK_LIBRARY ForceFeedback)
  find_library(FOUNDATION_LIBRARY Foundation)
  find_library(IOB_LIBRARY IOBluetooth)
  find_library(IOK_LIBRARY IOKit)
  find_library(OPENGL_LIBRARY OpenGL)
endif()

if(ENABLE_LTO)
  check_and_add_flag(LTO -flto)
  if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    set(CMAKE_AR gcc-ar)
    set(CMAKE_RANLIB gcc-ranlib)
  endif()
endif()

if(UNIX AND LINUX_LOCAL_DEV)
  add_definitions(-DLINUX_LOCAL_DEV)
endif()

# BSDs put packages in /usr/local instead of /usr, so we need to
# force CMake to look in those directories by default, too.
# All commands and submodule commands also need to see these
# changes, so just setting them in the project scope via
# include_directories and link_directories is not sufficient
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD|NetBSD")
  set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};/usr/local")
  set(CMAKE_REQUIRED_INCLUDES "/usr/local/include")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
endif()

# Dolphin requires threads.
find_package(Threads)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Build type (Release/Debug/RelWithDebInfo/MinSizeRel)" FORCE)
endif()

if(ENABLE_GPROF)
  check_and_add_flag(HAVE_PG -pg)
  if(NOT FLAG_C_HAVE_PG)
    message(FATAL_ERROR "Compiler option -pg is not supported")
  endif()
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
endif()

if(FASTLOG)
  add_definitions(-DDEBUGFAST)
endif()

if(GDBSTUB)
  add_definitions(-DUSE_GDBSTUB)
endif()

if(ENABLE_VTUNE)
  set(VTUNE_DIR "/opt/intel/vtune_amplifier")
  add_definitions(-DUSE_VTUNE)
  include_directories("${VTUNE_DIR}/include")
  set(VTUNE_LIBRARIES
      "${VTUNE_DIR}/lib64/libjitprofiling.a"
      "${VTUNE_DIR}/lib64/libittnotify.a"
  )
endif()

if(WIN32)
  message(STATUS "Building for Windows, disabling NoGUI frontend.")
  set(ENABLE_NOGUI OFF)
endif()

if(ANDROID)
  message(STATUS "Building for Android")
  if(NOT ENABLE_HEADLESS)
    add_definitions(-DANDROID)
    if(ENABLE_NOGUI)
      message(STATUS "Building Android app, disabling NoGUI frontend.")
      set(ENABLE_NOGUI 0)
    endif()
  else()
    # Lie to cmake a bit. We are cross compiling to Android
    # but not as a shared library. We want an executable.
    set(ANDROID 0)
  endif()
  set(USE_UPNP 0)
  set(ENABLE_QT 0)
  set(USE_DISCORD_PRESENCE 0)

  # We are cross compiling, search only the toolchain for libraries and includes
  SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
  SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()

if(ENABLE_HEADLESS)
  message(STATUS "Enabling Headless! Disabling GUI.")
  set(ENABLE_QT 0)
  set(USE_DISCORD_PRESENCE 0)
endif()

# Set file offset size to 64 bits.
#
# On modern Unixes, this is typically already the case. The lone exception is
# glibc, which may default to 32 bits. glibc allows this to be configured
# by setting _FILE_OFFSET_BITS.
if(NOT CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-D_FILE_OFFSET_BITS=64)
  add_definitions(-D_LARGEFILE_SOURCE)
endif()

########################################
# Dependency checking
#
# TODO: We should have options for dependencies included in the externals to
# override autodetection of system libraries and force the usage of the
# externals.
include(CheckLib)
include(CheckCXXSourceRuns)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL)
if (OPENGL_GL)
  include_directories(${OPENGL_INCLUDE_DIR})
endif()

if(ENABLE_X11)
  find_package(X11)
  if(X11_FOUND)
    add_definitions(-DHAVE_X11=1)
    check_lib(XRANDR xrandr Xrandr)
    if(XRANDR_FOUND)
      add_definitions(-DHAVE_XRANDR=1)
    else()
      add_definitions(-DHAVE_XRANDR=0)
    endif()
    pkg_check_modules(X11_INPUT REQUIRED xi>=1.5.0)
    message(STATUS "X11 support enabled")
  else()
    message(WARNING "X11 support enabled but not found. This build will not support X11.")
  endif()
endif()

if(ANDROID)
  set(EGL_FOUND 1)
  set(EGL_LIBRARIES EGL)
  add_definitions(-DHAVE_EGL=1)
  message(STATUS "EGL OpenGL interface enabled")
elseif(ENABLE_EGL)
  find_package(EGL)
  if(EGL_FOUND)
    add_definitions(-DHAVE_EGL=1)
    message(STATUS "EGL OpenGL interface enabled")
  else()
    message(WARNING "EGL support enabled but not found. This build will not support EGL.")
  endif()
endif()

if(ENCODE_FRAMEDUMPS)
  if(WIN32)
    set(FFMPEG_DIR Externals/ffmpeg)
  endif()
  find_package(FFmpeg COMPONENTS avcodec avformat avutil swscale)
  if(FFmpeg_FOUND)
    message(STATUS "libav/ffmpeg found, enabling AVI frame dumps")
    add_definitions(-DHAVE_FFMPEG)
  else()
    message(STATUS "libav/ffmpeg not found, disabling AVI frame dumps")
  endif()
endif()

if(OPROFILING)
  find_package(OProfile)
  if(OPROFILE_FOUND)
    message(STATUS "OProfile found, enabling profiling support")
    add_definitions(-DUSE_OPROFILE=1)
  else()
    message(FATAL_ERROR "OProfile not found. Can't build profiling support.")
  endif()
endif()

if(ENABLE_EVDEV)
  find_package(Libudev REQUIRED)
  find_package(Libevdev REQUIRED)
  if(LIBUDEV_FOUND AND LIBEVDEV_FOUND)
    message(STATUS "libevdev/libudev found, enabling evdev controller backend")
    add_definitions(-DHAVE_LIBUDEV=1)
    add_definitions(-DHAVE_LIBEVDEV=1)
  else()
    message(FATAL_ERROR "Couldn't find libevdev and/or libudev. Can't build evdev controller backend.\nDisable ENABLE_EVDEV if you wish to build without controller support")
  endif()
endif()

if(UNIX)
  message(STATUS "Using named pipes as controller inputs")
  add_definitions(-DUSE_PIPES=1)
  message(STATUS "Watching game memory for changes")
  add_definitions(-DUSE_MEMORYWATCHER=1)
endif()

if(ENABLE_ANALYTICS)
  message(STATUS "Enabling analytics collection (subject to end-user opt-in)")
  add_definitions(-DUSE_ANALYTICS=1)
endif()

########################################
# Setup include directories (and make sure they are preferred over the Externals)
#
include_directories(Source/Core)
if(ANDROID)
  include_directories(Source/Android)
endif()

########################################
# Process externals and setup their include directories
#
# NOTES about adding Externals:
#   - If an external provides a target, or one can be introduced with find_package, consider using it.
#     - If a target doesn't exist, consider introducing a target for it with add_library and adding all necessary
#       includes, definitions, etc, to that target. This way, only libraries that need those attributes simply
#       need to link that target in, as opposed to them being provided to every library
#       (which is the case with the directory-based include_directories, add_definitions, etc)
#
#   - make sure to tell cmake to link them statically or dynamically (most
#     should be linked statically)
#   - place the CMakeLists.txt in the first-level subdirectory, e.g.
#     Externals/zlib/CMakeLists.txt (that is: NOT in some Src/ subdirectory)

if (_M_X86)
  add_subdirectory(Externals/Bochs_disasm)
endif()

if(NOT ANDROID)
  add_subdirectory(Externals/cpp-optparse)
  add_subdirectory(Externals/minizip)
  include_directories(External/minizip)
endif()
add_subdirectory(Externals/fmt EXCLUDE_FROM_ALL)
add_subdirectory(Externals/glslang)

if(USE_SHARED_ENET)
  check_lib(ENET libenet enet enet/enet.h QUIET)
  include(CheckSymbolExists)
  if (ENET_FOUND)
    set(CMAKE_REQUIRED_INCLUDES ${ENET_INCLUDE_DIRS})
    # hack: LDFLAGS already contains -lenet but all flags but the first are
    # dropped; ugh, cmake
    set(CMAKE_REQUIRED_FLAGS ${ENET_LDFLAGS})
    set(CMAKE_REQUIRED_LIBRARIES ${ENET_LIBRARIES})
    check_symbol_exists(enet_socket_get_address enet/enet.h ENET_HAVE_SGA)
    set(CMAKE_REQUIRED_INCLUDES)
    set(CMAKE_REQUIRED_FLAGS)
    set(CMAKE_REQUIRED_LIBRARIES)
    if (NOT ENET_HAVE_SGA)
      # enet is too old
     set(ENET_FOUND FALSE)
    endif()
  endif()
endif()
if (ENET_FOUND)
  message(STATUS "Using shared enet")
else()
  message(STATUS "Using static enet from Externals")
  include_directories(Externals/enet/include)
  add_subdirectory(Externals/enet)
endif()

if(NOT XXHASH_FOUND)
  message(STATUS "Using static xxhash from Externals")
  add_subdirectory(Externals/xxhash)
endif()

find_package(ZLIB)
if(ZLIB_FOUND)
  message(STATUS "Using shared zlib")
else()
  message(STATUS "Shared zlib not found, falling back to the static library")
  add_subdirectory(Externals/zlib)
endif()

if(NOT APPLE)
  check_lib(LZO "(no .pc for lzo2)" lzo2 lzo/lzo1x.h QUIET)
endif()
if(LZO_FOUND)
  message(STATUS "Using shared lzo")
else()
  message(STATUS "Using static lzo from Externals")
  add_subdirectory(Externals/LZO)
  set(LZO lzo2)
endif()

if(NOT APPLE)
  check_lib(PNG libpng png png.h QUIET)
endif()
if (PNG_FOUND)
  message(STATUS "Using shared libpng")
else()
  message(STATUS "Using static libpng from Externals")
  add_subdirectory(Externals/libpng)
  set(PNG png)
endif()

# Using static FreeSurround from Externals
# There is no system FreeSurround library.
message(STATUS "Using static FreeSurround from Externals")
add_subdirectory(Externals/FreeSurround)

if (APPLE OR WIN32)
  message(STATUS "Using ed25519 from Externals")
  add_subdirectory(Externals/ed25519)
endif()

# Using static soundtouch from Externals
# Unable to use system soundtouch library: We require shorts, not floats.
add_subdirectory(Externals/soundtouch)
include_directories(Externals)

find_package(Cubeb)
if(CUBEB_FOUND)
  message(STATUS "Using the system cubeb")
else()
  message(STATUS "Using static cubeb from Externals")
  add_subdirectory(Externals/cubeb EXCLUDE_FROM_ALL)
endif()

if(NOT ANDROID)
  add_definitions(-D__LIBUSB__)
  if(NOT APPLE)
    find_package(LibUSB)
  endif()
  if(LIBUSB_FOUND AND NOT APPLE)
    message(STATUS "Using shared LibUSB")
    include_directories(${LIBUSB_INCLUDE_DIR})
  else()
    message(STATUS "Using static LibUSB from Externals")
    add_subdirectory(Externals/libusb)
    set(LIBUSB_LIBRARIES usb)
  endif()
  set(LIBUSB_FOUND true)
endif()

set(SFML_REQD_VERSION 2.1)
if(NOT APPLE)
  find_package(SFML ${SFML_REQD_VERSION} COMPONENTS network system)
endif()
if(SFML_FOUND)
  message(STATUS "Using shared SFML")
else()
  message(STATUS "Using static SFML ${SFML_REQD_VERSION} from Externals")
  add_definitions(-DSFML_STATIC)
  add_subdirectory(Externals/SFML)
  include_directories(BEFORE Externals/SFML/include)
endif()

if(USE_UPNP)
  if(NOT APPLE)
    find_package(Miniupnpc)
  endif()
  if(MINIUPNPC_FOUND AND MINIUPNPC_API_VERSION GREATER 8)
    message(STATUS "Using shared miniupnpc")
  else()
    message(STATUS "Using static miniupnpc from Externals")
    add_subdirectory(Externals/miniupnpc)
  endif()
  add_definitions(-DUSE_UPNP)
endif()

if(NOT APPLE)
  find_package(MbedTLS)
endif()
if(MBEDTLS_FOUND)
  message(STATUS "Using shared mbed TLS")
  include_directories(${MBEDTLS_INCLUDE_DIRS})
else()
  message(STATUS "Using static mbed TLS from Externals")
  set(MBEDTLS_LIBRARIES mbedtls mbedcrypto mbedx509)
  add_subdirectory(Externals/mbedtls/ EXCLUDE_FROM_ALL)
  include_directories(Externals/mbedtls/include)
endif()

if (NOT ANDROID)
#curl
find_package(CURL)
if(CURL_FOUND)
  message(STATUS "Using shared libcurl")
  include_directories(${CURL_INCLUDE_DIRS})
else()
  message(STATUS "Using static libcurl from Externals")
  add_subdirectory(Externals/curl)
  set(CURL_LIBRARIES curl)
  include_directories(BEFORE Externals/curl/include)
endif()

# pugixml
find_package(pugixml)
if(NOT pugixml_FOUND)
  message(STATUS "Using static pugixml from Externals")
  add_subdirectory(Externals/pugixml)
endif()
endif()

# iconv
find_library(ICONV_LIBRARIES NAMES iconv libiconv libiconv-2 c)
find_path(ICONV_INCLUDE_DIR NAMES iconv.h)

if (NOT ANDROID AND ICONV_LIBRARIES AND ICONV_INCLUDE_DIR)
  mark_as_advanced(ICONV_INCLUDE_DIR ICONV_LIBRARIES)
else()
  message(STATUS "Using static iconv from Externals")
  include_directories(Externals/libiconv-1.14/include)
  add_subdirectory(Externals/libiconv-1.14)
  set(ICONV_LIBRARIES iconv)
endif()

if(NOT ANDROID)
  find_package(HIDAPI)
  if(NOT HIDAPI_FOUND)
    message(STATUS "Using static HIDAPI from Externals")
    add_subdirectory(Externals/hidapi EXCLUDE_FROM_ALL)
  endif()
endif()

if(USE_DISCORD_PRESENCE)
  message(STATUS "Using static DiscordRPC from Externals")
  add_subdirectory(Externals/discord-rpc)
endif()

find_package(Libsystemd)
if(SYSTEMD_FOUND)
  message(STATUS "libsystemd found, enabling traversal server watchdog support")
  add_definitions(-DHAVE_LIBSYSTEMD)
else()
  message(STATUS "libsystemd not found, disabling traversal server watchdog support")
endif()

########################################
# Pre-build events: Define configuration variables and write SCM info header
#
if(DOLPHIN_WC_BRANCH STREQUAL "master" OR DOLPHIN_WC_BRANCH STREQUAL "stable")
  set(DOLPHIN_WC_IS_STABLE "1")
else()
  set(DOLPHIN_WC_IS_STABLE "0")
endif()

configure_file(
  "${PROJECT_SOURCE_DIR}/Source/Core/Common/scmrev.h.in"
  "${PROJECT_BINARY_DIR}/Source/Core/Common/scmrev.h"
)
include_directories("${PROJECT_BINARY_DIR}/Source/Core")

########################################
# Unit testing.
#
if(NOT ANDROID AND ENABLE_TESTS)
  message(STATUS "Using static gtest from Externals")
  add_subdirectory(Externals/gtest EXCLUDE_FROM_ALL)
else()
  message(STATUS "Unit tests are disabled")
endif()

########################################
# Process Dolphin source now that all setup is complete
#
add_subdirectory(Source)

########################################
# Install shared data files
#
if(NOT CMAKE_SYSTEM_NAME MATCHES "Darwin|Windows")
  install(DIRECTORY Data/Sys/ DESTINATION ${datadir}/sys PATTERN)
endif()
if(NOT CMAKE_SYSTEM_NAME MATCHES "Linux|FreeBSD|OpenBSD|Darwin")
  install(FILES Data/license.txt DESTINATION ${datadir})
endif()
if(CMAKE_SYSTEM_NAME MATCHES "Linux|FreeBSD|OpenBSD")
  # Install the application icon and menu item
  install(FILES Data/dolphin-emu.svg
          DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps)
  install(FILES Data/dolphin-emu.png
          DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/256x256/apps)
  install(FILES Data/dolphin-emu.desktop
          DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
  # Install manpages
  install(FILES Data/dolphin-emu.6
          DESTINATION ${mandir}/man6)
  install(FILES Data/dolphin-emu-nogui.6
          DESTINATION ${mandir}/man6)
endif()

# packaging information
set(CPACK_PACKAGE_NAME "dolphin-emu")
set(CPACK_PACKAGE_VENDOR "Dolphin Team")
set(CPACK_PACKAGE_VERSION_MAJOR ${DOLPHIN_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${DOLPHIN_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${DOLPHIN_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_FILE ${PROJECT_SOURCE_DIR}/Data/cpack_package_description.txt)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A GameCube and Wii emulator")

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
