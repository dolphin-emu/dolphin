########################################
# General setup
#
cmake_minimum_required(VERSION 3.13)

cmake_policy(SET CMP0079 NEW) # let target_link_libraries() link to a target defined in a different directory
cmake_policy(SET CMP0080 OLD) # allow using BundleUtilities at configure time

if (POLICY CMP0099)
  cmake_policy(SET CMP0099 NEW) # Propagate INTERFACE_LINK_OPTIONS from private dependencies, used by MacOS framework builds of SDL
endif()

# Weird chicken-and-egg problem: We can't check the compiler before the project() call, but we have to set the policies before it.
# So we do this in two steps: Set the policies if they exist, then error out afterwards if we end up being MSVC and they don't exist.
if (POLICY CMP0117)
  cmake_policy(SET CMP0091 NEW) # MSVC runtime library flags are selected by an abstraction.
  cmake_policy(SET CMP0092 NEW) # MSVC warning flags are not in CMAKE_{C,CXX}_FLAGS by default.
  cmake_policy(SET CMP0117 NEW) # MSVC RTTI flag will not be added by default.
endif()

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW) # MSVC debug information format flags are selected by an abstraction.
endif()

# Minimum OS X version.
# This is inserted into the Info.plist as well.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15.0" CACHE STRING "")

set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_SOURCE_DIR}/CMake/FlagsOverride.cmake")

project(dolphin-emu)

if (MSVC)
  if (POLICY CMP0117)
    # cmake is a weird language. You can't do if(not POLICY)
  else()
    message(FATAL_ERROR "Please update to CMake 3.20 or higher.")
  endif()

  set(CMAKE_C_STANDARD 99)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set(COMPILER ${CMAKE_CXX_COMPILER_ID})
if (COMPILER STREQUAL "GNU")
  set(COMPILER "GCC") # perfer printing GCC instead of GNU
endif()

# Enforce minimum compiler versions that support the c++20 features we use
set (GCC_min_version 11)
set (Clang_min_version 14)
set (AppleClang_min_version 14.0.3)
set (min_xcode_version "14.0") # corresponding xcode version for AppleClang_min_version
set (MSVC_min_version 14.32)
set (min_vs_version "2022 17.2.3") # corresponding Visual Studio version for MSVC_min_version

message(STATUS "Using ${COMPILER} ${CMAKE_CXX_COMPILER_VERSION}")

if ("-" STREQUAL "${${COMPILER}_min_version}-")
  message(WARNING "Unknown compiler ${COMPILER}, assuming it is new enough")
else()
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS  ${${COMPILER}_min_version})
    message(FATAL_ERROR "Requires GCC ${GCC_min_version}, Clang ${Clang_min_version},"
    " AppleClang ${AppleClang_min_version} (Xcode ${min_xcode_version}),"
    " or MSVC ${MSVC_min_version} (Visual Studio ${min_vs_version}) or higher")
  endif()
endif()

# Name of the Dolphin distributor. If you redistribute Dolphin builds (forks,
# unofficial builds) please consider identifying your distribution with a
# unique name here.
set(DISTRIBUTOR "None" CACHE STRING "Name of the distributor.")

set(DOLPHIN_DEFAULT_UPDATE_TRACK "" CACHE STRING "Name of the default update track. If empty, disables auto-update.")

if(UNIX AND NOT APPLE AND NOT ANDROID)
  option(ENABLE_X11 "Enables X11 Support" ON)
endif()
if(NOT WIN32 AND NOT APPLE AND NOT HAIKU)
  option(ENABLE_EGL "Enables EGL OpenGL Interface" ON)
endif()

if(NOT ANDROID)
  option(ENABLE_CLI_TOOL "Enable dolphin-tool, a CLI-based utility for functions such as managing disc images" ON)
endif()


set(USE_SYSTEM_LIBS "AUTO" CACHE STRING "Use system libraries instead of bundled libraries.  ON - Always use system and fail if unavailable, OFF - Always use bundled, AUTO - Use system if available, otherwise use bundled.  Default is AUTO")
if(APPROVED_VENDORED_DEPENDENCIES)
  message(WARNING "APPROVED_VENDORED_DEPENDENCIES is deprecated.  Please migrate to setting USE_SYSTEM_LIBS to ON and setting USE_SYSTEM_<dependency> to either AUTO or OFF to allow bundled libs.")
endif()
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
option(ENABLE_VULKAN "Enables vulkan video backend" ON)
option(USE_DISCORD_PRESENCE "Enables Discord Rich Presence, show the current game on Discord" ON)
option(USE_MGBA "Enables GBA controllers emulation using libmgba" ON)
option(ENABLE_AUTOUPDATE "Enables support for automatic updates" ON)
option(STEAM "Creates a build for Steam" OFF)
option(USE_RETRO_ACHIEVEMENTS "Enables integration with retroachievements.org" ON)

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
option(OPROFILING "Enable profiling" OFF)

# TODO: Add DSPSpy
option(DSPTOOL "Build dsptool" OFF)

# Enable SDL by default on operating systems that aren't Android.
if(NOT ANDROID)
  option(ENABLE_SDL "Enables SDL as a generic controller backend" ON)
else()
  option(ENABLE_SDL "Enables SDL as a generic controller backend" OFF)
endif()

if(APPLE)
  option(MACOS_USE_DEFAULT_SEARCH_PATH "Don't prioritize system library paths" OFF)
  option(SKIP_POSTPROCESS_BUNDLE "Skip postprocessing bundle for redistributability" OFF)
  # Enable adhoc code signing by default (otherwise makefile builds on ARM will not work)
  option(MACOS_CODE_SIGNING "Enable codesigning" ON)
  option(USE_BUNDLED_MOLTENVK "Build MoltenVK from Externals with Dolphin-specific patches" ON)
  set(MACOS_CODE_SIGNING_IDENTITY "-" CACHE STRING "The identity used for codesigning.")
  set(MACOS_CODE_SIGNING_IDENTITY_UPDATER "-" CACHE STRING "The identity used for codesigning, for the updater.")
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
  ${CMAKE_CURRENT_SOURCE_DIR}/CMake
)

# Support functions
include(CheckAndAddFlag)
include(CheckCCompilerFlag)
include(DolphinCompileDefinitions)
include(DolphinDisableWarningsMSVC)
include(DolphinLibraryTools)
include(GNUInstallDirs)
include(RemoveCompileFlag)

# Enable folders for IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set up paths
set(datadir ${CMAKE_INSTALL_FULL_DATADIR}/dolphin-emu CACHE PATH "datadir")
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

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    string(APPEND CMAKE_RUNTIME_OUTPUT_DIRECTORY /x64)
  elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    string(APPEND CMAKE_RUNTIME_OUTPUT_DIRECTORY /ARM64)
  endif()
endif()

# setup CCache
include(CCache)

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
  set(_M_X86_64 1)
  add_definitions(-D_M_X86_64=1)
  check_and_add_flag(HAVE_SSE2 -msse2)
elseif(_ARCH_64 AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  set(_M_ARM_64 1)
  add_definitions(-D_M_ARM_64=1)
  # CRC instruction set is used in the CRC32 hash function
  check_and_add_flag(HAVE_ARCH_ARMV8 -march=armv8-a+crc)
else()
  message(FATAL_ERROR "You're building on an unsupported platform: "
      "'${CMAKE_SYSTEM_PROCESSOR}' with ${CMAKE_SIZEOF_VOID_P}-byte pointers."
      " Enable generic build if you really want a JIT-less binary.")
endif()

if(CMAKE_GENERATOR MATCHES "Ninja")
  check_and_add_flag(DIAGNOSTICS_COLOR -fdiagnostics-color)
elseif(CMAKE_GENERATOR MATCHES "Visual Studio")
  # Only MSBuild needs this, other generators will compile one file at a time
  add_compile_options("/MP")
endif()

if(MSVC)
  check_and_add_flag(EXCEPTIONS /EHsc)
  dolphin_compile_definitions(_DEBUG DEBUG_ONLY)

  # Disable RTTI
  add_compile_options(/GR-)

  # Set warning level 4 (the highest)
  add_compile_options(/W4)

  # Treat all warnings as errors
  add_compile_options(/WX)

  # Disable some warnings
  add_compile_options(
    /wd4201 # nonstandard extension used : nameless struct/union
    /wd4127 # conditional expression is constant
    /wd4100 # 'identifier' : unreferenced formal parameter
    /wd4200 # InputCommon fix temp.
    /wd4244 # 'conversion' conversion from 'type1' to 'type2', possible loss of data
    /wd4121 # 'symbol' : alignment of a member was sensitive to packing
    /wd4324 # Padding was added at the end of a structure because you specified a __declspec(align) value.
    /wd4714 # function 'function' marked as __forceinline not inlined
    /wd4351 # new behavior: elements of array 'array' will be default initialized
    # TODO: Enable this warning once possible
    /wd4245 # conversion from 'type1' to 'type2', signed/unsigned mismatch
    # Currently jits use some annoying code patterns which makes this common
  )

  # Additional warnings
  add_compile_options(
    /w44263 # Non-virtual member function hides base class virtual function
    /w44265 # Class has virtual functions, but destructor is not virtual
    /w44946 # Reinterpret cast between related types
  )

  # All files are encoded as UTF-8
  add_compile_options(/utf-8)

  # Ignore warnings in external headers
  add_compile_options(/external:anglebrackets)
  add_compile_options(/external:W0)
  add_compile_options(/external:templates-)

  # Request deterministic builds
  add_compile_options(/experimental:deterministic)
  add_link_options(/experimental:deterministic)

  # Enable function-level linking
  add_compile_options(/Gy)
  # Generate intrinsic functions
  add_compile_options(/Oi)
  # Enable buffer security check on Debug, disable otherwise
  add_compile_options($<IF:$<CONFIG:Debug>,/GS,/GS->)
  # Remove unreferenced inline functions/data to reduce link time and catch bugs
  # Note: In msbuild build, this gets set by msbuild by default
  add_compile_options(/Zc:inline)
  # Fix various other non-conformant behaviors
  add_compile_options(/Zc:__cplusplus,enumTypes,externConstexpr,preprocessor,templateScope,throwingNew)
  # Enforce strict volatile semantics as per ISO C++
  add_compile_options(/volatile:iso)

  # Use 'precise' floating point model
  add_compile_options(/fp:precise)

  string(APPEND CMAKE_EXE_LINKER_FLAGS " /NXCOMPAT")
  # Generate debug data
  string(APPEND CMAKE_EXE_LINKER_FLAGS " /DEBUG")
  # Eliminate dead code and data
  string(APPEND CMAKE_EXE_LINKER_FLAGS " /OPT:REF /OPT:ICF")
else()
  add_definitions(-D_DEFAULT_SOURCE)

  # gcc uses some optimizations which might break stuff without this flag
  check_and_add_flag(NO_STRICT_ALIASING -fno-strict-aliasing)
  check_and_add_flag(NO_EXCEPTIONS -fno-exceptions)

  check_and_add_flag(VISIBILITY_INLINES_HIDDEN -fvisibility-inlines-hidden)
  check_and_add_flag(VISIBILITY_HIDDEN -fvisibility=hidden)

  check_and_add_flag(FOMIT_FRAME_POINTER -fomit-frame-pointer NO_DEBINFO_ONLY)

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
  if(NOT MACOS_USE_DEFAULT_SEARCH_PATH)
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

  # Prevents Xcode from overriding the -fno-strict-aliasing flag
  set(CMAKE_XCODE_ATTRIBUTE_GCC_STRICT_ALIASING NO)

  # Specify target CPUs.
  if(_ARCH_64 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    check_and_add_flag(HAVE_MSSSE3 -mssse3)
    check_and_add_flag(HAVE_ARCH_CORE2 -march=core2)
  endif()
  # Linker flags.
  # Drop unreachable code and data.
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dead_strip,-dead_strip_dylibs")

  # Set FMT_EXCEPTIONS = 0, for consistency with -fno-exceptions earlier.
  # If we set only -fno-exceptions, fmt fails to compile when included from
  # Objective-C++ because fmt tries try to use throw because __EXCEPTIONS is defined.
  add_definitions(-DFMT_EXCEPTIONS=0)

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
  if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/GL)
    string(APPEND CMAKE_EXE_LINKER_FLAGS " /LTCG")
  else()
    check_and_add_flag(LTO -flto)
    if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
      set(CMAKE_AR gcc-ar)
      set(CMAKE_RANLIB gcc-ranlib)
    endif()
  endif()
endif()

if(UNIX)
  if(LINUX_LOCAL_DEV OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND STEAM))
    add_definitions(-DLINUX_LOCAL_DEV)
  endif()
endif()

# BSDs put packages in /usr/local instead of /usr, so we need to
# force CMake to look in those directories by default, too.
# All commands and submodule commands also need to see these
# changes, so just setting them in the project scope via
# include_directories and link_directories is not sufficient
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};/usr/local")
  set(CMAKE_REQUIRED_INCLUDES "/usr/local/include")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")

  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14.0)
    # Workaround: the llvm libc++ and versions of clang eariler than 14 have a bug with consteval
    # so we define FMT_CONSTEVAL to blank to just disable consteval in fmt
    add_definitions(-DFMT_CONSTEVAL=)
  endif()
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

if(ENABLE_VTUNE)
  set(VTUNE_DIR "/opt/intel/vtune_amplifier")
  add_definitions(-DUSE_VTUNE)
  include_directories("${VTUNE_DIR}/include")
  set(VTUNE_LIBRARIES
      "${VTUNE_DIR}/lib64/libjitprofiling.a"
      "${VTUNE_DIR}/lib64/libittnotify.a"
  )
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

# Set file offset size to 64 bits. On modern Unixes, this is typically already the case. Exceptions:
#
# glibc may default to 32 bits. This can be configured by setting _FILE_OFFSET_BITS=64.
#
# bionic (Android) defaults to 32 bits for 32-bit ABIs. Here too we can use _FILE_OFFSET_BITS=64,
# but only on API 25 and up. Since we're currently supporting older API levels and 32-bit Android
# isn't a build configuration we officially support, let's leave this as it is for now.
# More details: https://android.googlesource.com/platform/bionic/+/master/docs/32-bit-abi.md
if(NOT ANDROID AND NOT CMAKE_SYSTEM_NAME MATCHES "Windows")
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

set(OpenGL_GL_PREFERENCE GLVND CACHE STRING "Linux-only: if GLVND, use the vendor-neutral GL libraries (default). If LEGACY, use the legacy ones (might be necessary to have optirun/primusrun work)")
set_property(CACHE OpenGL_GL_PREFERENCE PROPERTY STRINGS GLVND LEGACY)
find_package(OpenGL)
if (OPENGL_GL)
  include_directories(${OPENGL_INCLUDE_DIR})
endif()

if(ENABLE_X11)
  pkg_check_modules(X11 x11 IMPORTED_TARGET)
  if(X11_FOUND)
    add_definitions(-DHAVE_X11=1)
    pkg_check_modules(XRANDR xrandr IMPORTED_TARGET)
    if(XRANDR_FOUND)
      add_definitions(-DHAVE_XRANDR=1)
    endif()
    pkg_check_modules(X11_INPUT REQUIRED xi>=1.5.0 IMPORTED_TARGET)
    message(STATUS "X11 support enabled")
  else()
    message(WARNING "X11 support enabled but not found. This build will not support X11.")
  endif()
endif()

if(ENABLE_EGL)
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
    if(_M_X86_64)
      set(FFMPEG_DIR Externals/FFmpeg-bin/x64)
    elseif(_M_ARM_64)
      set(FFMPEG_DIR Externals/FFmpeg-bin/ARM64)
    endif()
  endif()
  find_package(FFmpeg COMPONENTS avcodec avformat avutil swresample swscale)
  if(FFmpeg_FOUND)
    if(APPLE)
      find_library(COREMEDIA_LIBRARY CoreMedia)
      find_library(VIDEOTOOLBOX_LIBRARY VideoToolbox)
      find_library(COREVIDEO_LIBRARY CoreVideo)
      find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
    endif()
    message(STATUS "libav/ffmpeg found, enabling AVI frame dumps")
    add_definitions(-DHAVE_FFMPEG)
    if(WIN32)
      # Our prebuilt binaries depend on Bcrypt
      set_property(TARGET FFmpeg::avutil APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES "Bcrypt.lib"
      )
    endif()
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
  find_package(LIBUDEV REQUIRED)
  find_package(LIBEVDEV REQUIRED)
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

if(ENABLE_SDL)
  dolphin_find_optional_system_library(SDL2 Externals/SDL 2.26.0)
endif()

if(ENABLE_ANALYTICS)
  message(STATUS "Enabling analytics collection (subject to end-user opt-in)")
  add_definitions(-DUSE_ANALYTICS=1)
endif()

if(ENABLE_AUTOUPDATE)
  message(STATUS "Enabling automatic update support")
  add_definitions(-DAUTOUPDATE=1)
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
#
if (_M_X86_64)
  add_subdirectory(Externals/Bochs_disasm)
endif()
add_subdirectory(Externals/cpp-optparse)

dolphin_find_optional_system_library_pkgconfig(FMT
  fmt>=10.1 fmt::fmt Externals/fmt
)

add_subdirectory(Externals/imgui)
add_subdirectory(Externals/implot)
add_subdirectory(Externals/glslang)
# SPIRV-Cross is used on Windows for GLSL to HLSL conversion for the Direct3D 11 and Direct3D 12
# video backends, and on Apple devices for the Metal video backend.
if(WIN32 OR APPLE)
  add_subdirectory(Externals/spirv_cross)
endif()
add_subdirectory(Externals/tinygltf)

if(ENABLE_VULKAN)
  add_definitions(-DHAS_VULKAN)

  if(APPLE AND USE_BUNDLED_MOLTENVK)
    add_subdirectory(Externals/MoltenVK)
  endif()


  if (ANDROID AND _M_ARM_64)
    add_subdirectory(Externals/libadrenotools)
  endif()
endif()

if(NOT WIN32 OR (NOT (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")))
  # OpenGL is available on all platforms except windows-arm64
  add_definitions(-DHAS_OPENGL)
endif()

dolphin_find_optional_system_library(pugixml Externals/pugixml)

dolphin_find_optional_system_library_pkgconfig(ENET libenet>=1.3.18 enet::enet Externals/enet)

dolphin_find_optional_system_library_pkgconfig(xxhash libxxhash>=0.8.2 xxhash::xxhash Externals/xxhash)

dolphin_find_optional_system_library(BZip2 Externals/bzip2)

dolphin_find_optional_system_library(LibLZMA Externals/liblzma)
# Imported target added in CMake 3.14
dolphin_make_imported_target_if_missing(LibLZMA::LibLZMA LIBLZMA)

dolphin_find_optional_system_library_pkgconfig(ZSTD libzstd>=1.4.0 zstd::zstd Externals/zstd)

dolphin_find_optional_system_library_pkgconfig(ZLIB zlib-ng ZLIB::ZLIB Externals/zlib-ng)

dolphin_find_optional_system_library_pkgconfig(MINIZIP
  "minizip-ng>=4.0.4;minizip>=4.0.4" minizip::minizip Externals/minizip-ng
)

dolphin_find_optional_system_library(LZO Externals/LZO)

dolphin_find_optional_system_library_pkgconfig(lz4 liblz4>=1.8 LZ4::LZ4 Externals/lz4)

dolphin_find_optional_system_library_pkgconfig(SPNG spng spng::spng Externals/libspng)

# Using static FreeSurround from Externals
# There is no system FreeSurround library.
message(STATUS "Using static FreeSurround from Externals")
add_subdirectory(Externals/FreeSurround)

if (APPLE OR WIN32)
  message(STATUS "Using ed25519 from Externals")
  add_subdirectory(Externals/ed25519)
  include_directories(Externals/ed25519)
endif()

# Using static soundtouch from Externals
# Unable to use system soundtouch library: We require shorts, not floats.
add_subdirectory(Externals/soundtouch)
include_directories(Externals/soundtouch)

dolphin_find_optional_system_library(CUBEB Externals/cubeb)

if(NOT ANDROID)
  dolphin_find_optional_system_library_pkgconfig(
    LibUSB libusb-1.0 LibUSB::LibUSB Externals/libusb
  )
  add_definitions(-D__LIBUSB__)
endif()

dolphin_find_optional_system_library(SFML Externals/SFML 2.1 COMPONENTS network system)

if(USE_UPNP)
  dolphin_find_optional_system_library(MINIUPNPC Externals/miniupnpc 1.6)
  add_definitions(-DUSE_UPNP)
endif()

dolphin_find_optional_system_library(MBEDTLS Externals/mbedtls 2.28)

dolphin_find_optional_system_library(CURL Externals/curl)

if(NOT ANDROID)
  dolphin_find_optional_system_library(Iconv Externals/libiconv-1.14)
else()
  message(STATUS "Using static iconv from Externals")
  add_subdirectory(Externals/libiconv-1.14 EXCLUDE_FROM_ALL)
endif()

if(NOT ANDROID)
  dolphin_find_optional_system_library(HIDAPI Externals/hidapi)
endif()

if(USE_DISCORD_PRESENCE)
  message(STATUS "Using static DiscordRPC from Externals")
  add_subdirectory(Externals/discord-rpc EXCLUDE_FROM_ALL)
  include_directories(Externals/discord-rpc/include)
endif()

if(NOT ENABLE_QT)
  set(USE_MGBA 0)
endif()
if(USE_MGBA)
  dolphin_find_optional_system_library(LIBMGBA Externals/mGBA)
endif()

find_package(SYSTEMD)
if(SYSTEMD_FOUND)
  message(STATUS "libsystemd found, enabling traversal server watchdog support")
  add_definitions(-DHAVE_LIBSYSTEMD)
else()
  message(STATUS "libsystemd not found, disabling traversal server watchdog support")
endif()

if(STEAM)
  add_definitions(-DSTEAM)
endif()

if (WIN32)
  include_directories(Externals/WIL/include)
  include_directories(Externals/OpenAL/include)
endif()

include_directories(Externals/picojson)

add_subdirectory(Externals/expr)

add_subdirectory(Externals/rangeset)

add_subdirectory(Externals/FatFs)

if (USE_RETRO_ACHIEVEMENTS)
	add_subdirectory(Externals/rcheevos)
endif()

########################################
# Pre-build events: Define configuration variables and write SCM info header
#

# Remove in-tree revision information generated by Visual Studio
# This is because the compiler will check in-tree first and use this, even if it is outdated
file(REMOVE "${PROJECT_SOURCE_DIR}/Source/Core/Common/scmrev.h")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Source/Core/Common)
if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/Source/Core/Common/scmrev.h)
	file(TOUCH ${CMAKE_CURRENT_BINARY_DIR}/Source/Core/Common/scmrev.h)
endif()

if(APPLE)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Source/Core/DolphinQt)
  if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/Source/Core/DolphinQt/Info.plist)
    file(TOUCH ${CMAKE_CURRENT_BINARY_DIR}/Source/Core/DolphinQt/Info.plist)
  endif()

  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Source/Core/MacUpdater)
  if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/Source/Core/MacUpdater/Info.plist)
    file(TOUCH ${CMAKE_CURRENT_BINARY_DIR}/Source/Core/MacUpdater/Info.plist)
  endif()
endif()

find_package(Git)
if(NOT GIT_FOUND)
	set(GIT_EXECUTABLE "")
endif()
add_custom_target(
	dolphin_scmrev
	${CMAKE_COMMAND} -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR} -DPROJECT_BINARY_DIR=${PROJECT_BINARY_DIR} -DDISTRIBUTOR=${DISTRIBUTOR} -DDOLPHIN_DEFAULT_UPDATE_TRACK=${DOLPHIN_DEFAULT_UPDATE_TRACK} -DGIT_FOUND=${GIT_FOUND} -DGIT_EXECUTABLE=${GIT_EXECUTABLE} -DDOLPHIN_WC_REVISION=${DOLPHIN_WC_REVISION} -DDOLPHIN_WC_DESCRIBE=${DOLPHIN_WC_DESCRIBE} -DDOLPHIN_WC_BRANCH=${DOLPHIN_WC_BRANCH} -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET} -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake/ScmRevGen.cmake
	BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/Source/Core/Common/scmrev.h" "${CMAKE_CURRENT_BINARY_DIR}/Source/Core/DolphinQt/Info.plist" "${CMAKE_CURRENT_BINARY_DIR}/Source/Core/MacUpdater/Info.plist"
	VERBATIM
)

# This is here so #include "Common/scmrev.h" finds the generated header.
include_directories("${PROJECT_BINARY_DIR}/Source/Core")

########################################
# Unit testing.
#
if(ENABLE_TESTS)
  dolphin_find_optional_system_library_pkgconfig(GTEST
    gtest gtest::gtest Externals/gtest
  )
  # dolphin_find_optional_system_library_pkgconfig() doesn't add an alias if it
  # uses the bundled libraries, so we add one ourselves.
  if (NOT TARGET gtest::gtest)
    add_library(gtest::gtest ALIAS gtest)
  endif()
  # Force gtest to link the C runtime dynamically on Windows in order to avoid
  # runtime mismatches.
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
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
          DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
  install(FILES Data/dolphin-emu-nogui.6
          DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
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

# Debian Specific. Install dpkg-dev for automatic deps generation
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SECTION "Games")

set(CPACK_SET_DESTDIR ON)
set(CPACK_SOURCE_GENERATOR "TGZ;TBZ2;ZIP")
set(CPACK_SOURCE_IGNORE_FILES  "\\\\.#;/#;.*~;\\\\.swp;/\\\\.git")
list(APPEND CPACK_SOURCE_IGNORE_FILES "${CMAKE_BINARY_DIR}")

# CPack must be included after the CPACK_* variables are set in order for those
# variables to take effect.
Include(CPack)
