########################################
# General setup
#
cmake_minimum_required (VERSION 2.6)
project (dolphin-emu)

set(DOLPHIN_IS_STABLE FALSE)
set(DOLPHIN_BIN_DIR		${CMAKE_CURRENT_SOURCE_DIR}/Binary/${CMAKE_HOST_SYSTEM_NAME})
set(DOLPHIN_PLUGINS_DIR	${DOLPHIN_BIN_DIR}/plugins)
set(DOLPHIN_USER_DIR	${DOLPHIN_BIN_DIR})
set(DOLPHIN_SYS_DIR		${DOLPHIN_BIN_DIR})
set(DOLPHIN_LICENSE_DIR	${DOLPHIN_BIN_DIR})

include(FindSubversion OPTIONAL) # for revision info
if(Subversion_FOUND)
	Subversion_WC_INFO(${PROJECT_SOURCE_DIR} DOLPHIN) # defines DOLPHIN_WC_REVISION
endif()

include(FindPkgConfig REQUIRED) # TODO: Make this optional or even implement our own package detection

# setup paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${DOLPHIN_BIN_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${DOLPHIN_PLUGINS_DIR})

# Various compile flags - TODO: Can these be simplified with a more general CMake variable?
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g")

# gcc uses some optimizations which might break stuff without this flag
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")

add_definitions(-DFILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE)

if(UNIX)
    # UNIX needs -fPIC for shared libraries, TODO: Would -fpic be enough as well?
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif(UNIX)


########################################
# Dependency checking
#
# TODO: Use find_library for other stuff?
#
# NOTES:
# there are numerous possible cases:
# - dependency may be required or optional
# - dependency may be already installed (but optionally the bundled one may be used)
# 
# TODO: We should have a number of options for optional dependencies (disable, force bundled, bundled or native, force native)
# e.g. the OpenGL plugin defaults to force native, so we error out if no GL libs are found. The user is free to explicitely disable it though.
# Stuff which is likely to be needed by users is made an option with default ON, other stuff (like e.g. sound backends) is made completely optionally.

# TODO: wxWidgets: Since _DEBUG gets defined in Debug builds, wxWidgets will enable some debugging functionality as well.
#       However, because we still link against release wx libs, which makes compilation fail because of undefined references.
#       When building the Debug configuration, we should probably check if the debug wx libs are available and fall back to the bundled ones otherwise.

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


########################################
# Setup include directories (and make sure they are preferred over the Externals)
#
include_directories(Source/PluginSpecs)
include_directories(Externals)
include_directories(.)  # config.h, TODO: Move to Source? Or just add the corresponding definitions to the compiler flags?
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
#	- add the include directory here
#	- make sure to tell cmake to link them statically or dynamically (most should be linked statically)
#	- ideally, the nested CMakeLists.txt should only contain a list of sources and an add_library()+target_link_libraries() call pair
#	- place the CMakeLists.txt in the first-level subdirectory, e.g. Externals/WiiUse/CMakeLists.txt (that is: NOT in some Src/ subdirectory)
#
add_subdirectory(Externals/Bochs_disasm)
include_directories(Externals/Bochs_disasm)

# TODO: Try using the native lib first
add_subdirectory(Externals/Lua)
include_directories(Externals/Lua)

# TODO: Try using the native lib first
add_subdirectory(Externals/LZO)
include_directories(Externals/LZO)

include(FindSDL OPTIONAL)
if(SDL_FOUND)
	include_directories(SDL_INCLUDE_DIR)
else(SDL_FOUND)
	# TODO: No CMakeLists.txt there, yet...
	add_subdirectory(Externals/SDL)
	include_directories(Externals/SDL/include)
endif(SDL_FOUND)

# TODO: Try using the native lib first
add_subdirectory(Externals/SFML)
include_directories(Externals/SFML/include)

# TODO: Try using the native lib first
add_subdirectory(Externals/SOIL)
include_directories(Externals/SOIL) # TODO: Or Externals/SOIL/SOIL?

include(FindZLIB OPTIONAL) # TODO: Move to top
if(ZLIB_FOUND)
	include_directories(ZLIB_INCLUDE_DIRS)
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
file(WRITE ./Source/Core/Common/Src/svnrev.h "#define SVN_REV_STR \"" ${DOLPHIN_WC_REVISION} "-" ${CMAKE_BUILD_TYPE} "\"")
# TODO: Write config.h here or add the corresponding compile definitions using add_definitions


########################################
# Start compiling our code
#
add_subdirectory(Source)


########################################
# copy over the Data folder ... TODO: Don't copy .svn dirs!
#
file(COPY Data/User DESTINATION ${DOLPHIN_USER_DIR})
file(COPY Data/Sys DESTINATION ${DOLPHIN_SYS_DIR})
file(COPY Data/license.txt DESTINATION ${DOLPHIN_LICENSE_DIR})


########################################
# Install and CPack information
#
install(DIRECTORY Data/User DESTINATION share/dolphin-emu)
install(DIRECTORY Data/Sys DESTINATION share/dolphin-emu)
install(FILES Data/license.txt DESTINATION share/dolphin-emu)
# TODO: Move childrens's install commands here?

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
